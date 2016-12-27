#ifndef SIO_ClientImpl_DEFINED
#define SIO_ClientImpl_DEFINED

#include <string>
#include <memory>
#include <chrono>

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
	using Listener = std::function<void(const std::string& name, const Poco::JSON::Array::Ptr&)>;

	SIOClientImpl(Poco::URI uri, const Listener& eventHandler, Logger& logger = Logger::get("SIOClientLog"));
	~SIOClientImpl(void);

	bool connect();
	bool handshake();
	bool openSocket();

	void disconnect(const std::string& endpoint);
	void monitor();
	virtual void run();
	void heartbeat(Poco::Timer& timer);
	bool receive();
	void send(const std::string& endpoint, const std::string& s);
	void send(SocketIOPacket* packet);
	void emit(const std::string& endpoint, const std::string& eventname, const std::string& args);
	void emit(const std::string& endpoint
			, const std::string& eventname
			, const std::vector<Poco::Dynamic::Var>& args);

private:
	const Poco::URI _uri;

	std::chrono::milliseconds _pingInterval = std::chrono::milliseconds{25000};
	std::chrono::milliseconds _pingTimeout = std::chrono::milliseconds{60000};

	Thread _thread;

	bool _connected = false;

	std::unique_ptr<HTTPClientSession> _session;
	std::unique_ptr<Timer> _heartbeatTimer;
	std::unique_ptr<Poco::Net::WebSocket> _webSocket;

	Listener _eventHandler;

	std::vector<char> _buffer;
	Logger& _logger;

	void sendFrame(const std::string& data);
};

#endif
