#ifndef SIO_Client_INCLUDED
#define SIO_Client_INCLUDED

#include <memory>

#include "SIOClientImpl.h"

#include "Poco/JSON/Array.h"

using Poco::JSON::Array;

class SIOClient
{
private:
	std::shared_ptr<SIOClientImpl> _socket;
	std::unique_ptr<SIOEventRegistry> _registry;
	std::unique_ptr<Poco::NotificationCenter> _nCenter;
	std::unique_ptr<SIONotificationHandler> _sioHandler;

	std::string _uri;
	std::string _endpoint;

public:
	SIOClient(std::string uri, std::string endpoint);

	bool connect();
	void disconnect();
	void send(std::string s);
	void emit(std::string eventname, std::string args);
	void emit(std::string eventname, Poco::JSON::Object::Ptr args);
	std::string getUri();
	Poco::NotificationCenter* getNCenter();

	typedef void (SIOEventTarget::*callback)(const void*, Array::Ptr&);

	void on(const char* name, SIOEventTarget* target, callback c);

	void fireEvent(const char* name, Array::Ptr args);
};

#endif
