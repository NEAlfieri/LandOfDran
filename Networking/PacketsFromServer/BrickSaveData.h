#pragma once

#include "../HeldServerPacket.h"

/*
	This packet comes from brickSaveRequest in Networking/PacketsFromClient/BrickSaveFiles.cpp
	Part of a save of every brick we asked for from the saved bricks window, written to our Saves folder once all of it arrives
*/
struct BrickSaveDataPacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	BrickSaveDataPacket(unsigned int holdTime, ENetPacket* _packet);
	~BrickSaveDataPacket();
};
