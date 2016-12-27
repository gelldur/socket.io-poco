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
	using Listener = std::function<void(const std::string& name, Poco::JSON::Array::Ptr&)>;
	SIOClient(const Poco::URI& uri);

	bool connect();
	void disconnect();
	void send(std::string s);
	void emit(std::string eventname, std::string args);
	void emit(const std::string& eventname, const std::vector<Poco::Dynamic::Var>& args);
	Poco::NotificationCenter* getNCenter();

	void on(const std::string& name, const Listener& listener);

	void fireEvent(const std::string& name, Array::Ptr args);

private:
	const Poco::URI _uri;

	std::shared_ptr<SIOClientImpl> _socket;
	std::unique_ptr<Poco::NotificationCenter> _nCenter;
	std::unique_ptr<SIONotificationHandler> _sioHandler;

	std::multimap<std::string, Listener> _listeners;
};

#endif
