#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

/*
	What the client should play the moment they click with a particular item, sent ahead of the click
	so a shot doesn't wait on a round trip. See Networking/ClickAction.h
*/
class SetClickActionPacket : public HeldServerPacket
{
public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	SetClickActionPacket(unsigned int holdTime, ENetPacket* _packet);
	~SetClickActionPacket();
};
