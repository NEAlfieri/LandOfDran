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

	std::string label = "Brick";
	if (const Brick* brick = simulation.bricks ? simulation.bricks->find(editing.brickID) : nullptr)
	{
		const SpecialBrickType* type = brick->isSpecial() ? pd.brickTypes.getSpecial(brick->typeID - 1) : nullptr;
		if (type)
		{
			label = type->uiName;
			editing.part = type->vehiclePart;
		}
		else
			label = std::to_string(brick->width) + "x" + std::to_string(brick->length) + " brick, " + std::to_string(brick->height) + (brick->height == 1 ? " plate" : " plates") + " tall";
	}

	//So turning the light on starts it off with a new light's settings
	if (!editing.attachments.hasLight)
		editing.attachments.resetLight();

	pd.wrenchDialog->openFor(editing, label, pd.audio->getMusicNames(), pd.particles->getEmitterTypeNames());
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
