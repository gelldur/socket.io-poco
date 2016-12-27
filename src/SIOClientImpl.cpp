#include "SIOClientImpl.h"

#include <cassert>

#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/StreamCopier.h"
#include "Poco/Format.h"
#include <iostream>
#include <sstream>
#include <limits>
#include <api/UrlBuilder.h>
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Timer.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/URI.h"

#include "SIONotifications.h"
#include "SIOClient.h"

using Poco::JSON::Parser;
using Poco::JSON::ParseHandler;
using Poco::Dynamic::Var;
using Poco::JSON::Array;
using Poco::JSON::Object;

using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPMessage;
using Poco::Net::NetException;
using Poco::Net::SocketAddress;
using Poco::StreamCopier;
using Poco::StringTokenizer;
using Poco::cat;
using Poco::UInt16;
using Poco::Timer;
using Poco::TimerCallback;
using Poco::Dynamic::Var;
using Poco::Net::WebSocket;
using Poco::URI;

SIOClientImpl::SIOClientImpl(URI uri)
		: SIOClientImpl(uri, Logger::get("SIOClientLog"))
{
}

SIOClientImpl::SIOClientImpl(URI uri, Logger& logger)
		: _logger(logger)
		, _uri(uri)
{
}

SIOClientImpl::~SIOClientImpl(void)
{
	_thread.join();
	disconnect("");

	if (_webSocket)
	{
		_webSocket->shutdown();
	}
}

bool SIOClientImpl::init()
{
	if (handshake())
	{
		if (openSocket())
		{
			return true;
		}
	}

	return false;
}

bool SIOClientImpl::handshake()
{
	if (_uri.getScheme() == "https")
	{
		return false;
	}
	else
	{
		_session = std::unique_ptr<HTTPClientSession>(new HTTPClientSession(_uri.getHost(), _uri.getPort()));
	}
	_session->setKeepAlive(true);
	Poco::URI uri{_uri};
	uri.setPath("");//I'm not sure why i must remove it here
	uri.addQueryParameter("EIO", "3");
	uri.addQueryParameter("transport", "websocket");

	HTTPRequest request(HTTPRequest::HTTP_GET, uri.toString(), HTTPMessage::HTTP_1_1);
	request.set("Accept", "*/*");
	request.setContentType("text/plain");

	HTTPResponse response;
	std::string temp;

	try
	{
		_logger.information("Send Handshake Post request...:");
		_session->sendRequest(request);

		_logger.information("Receive Handshake Post request...");
		std::istream& rs = _session->receiveResponse(response);
		StreamCopier::copyToString(rs, temp);
		if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
		{
			_logger.error("%d %s", (int) response.getStatus(), response.getReason());
			_logger.error("response: %s\n", temp);
			return false;
		}
	}
	catch (const Poco::Net::NetException& ex)
	{
		std::stringstream stream;
		request.write(stream);
		_logger.warning("Request:%s", stream.str());

		stream.str("");
		response.write(stream);
		_logger.warning("Response:%s", stream.str());

		_logger.warning("Exception when creating websocket:%s\ncode:%d\nwhat:%s", ex.displayText()
						, ex.code()
						, std::string{ex.what()});
		return false;
	}

	_logger.information("%d %s", (int) response.getStatus(), response.getReason());
	_logger.information("response: %s\n", temp);

	return true;
}

