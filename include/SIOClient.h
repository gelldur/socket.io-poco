#ifndef SIO_Client_INCLUDED
#define SIO_Client_INCLUDED

#include <memory>
#include <functional>
#include <map>

#include <Poco/URI.h>
#include <Poco/JSON/Array.h>

#include "SIOClientImpl.h"

class SIOClient
{
public:
	SIOClient(const Poco::URI& uri);

	bool connect();
	void disconnect();
	void send(std::string s);
	void emit(std::string eventname, std::string args);
	void emit(const std::string& eventname, const std::vector<Poco::Dynamic::Var>& args);

	void on(const std::string& name, const SIOClientImpl::Listener& listener);
	void fireEvent(const std::string& name, const Poco::JSON::Array::Ptr& args);

	bool isConnected() const
	{
		return _socket && _socket->isConnected();
	}

private:
	const Poco::URI _uri;

	std::unique_ptr<SIOClientImpl> _socket;
	std::multimap<std::string, SIOClientImpl::Listener> _listeners;
};

#endif
