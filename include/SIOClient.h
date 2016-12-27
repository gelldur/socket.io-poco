#ifndef SIO_Client_INCLUDED
#define SIO_Client_INCLUDED

#include <memory>
#include <functional>
#include <map>

#include "SIOClientImpl.h"

#include "Poco/JSON/Array.h"

using Poco::JSON::Array;

class SIOClient
{
public:
	using Listener = std::function<void(const std::string& name, Array::Ptr&)>;
	SIOClient(std::string uri, std::string endpoint);

	bool connect();
	void disconnect();
	void send(std::string s);
	void emit(std::string eventname, std::string args);
	void emit(std::string eventname, Poco::JSON::Object::Ptr args);
	std::string getUri();
	Poco::NotificationCenter* getNCenter();

	void on(const std::string& name, const Listener& listener);

	void fireEvent(const std::string& name, Array::Ptr args);

private:
	std::shared_ptr<SIOClientImpl> _socket;
	std::unique_ptr<Poco::NotificationCenter> _nCenter;
	std::unique_ptr<SIONotificationHandler> _sioHandler;

	std::string _uri;
	std::string _endpoint;
	std::multimap<std::string, Listener> _listeners;
};

#endif
