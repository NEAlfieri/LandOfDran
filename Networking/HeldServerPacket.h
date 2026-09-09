#pragma once

#include "../LandOfDran.h"
#include "../GameLoop/ClientProgramData.h"
#include "../GameLoop/Simulation.h"

/*
	This is a special abstract struct to wrap packets *from* the server *on* the client
	The Client class has one big loop that tries to apply the packets (using that virtual function), and will
	delete them if they are applied succesfully or they have been waiting too long
	"Apply" means, for instance, if you make 10 basketballs and say to attach a smoke emitter one number 4
	the emitter packet can't be applied until the packet creating basketball 4 is finished processing
*/
struct HeldServerPacket
{
protected:

	ENetPacket* packet = nullptr;

	//When this number is less than SDL_GetTicks() we can discard the packet
	//Set by Client::packetHoldTime
	unsigned int deletionTime = 0;

public:

	//What type of packet is this
	FromServerPacketType type = FromServerPacketType::InvalidServer;

	virtual ~HeldServerPacket() {}

	//Returns true if the packet was successfully applied (and can be discarded)
	virtual bool applyPacket(const ClientProgramData& pd,Simulation &simulation, const ExecutableArguments& cmdArgs) = 0;

	//True once deletionTime has passed, meaning the packet has been held too long and should be discarded even though it was never applied
	bool isExpired() const { return SDL_GetTicks() > deletionTime; }
};
