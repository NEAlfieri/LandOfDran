#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"

//The client's ghost brick, as a brick record from BrickHolder::writeRecord whose id is ignored
void plantBrick(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + BrickHolder::recordBytes)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	Brick desc = BrickHolder::readRecord(packet->data + 1);
	desc.ownerID = source->getNetId();

	//Prints are put on with the wrench, so the record's is the client's own index for one and means nothing here
	desc.printID = 0;

	Brick* brick = pd->bricks->add(desc);
	if (!brick)
	{
		source->sendCenterPrint("Can't plant there, it overlaps another brick or is out of bounds", 2000, 1.0f, 0.4f, 0.4f);
		return;
	}

	client->plantedBricks.push_back(brick->netId);

	pushClientLua(pd->luaState, source->me);
	pd->bricks->pushLua(pd->luaState, brick);
	pd->eventManager->callEvent(pd->luaState, "ClientPlantBrick", 2);
	lua_settop(pd->luaState, 0);
}

//Removes the newest brick this client planted that still exists
void undoBrick(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	while (!client->plantedBricks.empty())
	{
		netIDType id = client->plantedBricks.back();
		client->plantedBricks.pop_back();

		if (Brick* brick = pd->bricks->find(id))
		{
			pd->bricks->remove(brick, true);
			return;
		}
	}
}