bool SIOClientImpl::openSocket()
{
	Poco::URI uri{_uri};
	uri.setScheme("");//I'm not sure why we must remove it
	uri.addQueryParameter("EIO", "3");
	uri.addQueryParameter("transport", "websocket");

	HTTPRequest request{HTTPRequest::HTTP_GET, uri.toString(), HTTPMessage::HTTP_1_1};
	HTTPResponse response;

	try
	{
		_webSocket = std::unique_ptr<WebSocket>(new WebSocket(*_session, request, response));
	}
	catch (const Poco::Net::NetException& ex)
	{
		std::stringstream stream;
		request.write(stream);
		_logger.warning("Request:%s", stream.str());

		stream.str("");
		response.write(stream);
		_logger.warning("Response:%s", stream.str());

		_logger.warning("Exception when creating websocket:%s\ncode:%d\nwhat:%s", ex.displayText()
						, ex.code()
						, std::string{ex.what()});

		_webSocket.reset();
	}

	if (_webSocket == nullptr)
	{
		_logger.error("Impossible to create websocket");
		return false;
	}

	_logger.information("Send ping");
	std::string s = "5";//That's a ping https://github.com/Automattic/engine.io-parser/blob/1b8e077b2218f4947a69f5ad18be2a512ed54e93/lib/index.js#L21
	_webSocket->sendFrame(s.data(), s.size());

	_logger.information("WebSocket Created and initialised");

	_connected = true;//FIXME on 1.0.x the server acknowledge the connection

	_heartbeatTimer = std::unique_ptr<Timer>(new Timer(_pingInterval.count(), _pingInterval.count()));
	TimerCallback<SIOClientImpl> heartbeat(*this, &SIOClientImpl::heartbeat);
	_heartbeatTimer->start(heartbeat);

	_thread.start(*this);

	return _connected;

}

SIOClientImpl* SIOClientImpl::connect(SIOClient* client, URI uri)
{
	SIOClientImpl* s = new SIOClientImpl(uri);
	s->_client = client;

	if (s && s->init())
	{
		return s;
	}

	return nullptr;
}

void SIOClientImpl::disconnect(const std::string& endpoint)
{
	std::string s;
	s = "41" + endpoint;
	_webSocket->sendFrame(s.data(), s.size());
	if (endpoint == "")
	{
		_logger.information("Disconnect");
		_heartbeatTimer->stop();
		_connected = false;
	}

	_webSocket->shutdown();
}

void SIOClientImpl::heartbeat(Poco::Timer& timer)
{
	_logger.information("heartbeat called");
	SocketIOPacket* packet = SocketIOPacket::createPacketWithType("heartbeat", SocketIOPacket::SocketIOVersion::V10x);
	send(packet);
}

void SIOClientImpl::run()
{
	monitor();
}

void SIOClientImpl::monitor()
{
	do
	{
		receive();
	} while (_connected);
}

void SIOClientImpl::send(const std::string& endpoint, const std::string& s)
{
	emit(endpoint, "message", s);
}

void SIOClientImpl::emit(const std::string& endpoint
		, const std::string& eventname
		, const std::vector<Poco::Dynamic::Var>& args)
{
	_logger.information("Emitting event \"%s\"", eventname);
	SocketIOPacket* packet = SocketIOPacket::createPacketWithType("event", SocketIOPacket::SocketIOVersion::V10x);
	packet->setEndpoint(endpoint);
	packet->setEvent(eventname);
	for (const auto& element : args)
	{
		packet->addData(element);
	}
	this->send(packet);
}

void SIOClientImpl::emit(const std::string& endpoint, const std::string& eventname, const std::string& args)
{
	_logger.information("Emitting event \"%s\"", eventname);
	SocketIOPacket* packet = SocketIOPacket::createPacketWithType("event", SocketIOPacket::SocketIOVersion::V10x);
	packet->setEndpoint(endpoint);
	packet->setEvent(eventname);
	packet->addData(args);
	this->send(packet);
}

void SIOClientImpl::send(SocketIOPacket* packet)
{
	std::string req = packet->toString();
	if (_connected)
	{
		_logger.information("-->SEND:%s", req);
		_webSocket->sendFrame(req.data(), req.size());
	}
	else
	{
		_logger.warning("Cant send the message (%s) because disconnected", req);
	}
}

enum PacketType : char
{
	OPEN = '0', CLOSE = '1', PING = '2', PONG = '3', MESSAGE = '4', UPGRADE = '5', NOOP = '6'
};

