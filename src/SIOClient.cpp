#include "SIOClient.h"

#include "Poco/URI.h"

using Poco::URI;

SIOClient::SIOClient(const Poco::URI& uri)
		: _uri{uri}
		, _nCenter{new NotificationCenter{}}
		, _sioHandler{new SIONotificationHandler(_nCenter.get())}
{
}

bool SIOClient::connect()
{
	//check if connection to endpoint exists
	if (_socket == nullptr)
	{
		_socket = std::shared_ptr<SIOClientImpl>(SIOClientImpl::connect(this, _uri));

		if (_socket == nullptr)
		{
			return false;//connect failed
		}
	}

	return true;
}

void SIOClient::disconnect()
{
	_socket->disconnect("");
}

NotificationCenter* SIOClient::getNCenter()
{
	return _nCenter.get();
}

void SIOClient::on(const std::string& name, const SIOClient::Listener& listener)
{
	_listeners.insert(std::make_pair(name, listener));
}

void SIOClient::fireEvent(const std::string& name, Array::Ptr args)
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
