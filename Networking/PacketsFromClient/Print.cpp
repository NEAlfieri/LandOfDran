#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/BrickLua.h"

/*
	1 byte		-	packet type
	4 bytes		-	brick net ID
	1 byte		-	print name length
	0-255 bytes	-	print name, "" for no print

	The print the client picked in the print menu the server last sent them, see Interface/PrintMenu.h
*/
void printSubmit(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2 + sizeof(netIDType))
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	netIDType brickID;
	memcpy(&brickID, packet->data + 1, sizeof(netIDType));

	//Only the brick they were last sent a print menu for, and only once
	if (brickID != client->printedBrickID)
		return;
	client->printedBrickID = NO_ID;

	Brick* brick = pd->bricks->find(brickID);
	if (!brick)
		return;

	size_t at = 1 + sizeof(netIDType);
	size_t printNameLength = packet->data[at++];
	if (at + printNameLength > packet->dataLength)
		return;

	std::string printName((char*)packet->data + at, printNameLength);

	//Only a print brick has faces to wear one, and only a print we have
	const SpecialBrickType* type = brick->isSpecial() ? pd->brickTypes.getSpecial(brick->typeID - 1) : nullptr;
	if (!type || type->groupCount[BrickTexturePrint] < 1)
		return;

	int found = pd->prints.find(printName);
	if (!printName.empty() && found < 0)
		return;

	uint16_t printID = (uint16_t)(found + 1);
	if (brick->printID != printID)
		pd->bricks->setPrint(brick, printID);
}
