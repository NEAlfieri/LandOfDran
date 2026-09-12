#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"

/*
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void movementInputs(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < sizeof(float) * 6 + sizeof(netIDType) + 2)
		return;

	netIDType targetID;
	memcpy(&targetID, packet->data + 1, sizeof(netIDType));
	unsigned char flags = packet->data[1 + sizeof(netIDType)];
	float x, y, z, px, py, pz;
	memcpy(&x, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 0, sizeof(float));
	memcpy(&y, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 1, sizeof(float));
	memcpy(&z, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 2, sizeof(float));
	memcpy(&px, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 3, sizeof(float));
	memcpy(&py, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 4, sizeof(float));
	memcpy(&pz, packet->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 5, sizeof(float));

	auto clientData = pd->getClient(source->me);
	if(!clientData)
		return;

	//Find the controller with the ID sent by the client
	auto controller = std::find_if(clientData->controllers.begin(), clientData->controllers.end(), [&targetID](PlayerController& pc)
		{ 
			auto locked = pc.target.lock(); 
			if (!locked)
				return false;
			return locked->getID() == targetID;
		}
	);

	if (controller == clientData->controllers.end())
		return;

	//Pass movement keys and camera direction/position to the controller
	controller->control(pd->physicsWorld, 0, glm::vec3(x, y, z), glm::vec3(px, py, pz), flags & 1, flags & 2, flags & 4, flags & 8, flags & 16);
}