bool SIOClientImpl::receive()
{
	if (_buffer.size() < _webSocket->getReceiveBufferSize())
	{
		_buffer.resize(_webSocket->getReceiveBufferSize());
	}
	assert(_buffer.empty() == false);

	int flags = 0;
	int bytesReceived = _webSocket->receiveFrame(&_buffer[0], static_cast<int>(_buffer.size()), flags);
	_buffer.resize(bytesReceived);
	if (bytesReceived <= 0)
	{
		//Shutdown connection or closed
		_logger.information("Shutdown connection or closed by host");
		disconnect("");
		return false;
	}
	_logger.information("Bytes received: %d ", bytesReceived);

	const char control = _buffer.at(0);

	_logger.information("Buffer received:");
	_logger.information("%s", std::string{_buffer.begin(), _buffer.end()});
	_logger.information("Control code: %c", control);
	switch (control)
	{
		case PacketType::OPEN:
		{
			auto foundBegin = std::find(_buffer.begin(), _buffer.end(), '{');
			assert(foundBegin != _buffer.end());
			auto foundEnd = std::find(foundBegin, _buffer.end(), '}');
			assert(foundEnd != _buffer.end());
			++foundEnd;

			Parser parser;
			const auto& result = parser.parse(std::string{foundBegin, foundEnd});
			const auto& message = result.extract<Object::Ptr>();

			_pingInterval = std::chrono::milliseconds{message->get("pingInterval").convert<std::size_t>()};
			_pingTimeout = std::chrono::milliseconds{message->get("pingTimeout").convert<std::size_t>()};

			break;
		}
		case PacketType::CLOSE:
			_logger.information("Host want to close");
			disconnect("");
			break;
		case PacketType::PING:
			_logger.information("Ping received, send pong");
			_buffer.insert(_buffer.begin(), PacketType::PONG);
			_webSocket->sendFrame(&_buffer[0], _buffer.size());
			break;
		case PacketType::PONG:
		{
			_logger.information("Pong received");
			std::string data{_buffer.begin(), _buffer.end()};
			if (data.find("probe") != std::string::npos)
			{
				_logger.information("Request Update");
				sendFrame({1, PacketType::UPGRADE});
			}
			break;
		}
		case PacketType::MESSAGE:
		{
			const char control = _buffer.at(1);
			_logger.information("Message code: [%c]", control);
			switch (control)
			{
				case PacketType::OPEN:
					_logger.information("Socket Connected");
					_connected = true;
					break;
				case PacketType::CLOSE:
					_logger.information("Socket Disconnected");
					disconnect("");
					break;
				case PacketType::PING:
				{
					auto foundBegin = std::find(_buffer.begin(), _buffer.end(), '[');
					assert(foundBegin != _buffer.end());
					auto foundEnd = std::find(foundBegin, _buffer.end(), ']');
					assert(foundEnd != _buffer.end());
					++foundEnd;

					Parser parser;
					const auto& result = parser.parse(std::string{foundBegin, foundEnd});
					const auto& message = result.extract<Poco::JSON::Array::Ptr>();

					auto packetOut = std::unique_ptr<SocketIOPacket>(SocketIOPacket::createPacketWithType("event"
																										  , SocketIOPacket::SocketIOVersion::V10x));
					packetOut->setEvent(message->get(0).toString());
					for (int i = 1; i < message->size(); ++i)
					{
						if (message->isArray(i))
						{
							packetOut->addData(message->getArray(i));
						}
						else
						{
							packetOut->addData(message->get(i));
						}
					}
					_client->getNCenter()->postNotification(new SIOEvent(_client, std::move(packetOut)));
				}
					break;
				case PacketType::PONG:
					_logger.information("Message Ack");
					break;
				case PacketType::MESSAGE:
					_logger.information("Error");
					break;
				case PacketType::UPGRADE:
					_logger.information("Binary Event");
					break;
				case PacketType::NOOP:
					_logger.information("Binary Ack");
					break;
			}
		}
			break;
		case PacketType::UPGRADE:
			_logger.information("Upgrade required");
			break;
		case PacketType::NOOP:
			_logger.information("Noop");
			break;
	}

	return true;
}

void SIOClientImpl::sendFrame(const std::string& data)
{
	_webSocket->sendFrame(data.c_str(), data.size());
}
