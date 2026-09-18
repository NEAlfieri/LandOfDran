#pragma once

#include "../HeldServerPacket.h"

/*
	This packet comes from Dynamic::setPart on the server
	Puts a model worn on a dynamic, like a hat on a player, in one of its slots painted a color, or with no name takes that slot's part off
*/
struct DynamicPartPacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	DynamicPartPacket(unsigned int holdTime, ENetPacket* _packet);
	~DynamicPartPacket();
};
