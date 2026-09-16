#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

//The server's print IDs and names, matched to our own prints by name, see PrintTypes::sendTypes for the layout
class BrickPrintTypesPacket : public HeldServerPacket
{
	public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	BrickPrintTypesPacket(unsigned int holdTime, ENetPacket* _packet);
	~BrickPrintTypesPacket();
};
