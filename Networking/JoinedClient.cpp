#include "JoinedClient.h"

void JoinedClient::sendChat(std::string message) const
{
	ENetPacket* ret = enet_packet_create(NULL, message.length() + 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)ChatMessageFromServer;
	ret->data[1] = (unsigned char)message.length();
	memcpy(ret->data + 2, message.c_str(), message.length());
	send(ret, OtherReliable);
}

ENetPacket* JoinedClient::makeCenterPrintPacket(std::string text, unsigned int durationMS, float red, float green, float blue)
{
	if (text.length() > 255)
		text = text.substr(0, 255);

	durationMS = std::min(durationMS, 60000u);

	unsigned int size = 1 + sizeof(unsigned int) + sizeof(float) * 3 + 1 + text.length();
	ENetPacket* ret = enet_packet_create(NULL, size, getFlagsFromChannel(OtherReliable));

	int byteIterator = 0;
	ret->data[byteIterator] = (unsigned char)CenterPrint;
	byteIterator += 1;

	memcpy(ret->data + byteIterator, &durationMS, sizeof(unsigned int));
	byteIterator += sizeof(unsigned int);

	memcpy(ret->data + byteIterator, &red, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(ret->data + byteIterator, &green, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(ret->data + byteIterator, &blue, sizeof(float));
	byteIterator += sizeof(float);

	ret->data[byteIterator] = (unsigned char)text.length();
	byteIterator += 1;

	memcpy(ret->data + byteIterator, text.c_str(), text.length());

	return ret;
}

void JoinedClient::sendCenterPrint(std::string text, unsigned int durationMS, float red, float green, float blue) const
{
	send(makeCenterPrintPacket(std::move(text), durationMS, red, green, blue), OtherReliable);
}

void JoinedClient::send(ENetPacket* packet, PacketChannel channel) const
{
	if (!peer)
		return;

	if (enet_peer_send(peer, channel, packet) < 0)
	{
		scope("JoinedClient::send");
		error("enet_peer_send failed");
	}
}

void JoinedClient::send(const char* data, unsigned int len, PacketChannel channel) const
{
	if (!peer)
		return;

	ENetPacket* packet = enet_packet_create(data, len, getFlagsFromChannel(channel));
	if (!packet)
	{
		scope("JoinedClient::send");
		error("enet_packet_create failed");
		return;
	}
	if(enet_peer_send(peer, channel, packet) < 0)
	{
		scope("JoinedClient::send");
		error("enet_peer_send failed");
	}
}

JoinedClient::JoinedClient(ENetEvent& event,unsigned int _netID)
	: netID(_netID)
{
	char host[256];
	enet_address_get_host(&event.peer->address, host, 256);
	ip = std::string(host);

	char numericHost[256];
	enet_address_get_host_ip(&event.peer->address, numericHost, 256);
	loopback = std::string(numericHost) == "127.0.0.1";

	info("Someone connected from " + ip);
	peer = event.peer;
	peer->data = this;
}

void JoinedClient::kick(KickReason reason)
{
	if (peer)
		enet_peer_disconnect_later(peer, reason);
	peer = nullptr;
}

JoinedClient::~JoinedClient()
{
	//Won't do anything if they disconnected already
	kick(KickReason::OtherReason);
}
