#include "OpenVehicleWrench.h"

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
	4 bytes		-	how many bricks it has, 0 for a model vehicle
	4 bytes		-	net ID of the light its headlight shines with, NO_ID for none, which the preview stands in for
	The rest	-	BrickAttachments::write with its music, horn, and headlight as the light
*/
bool OpenVehicleWrenchPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 1 + sizeof(netIDType) + sizeof(uint32_t) + sizeof(netIDType) + 1)
		return true;

	netIDType vehicleID;
	uint32_t brickCount;
	memcpy(&vehicleID, packet->data + 1, sizeof(netIDType));
	memcpy(&brickCount, packet->data + 1 + sizeof(netIDType), sizeof(uint32_t));

	WrenchSubmission editing;
	editing.vehicleID = vehicleID;
	memcpy(&editing.lightID, packet->data + 1 + sizeof(netIDType) + sizeof(uint32_t), sizeof(netIDType));

	//The light's direction is in the vehicle's space, and the dialog turns it relative to the way it drives
	if (std::shared_ptr<Vehicle> vehicle = simulation.vehicles ? simulation.vehicles->find(vehicleID) : nullptr)
		editing.vehicleForward = vehicle->forward;

	size_t at = 1 + sizeof(netIDType) + sizeof(uint32_t) + sizeof(netIDType);
	if (!editing.attachments.read(packet->data, packet->dataLength, at))
	{
		error("Vehicle wrench dialog packet was too short");
		return true;
	}

	//So checking Has headlight starts it off as a headlight, shining the way the vehicle drives, rather than as a plain light
	if (!editing.attachments.hasLight)
		editing.attachments.resetHeadlight(editing.vehicleForward);

	//A model vehicle has no bricks at all, so it gets no brick count and no Save section, see WrenchSubmission::madeOfBricks
	editing.madeOfBricks = brickCount > 0;
	std::string label = editing.madeOfBricks
		? "Vehicle, " + std::to_string(brickCount) + (brickCount == 1 ? " brick" : " bricks")
		: "Vehicle, a model";
	pd.wrenchDialog->openFor(editing, label, pd.audio->getMusicNames(), {}, pd.audio->getEffectNames());
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
