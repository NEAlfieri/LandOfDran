#include "OpenPrintMenu.h"

bool OpenPrintMenuPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte		-	packet type
		4 bytes		-	brick net ID
		1 byte		-	print name length
		0-255 bytes	-	the print it already wears, "" for none
	*/

	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 2 + sizeof(netIDType))
		return true;

	netIDType brickID;
	memcpy(&brickID, packet->data + 1, sizeof(netIDType));

	size_t at = 1 + sizeof(netIDType);
	size_t printNameLength = packet->data[at++];
	if (at + printNameLength > packet->dataLength)
	{
		error("Print menu packet was too short");
		return true;
	}

	std::string printName((char*)packet->data + at, printNameLength);

	pd.printMenu->openFor(brickID, printName, simulation.serverPrintNames);
	pd.context->setMouseLock(false);

	return true;
}

OpenPrintMenuPacket::OpenPrintMenuPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

OpenPrintMenuPacket::~OpenPrintMenuPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
