#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

/*
	Opens the print menu for a brick, see Interface/PrintMenu.h
*/
class OpenPrintMenuPacket : public HeldServerPacket
{
public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	OpenPrintMenuPacket(unsigned int holdTime, ENetPacket* _packet);
	~OpenPrintMenuPacket();
};
