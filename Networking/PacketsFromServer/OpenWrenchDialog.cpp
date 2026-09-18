#include "OpenWrenchDialog.h"

bool OpenWrenchDialogPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte		-	packet type
		4 bytes		-	brick net ID
		1 byte		-	1 if it collides
		1 byte		-	name length
		0-255 bytes	-	name
		Then		-	BrickAttachments::write
		4 bytes		-	net ID of the light the brick already has, NO_ID for none
		1 byte		-	how many vehicle spawns follow, 0 unless the brick is a Vehicle Spawn brick
		Each		-	a length byte and the name of a vehicle spawn the dialog can pick
	*/

	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 2 + sizeof(netIDType) + 1)
		return true;

	WrenchSubmission editing;
	memcpy(&editing.brickID, packet->data + 1, sizeof(netIDType));
	editing.collides = packet->data[1 + sizeof(netIDType)] & 1;

	size_t at = 2 + sizeof(netIDType);
	size_t nameLength = packet->data[at++];
	if (at + nameLength > packet->dataLength)
		return true;

	editing.name.assign((char*)packet->data + at, nameLength);
	at += nameLength;

	if (!editing.attachments.read(packet->data, packet->dataLength, at))
	{
		error("Wrench dialog packet was too short");
		return true;
	}

	//The brick's real light, which the preview stands in for while the dialog is open
	if (at + sizeof(netIDType) > packet->dataLength)
	{
		error("Wrench dialog packet was too short");
		return true;
	}
	memcpy(&editing.lightID, packet->data + at, sizeof(netIDType));
	at += sizeof(netIDType);

	//The vehicles a Vehicle Spawn brick can pick from, which only the server knows, see registerVehicleSpawn
	std::vector<std::string> vehicleSpawns;
	if (at >= packet->dataLength)
	{
		error("Wrench dialog packet was too short");
		return true;
	}
	size_t spawnCount = packet->data[at++];
	for (size_t a = 0; a < spawnCount; a++)
	{
		if (at >= packet->dataLength)
		{
			error("Wrench dialog packet was too short");
			return true;
		}
		size_t spawnLength = packet->data[at++];
		if (at + spawnLength > packet->dataLength)
		{
			error("Wrench dialog packet was too short");
			return true;
		}
		vehicleSpawns.emplace_back((char*)packet->data + at, spawnLength);
		at += spawnLength;
	}

	//Every item type the server gave a name, by script name with that name, for the Item section to pick from. One without
	//a name isn't meant to be offered by a brick, like a uiName left off a Blockland datablock
	std::vector<std::pair<std::string, std::string>> itemTypes;
	for (const std::shared_ptr<DynamicType>& type : simulation.dynamicTypes)
	{
		if (type && type->isItemType && !type->itemName.empty())
			itemTypes.emplace_back(type->scriptName, type->itemName);
	}

	std::string label = "Brick";
	if (const Brick* brick = simulation.bricks ? simulation.bricks->find(editing.brickID) : nullptr)
	{
		const SpecialBrickType* type = brick->isSpecial() ? pd.brickTypes.getSpecial(brick->typeID - 1) : nullptr;
		if (type)
		{
			label = type->uiName;
			editing.part = type->vehiclePart;
			editing.vehicleSpawnBrick = type->vehicleSpawn;
		}
		else
			label = std::to_string(brick->width) + "x" + std::to_string(brick->length) + " brick, " + std::to_string(brick->height) + (brick->height == 1 ? " plate" : " plates") + " tall";
	}

	//So turning the light on starts it off with a new light's settings
	if (!editing.attachments.hasLight)
		editing.attachments.resetLight();

	pd.wrenchDialog->openFor(editing, label, pd.audio->getMusicNames(), pd.particles->getEmitterTypeNames(), pd.audio->getEffectNames(), vehicleSpawns, itemTypes);
	pd.context->setMouseLock(false);

	return true;
}

OpenWrenchDialogPacket::OpenWrenchDialogPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

OpenWrenchDialogPacket::~OpenWrenchDialogPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
