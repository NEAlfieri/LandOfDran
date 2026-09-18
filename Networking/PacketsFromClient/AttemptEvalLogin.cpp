#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"

/*
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void attemptEvalLogin(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (!pd->useEvalPassword)
		return;

	//Invalid empty packet
	if (packet->dataLength < 3)
		return;

	unsigned char messageLength = packet->data[1];

	if(packet->dataLength < 2 + messageLength)
		return;

	std::string password = std::string((char*)packet->data + 2, messageLength);

	if (password == pd->evalPassword)
	{
		source->isAdmin = true;

		//Admins get the free camera by default, see ClientData::freeCameraEnabled. Lua can take it back in ClientAdminLogin
		if (std::shared_ptr<ClientData> client = pd->getClient(source->me))
		{
			client->freeCameraEnabled = true;
			client->sendAbilities();
		}

		//The player list marks admins
		pd->playerListChanged = true;

		server->broadcastChat(source->name + " has logged into the eval console");
		info(source->name + " has logged into the eval console");	

		char ret[2];
		ret[0] = EvalLoginResponse;
		ret[1] = 255;	//True: you got the right password
		source->send(ret, 2, OtherReliable);

		pushClientLua(pd->luaState, source->me);
		pd->eventManager->callEvent(pd->luaState, "ClientAdminLogin", 1);
		lua_settop(pd->luaState, 0);
		return;
	}

	info(source->name + " failed to log into the eval console");

	char ret[2];
	ret[0] = EvalLoginResponse;
	ret[1] = 0;	//False: You guessed wrong!
	source->send(ret, 2, OtherReliable);
	return;
}
