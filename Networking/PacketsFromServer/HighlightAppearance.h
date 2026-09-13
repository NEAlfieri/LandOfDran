#pragma once

#include "../HeldServerPacket.h"

/*
	This packet comes from Dynamic::makeHighlightPacket or StaticObject::makeHighlightPacket on the server
	Applies (or, if color.a <= 0, clears) an outline/highlight effect on a dynamic or static instance
*/
struct HighlightAppearancePacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	HighlightAppearancePacket(unsigned int holdTime, ENetPacket* _packet);
	~HighlightAppearancePacket();
};
