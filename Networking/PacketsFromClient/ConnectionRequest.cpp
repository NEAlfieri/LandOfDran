#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../GameLoop/ClientData.h"
#include "../../LuaFunctions/SoundLua.h"
#include "../../LuaFunctions/EmitterLua.h"

/*	
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void applyConnectionRequest(JoinedClient * source,Server const * const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	//Invalid empty packet
	if (packet->dataLength < 3)
		return;

	//Do they have the same version as the server
	unsigned int gameVersion = packet->data[1];

	if (gameVersion != GAME_VERSION)
	{
		//Kick them, wrong version
		char ret[2];
		ret[0] = AcceptConnection;
		ret[1] = ConnectionWrongVersion;
		source->send(ret, 2, JoinNegotiation);
		source->kick(ConnectionRejected);
		return;
	}

	//What name would they like to have? (Wouldn't apply if they are logging in which I haven't reimplemented yet)
	unsigned int desiredNameLength = packet->data[2];

	if (packet->dataLength < 3 + desiredNameLength)
		return;

	std::string desiredName = std::string((char*)packet->data + 3, desiredNameLength);

	//Do any other currently connected guest (i.e. not logged in) users already have that name?
	bool nameAlreadyUsed = false;
	for (unsigned int a = 0; a < server->getNumClients(); a++)
	{
		if (server->getClientByIndex(a)->name == desiredName)
		{
			nameAlreadyUsed = true;
			break;
		}
	}

	if (nameAlreadyUsed)
	{
		//Yes, someone already has that name, kick the new client
		char ret[2];
		ret[0] = AcceptConnection;
		ret[1] = ConnectionNameUsed;
		source->send(ret, 2, JoinNegotiation);
		source->kick(ConnectionRejected);

		info("Client " + source->getIP() + " choose already used name " + desiredName + ", kicking.");

		return;
	}

	//Nothing was wrong, you get to connect!
	char ret[6];
	ret[0] = AcceptConnection;
	ret[1] = ConnectionOkay;
	unsigned int numTypes = (unsigned int)pd->allNetTypes.size();
	memcpy(ret + 2, &numTypes, sizeof(unsigned int));

	source->send(ret, 6, JoinNegotiation);

	//Set stuff up for our newly connected client
	source->name = desiredName;

	info("Client joined as guest with name " + desiredName);
	server->broadcastChat(desiredName + " connected.");

	//Sounds, particles, and emitters first, they don't count toward the types the loading bar waits for
	sendSoundTypes(pd, source);
	pd->bricks->sendSpecialTypes(source);
	pd->prints.sendTypes(source);
	sendParticleEmitterTypes(pd, source);

	//Send types to client:
	for (size_t a = 0; a < pd->allNetTypes.size(); a++)
		source->send(pd->allNetTypes[a]->createTypePacket(), JoinNegotiation);

	pd->makeClient(source->me);
}
 

