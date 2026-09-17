#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"

/*
	1 byte		-	packet type
	1 byte		-	1 if the item bar is out
	1 byte		-	picked slot

	Which of the client's items is in their player's hand, LoopServer::updateItems tells everyone
*/
void inventorySelect(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 3 || packet->data[2] >= inventorySize)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	client->inventoryOpen = packet->data[1] & 1;
	client->selectedSlot = packet->data[2];
}

/*
	1 byte		-	packet type
	1 byte		-	1 while their paint palette wants a paint can in their hand

	Only fires ClientPaintCan, Inventory.lua is what actually puts a can in their hand and takes it back
*/
void paintCanRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	bool out = packet->data[1] & 1;
	if (client->paintCanOut == out)
		return;

	client->paintCanOut = out;

	lua_State* L = pd->luaState;
	pushClientLua(L, source->me);
	lua_pushboolean(L, out);
	pd->eventManager->callEvent(L, "ClientPaintCan", 2);
	lua_settop(L, 0);
}

/*
	1 byte		-	packet type
	1 byte		-	picked slot

	The client pressed the drop item keys, which only fires ClientDropItem, Inventory.lua throws the item
*/
void dropItemRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2 || packet->data[1] >= inventorySize)
		return;

	if (!pd->getClient(source->me))
		return;

	lua_State* L = pd->luaState;
	pushClientLua(L, source->me);
	lua_pushinteger(L, packet->data[1]);
	pd->eventManager->callEvent(L, "ClientDropItem", 2);
	lua_settop(L, 0);
}
