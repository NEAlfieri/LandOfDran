#pragma once

#include "../LandOfDran.h"

/*
	These are held by the Server and represent a handle to a single person who joined the server for the duration of their play
	Don't confuse with Client which is something the client program holds a single instance of
*/
class JoinedClient
{
	//Unique ID specific to clients, incremented each time one joins, used to associate enet events with a JoinedClient
	netIDType netID = -1;

	ENetPeer* peer = nullptr;

	std::string ip = "";

	//Whether this connection came in over loopback, used to auto-admin the host's own client in single player.
	//Computed from the raw numeric address, not ip above, since that's resolved through DNS/hosts and can come
	//back as "localhost" instead of "127.0.0.1" depending on the OS
	bool loopback = false;

public:

	//A kicked client keeps its place in the server's list until ENet reports the disconnect, with no peer left to ask, like send
	float getPing() const { return peer ? peer->roundTripTime : 0.0f; }

	//Set this to ClientData I guess
	void* userData = nullptr;

	/*
		Roughly where this client is watching the world from, and how many bytes of object updates they can still be
		sent this tick. Both are refreshed once a tick by LoopServer::startUpdateBudgets, before the sendRecent calls
		that spend them, see ObjHolder::sendRecent
		Without a position - they haven't spawned, or they're spectating - nothing is throttled by distance for them
	*/
	glm::vec3 relevancePosition = glm::vec3(0, 0, 0);
	bool hasRelevancePosition = false;
	int updateByteBudget = 0;

	//Points to itself, used to pass to lua functions pretty much
	//Derivitive of a shared_ptr the server holds for all clients
	std::shared_ptr<JoinedClient> me;

	//Did they log in with the eval password?
	bool isAdmin = false;

	/*
		Have they been sent the sounds, types, and everything else that comes with joining
		That waits until they've answered the list of add-on files the server offered them, since a type
		can name a file they're about to be sent, see Networking/PacketsFromClient/ServerFileRequest.cpp
	*/
	bool sentJoinData = false;

	std::string name = "";

	std::string getIP() const { return ip; }

	bool isLoopback() const { return loopback; }

	netIDType getNetId() const { return netID; }

	float getPacketLoss() const { return peer ? peer->packetLoss : 0.0f; }

	//Send a packet to this client
	void send(const char* data, unsigned int len, PacketChannel channel) const;

	void send(ENetPacket* packet, PacketChannel channel) const;

	//Create a client from a connection event
	JoinedClient(ENetEvent& event,unsigned int _netID);

	void sendChat(std::string message) const;

	//Builds a CenterPrint packet, shared by sendCenterPrint (one client) and centerPrintAll (broadcast) so the latter only builds it once
	static ENetPacket* makeCenterPrintPacket(std::string text, unsigned int durationMS, float red, float green, float blue);

	void sendCenterPrint(std::string text, unsigned int durationMS, float red, float green, float blue) const;

	//Puts a colored vignette over their screen that wobbles the picture and fades out over durationMS, see Lua's client:setVignette
	void sendVignette(float red, float green, float blue, float alpha, float strength, unsigned int durationMS) const;

	//Similar to the destructor, but can be called before it to specify a reason
	void kick(KickReason reason);

	/*
		Destroy and disconnect a client
		JoinedClients should have destruction managed by the Server through shared ptr
	*/
	~JoinedClient();
};
