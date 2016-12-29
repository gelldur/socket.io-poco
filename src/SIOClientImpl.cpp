#include "SIOClientImpl.h"

#include <chrono>
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

SIOClientImpl::SIOClientImpl(URI uri, const Listener& eventHandler, Logger& logger)
		: _uri(uri)
		, _isConnected{false}
		, _eventHandler(eventHandler)
		, _logger(logger)
{
}

SIOClientImpl::~SIOClientImpl(void)
{
	disconnect();
}

bool SIOClientImpl::connect()
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
	_session = std::unique_ptr<HTTPClientSession>(new HTTPClientSession(_uri.getHost(), _uri.getPort()));
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
	if (_webSocket)
	{
		disconnect();
	}

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

	using namespace std::chrono;
	Poco::Timestamp timestamp;
	while (timestamp.elapsed() < duration_cast<microseconds>(_pingTimeout).count() &&
		   _isConnected == false)//wait for connect
	{
		if (receive() == false)//is blocking call
		{
			break;
		}
		_logger.information("WebSocket waiting for connection");
	}

	if (_isConnected == false)
	{
		_logger.warning("WebSocket can't connect, timeout or error");
		return false;
	}

	_logger.information("WebSocket Created and initialised");

	_heartbeatTimer = std::unique_ptr<Timer>(new Timer(_pingInterval.count(), _pingInterval.count()));
	TimerCallback<SIOClientImpl> heartbeat(*this, &SIOClientImpl::heartbeat);
	_heartbeatTimer->start(heartbeat);

	_thread.start(*this);

	return true;

}

void SIOClientImpl::disconnect()
{
	if (_isConnected == false)
	{
		return;
	}
	_logger.information("Disconnect");

	_isConnected = false;
	_heartbeatTimer->stop();

	_webSocket->shutdown();
	_thread.join();

	_webSocket.reset();//clear socket
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
	while (_isConnected)
	{
		receive();//is blocking call
	}
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
	send(packet);
}

void SIOClientImpl::emit(const std::string& endpoint, const std::string& eventname, const std::string& args)
{
	_logger.information("Emitting event \"%s\"", eventname);
	SocketIOPacket* packet = SocketIOPacket::createPacketWithType("event", SocketIOPacket::SocketIOVersion::V10x);
	packet->setEndpoint(endpoint);
	packet->setEvent(eventname);
	packet->addData(args);
	send(packet);
}

bool SIOClientImpl::receive()
{
	assert(_webSocket);
	if (_buffer.size() < _webSocket->getReceiveBufferSize())
	{
		_buffer.resize(_webSocket->getReceiveBufferSize());
	}
	assert(_buffer.empty() == false);

	int flags = 0;
	int bytesReceived = _webSocket->receiveFrame(&_buffer[0], static_cast<int>(_buffer.size()), flags);
	if (flags & WebSocket::FrameOpcodes::FRAME_OP_CLOSE)
	{
		_logger.information("Close frame");
		return true;
	}
	if (bytesReceived <= 0)
	{
		//Shutdown connection or closed
		_logger.information("Shutdown connection or closed by host");
		disconnect();
		return false;
	}
	_buffer.resize(bytesReceived);

	const char control = _buffer.at(0);

	_logger.information("Buffer received: %z bytes flags %d", _buffer.size(), flags);
	_logger.information("%s", std::string{_buffer.begin(), _buffer.end()});
	_logger.information("Control code: %c", control);
	switch (control)
	{
		case FrameType::OPEN:
		{
			auto foundBegin = std::find(_buffer.begin(), _buffer.end(), '{');
			assert(foundBegin != _buffer.end());
			auto foundEnd = std::find(_buffer.rbegin(), _buffer.rend(), '}');
			assert(foundEnd != _buffer.rend());

			onHandshake(std::string{foundBegin, foundEnd.base()});
			break;
		}
		case FrameType::CLOSE:
			_logger.information("Host want to close");
			disconnect();
			break;
		case FrameType::PING:
			_logger.information("Ping received, send pong");
			_buffer.insert(_buffer.begin(), FrameType::PONG);
			_webSocket->sendFrame(&_buffer[0], _buffer.size());
			break;
		case FrameType::PONG:
		{
			_logger.information("Pong received");
			std::string data{_buffer.begin(), _buffer.end()};
			if (data.find("probe") != std::string::npos)
			{
				_logger.information("Request Update");
				sendFrame(FrameType::UPGRADE);
			}
			break;
		}
		case FrameType::MESSAGE:
			onMessage(_buffer);
			break;
		case FrameType::UPGRADE:
			_logger.information("Upgrade required");
			break;
		case FrameType::NOOP:
			_logger.information("Noop");
			break;
	}

	return true;
}

