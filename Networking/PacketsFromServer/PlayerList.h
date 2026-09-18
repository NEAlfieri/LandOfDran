#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

//Everyone on the server with their ping and score text, for the player list window, see Interface/PlayerListWindow.h and LoopServer::broadcastPlayerList
class PlayerListPacket : public HeldServerPacket
{
public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	PlayerListPacket(unsigned int holdTime, ENetPacket* _packet);
	~PlayerListPacket();
};
