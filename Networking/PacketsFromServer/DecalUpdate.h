#pragma once

#include "../HeldServerPacket.h"

/*
	A decal type, some new decals, or every decal cleared, from Lua's addDecalType, addDecal and clearDecals
	See DecalUpdateKind for the layout of each
*/
struct DecalUpdatePacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	DecalUpdatePacket(unsigned int holdTime, ENetPacket* _packet);
	~DecalUpdatePacket();
};
