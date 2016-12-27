#include "SIOClient.h"

#include "Poco/URI.h"

using Poco::URI;

SIOClient::SIOClient(std::string uri, std::string endpoint)
		: _registry{new SIOEventRegistry()}
		, _nCenter{new NotificationCenter{}}
		, _sioHandler{new SIONotificationHandler(_nCenter.get())}
		, _uri{uri}
		, _endpoint{endpoint}
{
}

bool SIOClient::connect()
{
	//check if connection to endpoint exists
	URI tmp_uri(_uri);

	if (_socket == nullptr)
	{
		_socket = std::shared_ptr<SIOClientImpl>(SIOClientImpl::connect(this,tmp_uri));

		if (_socket == nullptr)
		{
			return false;//connect failed
		}
	}

	if (tmp_uri.getPath() != "")
	{
		_socket->connectToEndpoint(tmp_uri.getPath());
	}

	return true;
}

void SIOClient::disconnect()
{
	_socket->disconnect(_endpoint);
}

std::string SIOClient::getUri()
{
	return _uri;
}

NotificationCenter* SIOClient::getNCenter()
{
	return _nCenter.get();
}

void SIOClient::on(const char* name, SIOEventTarget* target, callback c)
{
	_registry->registerEvent(name, target, c);
}

void SIOClient::fireEvent(const char* name, Array::Ptr args)
{
	_registry->fireEvent(this, name, args);
}

void SIOClient::send(std::string s)
{
	_socket->send(_endpoint, s);
}

void SIOClient::emit(std::string eventname, Poco::JSON::Object::Ptr args)
{
	_socket->emit(_endpoint, eventname, args);
}

void SIOClient::emit(std::string eventname, std::string args)
{
	_socket->emit(_endpoint, eventname, args);
}
