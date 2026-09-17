#include "VehicleBricks.h"

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
	2 bytes		-	index of the first brick in this packet
	2 bytes		-	how many bricks follow
	Per brick	-	BrickHolder::recordBytes, positions from the corner of the vehicle's bricks' box
*/
bool VehicleBricksPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	static constexpr unsigned int headerBytes = 1 + sizeof(netIDType) + sizeof(uint16_t) * 2;

	if (cmdArgs.gameState != InGame || !simulation.vehicles)
		return false;

	if (packet->dataLength < headerBytes)
		return true;

	netIDType id;
	uint16_t first, count;
	memcpy(&id, packet->data + 1, sizeof(netIDType));
	memcpy(&first, packet->data + 1 + sizeof(netIDType), sizeof(uint16_t));
	memcpy(&count, packet->data + 1 + sizeof(netIDType) + sizeof(uint16_t), sizeof(uint16_t));

	//Its creation packet hasn't been applied yet
	std::shared_ptr<Vehicle> vehicle = simulation.vehicles->find(id);
	if (!vehicle)
		return false;

	if (packet->dataLength < headerBytes + count * BrickHolder::recordBytes)
	{
		error("VehicleBricks packet shorter than its brick count");
		return true;
	}

	//The same bricks can be sent twice to someone who joins right as a vehicle is made, see ObjHolder::sendRecent
	if (first != vehicle->bricks.size() || vehicle->hasAllBricks())
		return true;

	for (unsigned int a = 0; a < count && !vehicle->hasAllBricks(); a++)
	{
		Brick desc = BrickHolder::readRecord(packet->data + headerBytes + a * BrickHolder::recordBytes);

		//The record has the server's type ID, see SpecialBrickTypesPacket
		if (desc.isSpecial())
			desc.typeID = desc.typeID < simulation.brickTypeFromServer.size() ? simulation.brickTypeFromServer[desc.typeID] : 0;

		//And the server's print ID, see BrickPrintTypesPacket
		if (desc.printID != 0)
			desc.printID = desc.printID < simulation.printFromServer.size() ? simulation.printFromServer[desc.printID] : 0;

		vehicle->bricks.push_back(desc);
	}

	if (vehicle->hasAllBricks())
		vehicle->finishClient(&pd.brickTypes, pd.brickRenderer, pd.tireModel, simulation.dynamicTypes);

	return true;
}

VehicleBricksPacket::VehicleBricksPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

VehicleBricksPacket::~VehicleBricksPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
