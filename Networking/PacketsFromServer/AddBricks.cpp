#include "AddBricks.h"

bool AddBricksPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame || !simulation.bricks)
		return false;

	if (packet->dataLength < 3)
		return true;

	uint16_t count;
	memcpy(&count, packet->data + 1, sizeof(uint16_t));

	if (packet->dataLength < 3 + count * BrickHolder::recordBytes)
	{
		error("AddBricks packet shorter than its brick count");
		return true;
	}

	for (unsigned int a = 0; a < count; a++)
	{
		Brick desc = BrickHolder::readRecord(packet->data + 3 + a * BrickHolder::recordBytes);

		//The record has the server's type ID, see SpecialBrickTypesPacket
		if (desc.isSpecial())
			desc.typeID = desc.typeID < simulation.brickTypeFromServer.size() ? simulation.brickTypeFromServer[desc.typeID] : 0;

		//And the server's print ID, see BrickPrintTypesPacket
		if (desc.printID != 0)
			desc.printID = desc.printID < simulation.printFromServer.size() ? simulation.printFromServer[desc.printID] : 0;

		simulation.bricks->addFromServer(desc);
	}

	return true;
}

AddBricksPacket::AddBricksPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

AddBricksPacket::~AddBricksPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
