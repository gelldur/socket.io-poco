#include "SIOClient.h"

#include "Poco/URI.h"

using Poco::URI;

SIOClient::SIOClient(const Poco::URI& uri)
		: _uri{uri}
{
}

bool SIOClient::connect()
{
	//check if connection to endpoint exists
	if (_socket == nullptr)
	{
		_socket = std::unique_ptr<SIOClientImpl>(
				new SIOClientImpl(_uri, std::bind(&SIOClient::fireEvent, this, std::placeholders::_1
												  , std::placeholders::_2)));
	}

	return _socket->connect();
}

void SIOClient::disconnect()
{
	_socket->disconnect("");
}

void SIOClient::on(const std::string& name, const SIOClientImpl::Listener& listener)
{
	_listeners.insert(std::make_pair(name, listener));
}

void SIOClient::fireEvent(const std::string& name, const Poco::JSON::Array::Ptr& args)
{
	auto found = _listeners.equal_range(name);
	while (found.first != found.second)
	{
		found.first->second(name, args);
		++found.first;
	}
}

void SIOClient::send(std::string s)
{
	_socket->send("", s);
}

void SIOClient::emit(const std::string& eventname, const std::vector<Poco::Dynamic::Var>& args)
{
	_socket->emit("", eventname, args);
}

void SIOClient::emit(std::string eventname, std::string args)
{
	_socket->emit("", eventname, args);
}
