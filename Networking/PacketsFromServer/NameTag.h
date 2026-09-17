#pragma once

#include "../HeldServerPacket.h"

/*
	This packet comes from Dynamic::makeNameTagPacket on the server
	Sets (or, with no text, clears) the text drawn floating over a dynamic, see dynamic:setNameTag in Lua
*/
struct NameTagPacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	NameTagPacket(unsigned int holdTime, ENetPacket* _packet);
	~NameTagPacket();
};
