#pragma once

#include <memory>

#include "Poco/Notification.h"
#include "SIOPacket.h"

using Poco::Notification;

class SIOClient;

//class SIOMessage: public Notification
//{
//public:
//	SIOMessage(SIOClient *client, SocketIOPacket msg) : _client(client), _msg(msg) {}
//	SocketIOPacket getMsg() const {return _msg;}
//	SIOClient *_client;
//private:
//	SocketIOPacket _msg;
//};

//class SIOJSONMessage: public Notification
//{
//public:
//	SIOJSONMessage(std::string msg) : _msg(msg) {}
//	std::string getMsg() const {return _msg;}
//private:
//	std::string _msg;
//};

class SIOEvent : public Notification
{
public:
	SIOEvent(SIOClient* client, std::unique_ptr<SocketIOPacket> data)
			: client(client)
			, data(std::move(data))
	{
	}

	SIOClient* client;
	std::unique_ptr<SocketIOPacket> data;
};