void SIOClientImpl::send(SocketIOPacket* packet)
{
	std::string req = packet->toString();
	if (_isConnected)
	{
		_logger.information("-->SEND:%s", req);
		assert(_webSocket);
		_webSocket->sendFrame(req.data(), req.size());
	}
	else
	{
		_logger.warning("Cant send the message (%s) because disconnected", req);
	}
}

void SIOClientImpl::sendFrame(const std::string& data)
{
	if (_isConnected)
	{
		_logger.information("-->SEND:%s", data);
		assert(_webSocket);
		_webSocket->sendFrame(data.c_str(), data.size());
	}
	else
	{
		_logger.warning("Cant send the message (%s) because disconnected", data);
	}
}

void SIOClientImpl::sendFrame(const char data)
{
	if (_isConnected)
	{
		_logger.information("-->SEND:%c", data);
		assert(_webSocket);
		_webSocket->sendFrame(&data, 1);
	}
	else
	{
		_logger.warning("Cant send the message (%c) because disconnected", data);
	}
}

void SIOClientImpl::sendFrame(SIOClientImpl::FrameType frameType, SIOClientImpl::Type type)
{
	if (_isConnected)
	{
		_logger.information("-->SEND:%c%c", (char) frameType, (char) type);
		assert(_webSocket);
		char data[] = {frameType, type};
		_webSocket->sendFrame(data, sizeof(data));
	}
	else
	{
		_logger.warning("Cant send the message (%c%c) because disconnected", (char) frameType, (char) type);
	}
}

void SIOClientImpl::onHandshake(const std::string& data)
{
	assert(data.front() == '{');
	assert(data.back() == '}');
	Parser parser;
	const auto& result = parser.parse(data);
	const auto& message = result.extract<Object::Ptr>();

	_pingInterval = std::chrono::milliseconds{message->get("pingInterval").convert<std::size_t>()};
	_pingTimeout = std::chrono::milliseconds{message->get("pingTimeout").convert<std::size_t>()};
	_sid = message->get("sid").toString();
}

void SIOClientImpl::onMessage(const std::vector<char>& buffer)
{
	const char control = buffer.at(1);

	_logger.information("Message code: [%c]", control);
	switch (control)
	{
		case Type::CONNECT:
			_logger.information("Socket Connected");
			_isConnected = true;
			break;
		case Type::DISCONNECT:
			_logger.information("Socket Disconnected by server");
			disconnect();
			break;
		case Type::EVENT:
		{
			auto foundBegin = std::find(buffer.begin(), buffer.end(), '[');
			assert(foundBegin != buffer.end());
			auto foundEnd = std::find(buffer.rbegin(), buffer.rend(), ']');
			assert(foundEnd != buffer.rend());

			auto extracted = std::string{foundBegin, foundEnd.base()};
			assert(extracted.front() == '[');
			assert(extracted.back() == ']');

			Parser parser;
			auto result = parser.parse(extracted);
			auto message = result.extract<Poco::JSON::Array::Ptr>();

			auto eventName = message->get(0).toString();
			message->remove(0);
			if (_eventHandler)
			{
				_eventHandler(eventName, message);
			}
			break;
		}
		case Type::ACK:
			_logger.information("Message Ack");
			break;
		case Type::ERROR:
			_logger.information("Error");
			break;
		case Type::BINARY_EVENT:
			_logger.information("Binary Event");
			break;
		case Type::BINARY_ACK:
			_logger.information("Binary Ack");
			break;
	}
}

