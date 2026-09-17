#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"

/*
	1 byte		-	packet type
	1 byte		-	1 while the client's camera is off flying, 0 once it's back on their player

	The client's own game decides this (the Drop Camera At Player and Drop Player At Camera key binds) and moves
	their player itself, like it does for every other move their player makes. The server hears about it so a light
	can mark where the loose camera is and so updates follow the camera rather than the player left behind.
	Ignored while client:setFreeCameraEnabled is off, which it is for everyone but admins
*/
void freeCameraRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	bool on = packet->data[1];

	client->setFreeCamera(pd, on);

	//They asked for something they aren't allowed, so tell them again what they can do and let their game put the camera back
	if (on && !client->freeCamera)
		client->sendAbilities();
}
