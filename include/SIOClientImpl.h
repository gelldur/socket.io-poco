#ifndef SIO_ClientImpl_DEFINED
#define SIO_ClientImpl_DEFINED

#include <string>
#include <memory>

#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Logger.h"
#include "Poco/Timer.h"
#include "Poco/NotificationCenter.h"
#include "Poco/Thread.h"
#include "Poco/ThreadTarget.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/URI.h"

#include "Poco/JSON/Parser.h"

#include "SIONotificationHandler.h"
#include "SIOPacket.h"

using Poco::Net::HTTPClientSession;
using Poco::Logger;
using Poco::Timer;
using Poco::TimerCallback;
using Poco::NotificationCenter;
using Poco::Thread;
using Poco::ThreadTarget;

class SIOClientImpl : public Poco::Runnable
{
public:
	SIOClientImpl();
	SIOClientImpl(Poco::URI uri);
	~SIOClientImpl(void);

	bool handshake();
	bool openSocket();
	bool init();

	static SIOClientImpl* connect(SIOClient* client, Poco::URI uri);
	void disconnect(std::string endpoint);
	void connectToEndpoint(std::string endpoint);
	void monitor();
	virtual void run();
	void heartbeat(Poco::Timer& timer);
	bool receive();
	void send(std::string endpoint, std::string s);
	void send(SocketIOPacket* packet);
	void emit(std::string endpoint, std::string eventname, std::string args);
	void emit(std::string endpoint, std::string eventname, Poco::JSON::Object::Ptr args);

	std::string getUri();

private:
	SIOClient* _client = nullptr;
	std::string _sid;
	int _heartbeat_timeout;
	int _timeout;
	std::string _host;
	int _port;
	Poco::URI _uri;
	bool _connected;
	SocketIOPacket::SocketIOVersion _version;

	HTTPClientSession* _session;
	std::unique_ptr<Poco::Net::WebSocket> _ws;
	Timer* _heartbeatTimer;
	Logger* _logger;
	Thread _thread;

	char* _buffer;
	std::size_t _buffer_size;
};

#endif
