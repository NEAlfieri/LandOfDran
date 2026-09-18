#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

/*
	Lua's client:setVignette: a color drawn in from the edges of the screen with the picture wobbling like it does under the
	water, fading out over a duration. Replaces whatever vignette was showing, see Simulation::vignette and underwater.frag
*/
class VignettePacket : public HeldServerPacket
{
public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	VignettePacket(unsigned int holdTime, ENetPacket* _packet);
	~VignettePacket();
};
