#pragma once

#include "../LandOfDran.h"
#include "HeldServerPacket.h"
#include "PacketsFromServer/AcceptConnection.h"
#include "PacketsFromServer/AddSimObjectType.h"
#include "PacketsFromServer/AddSimObjects.h"
#include "PacketsFromServer/UpdateSimObjects.h"
#include "PacketsFromServer/DeleteSimObjects.h"
#include "PacketsFromServer/ChatMessageFromServer.h"
#include "PacketsFromServer/EvalLoginResponse.h"
#include "PacketsFromServer/DisplayConsoleLine.h"
#include "PacketsFromServer/TakeOverPhysics.h"
#include "PacketsFromServer/CameraSettings.h"
#include "PacketsFromServer/MovementSettings.h"
#include "PacketsFromServer/MeshAppearance.h"
#include "PacketsFromServer/ServerPerformanceDetails.h"
#include "PacketsFromServer/CenterPrint.h"
#include "../GameLoop/ClientProgramData.h"
#include "../GameLoop/Simulation.h"

/*
	This is the networking interface that the client program should have one instance of, that managers sending and receiving from the server
	Do not confuse with JoinedClient, which is something the server may manage one or more of
	Also don't conufse with LoopClient which is just a container for the main game loop that the client program (as oppsed to dedicated servers) runs
*/
class Client
{
	ENetHost* client = nullptr;
	ENetPeer* peer = nullptr;

	//How long to hold server packets for if they can't be applied instantly
	unsigned int packetHoldTime = 10000;

	bool valid = false;

	//If the server kicked us first, don't bother making sure the server acknowledges our disconnection
	bool alreadyDisconnected = false;

	std::vector<HeldServerPacket*> packets;

	//Try applying each held packet and deleting any that expired or have been applied
	void tryApplyHeldPackets(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs);

	//For DebugMenu to show bandwidth usage over last second
	float lastIncomingQueryTime = 0;
	float lastOutgoingQueryTime = 0;
	float lastIncoming = 0;
	float lastOutgoing = 0;

public:

	//Apparently version 1.3.17 ENet does not have ENetHost::totalQueued ???
#if ENET_VERSION_MAJOR == 1 && ENET_VERSION_MINOR == 3 && ENET_VERSION_PATCH == 18
	float getNumQueued() const { return client->totalQueued;  }
#else
	float getNumQueued() const { return -1234;  }
#endif

	float getIncoming();
	float getOutgoing();

	float getPing() const			{ return (float)peer->roundTripTime; }
	float getPingVariance() const	{ return (float)peer->roundTripTimeVariance; }
	float getLoss() const			{ return (float)peer->packetLoss; }

	bool isValid() const { return valid; }

	//Returns NotKicked unless the client is disconnected
	KickReason run(const ClientProgramData & pd,Simulation &simulation, const ExecutableArguments &cmdArgs);

	void send(const char* data, unsigned int len, PacketChannel channel);
	void send(ENetPacket* packet, PacketChannel channel);

	//pump, if set, is called periodically while waiting for the connection to complete.
	//Used for single player: the embedded local server needs to service its own ENet host
	//for the handshake to complete, since nothing else is ticking it while we block here.
	Client(std::string ip,unsigned int port,unsigned int _packetHoldTime, std::function<void()> pump = nullptr);
	~Client();
};
