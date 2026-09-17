#include "OpenVehicleWrench.h"

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
	4 bytes		-	how many bricks it has
	The rest	-	BrickAttachments::write with just its music
*/
bool OpenVehicleWrenchPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 1 + sizeof(netIDType) + sizeof(uint32_t) + 1)
		return true;

	netIDType vehicleID;
	uint32_t brickCount;
	memcpy(&vehicleID, packet->data + 1, sizeof(netIDType));
	memcpy(&brickCount, packet->data + 1 + sizeof(netIDType), sizeof(uint32_t));

	WrenchSubmission editing;
	editing.vehicleID = vehicleID;

	size_t at = 1 + sizeof(netIDType) + sizeof(uint32_t);
	if (!editing.attachments.read(packet->data, packet->dataLength, at))
	{
		error("Vehicle wrench dialog packet was too short");
		return true;
	}

	std::string label = "Vehicle, " + std::to_string(brickCount) + (brickCount == 1 ? " brick" : " bricks");
	pd.wrenchDialog->openFor(editing, label, pd.audio->getMusicNames(), {});
	pd.context->setMouseLock(false);

	return true;
}

OpenVehicleWrenchPacket::OpenVehicleWrenchPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

OpenVehicleWrenchPacket::~OpenVehicleWrenchPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
