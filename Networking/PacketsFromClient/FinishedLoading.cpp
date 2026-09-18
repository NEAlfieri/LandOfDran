#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"
#include "../../LuaFunctions/SoundLua.h"
#include "../../LuaFunctions/SkyLua.h"
#include "../../LuaFunctions/DecalLua.h"
#include "../../LuaFunctions/VehicleLua.h"

/*
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void clientFinishedLoading(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	pushClientLua(pd->luaState, source->me);
	pd->eventManager->callEvent(pd->luaState, "ClientJoin", 1);
	lua_settop(pd->luaState, 0); //We don't need to do anything with the client the lua function returns

	info(source->name + " finished loading phase 1");

	//Single player: this is the host's own client connecting to its own embedded server over loopback, so log
	//them into the eval console automatically instead of making them enter a password for their own game
	if (pd->isTrustedLocalAdmin(source) && !source->isAdmin)
	{
		source->isAdmin = true;

		char ret[2];
		ret[0] = EvalLoginResponse;
		ret[1] = 255;
		source->send(ret, 2, OtherReliable);

		//Same as logging in with the password: admins get the free camera, see ClientData::freeCameraEnabled
		if (std::shared_ptr<ClientData> client = pd->getClient(source->me))
		{
			client->freeCameraEnabled = true;
			client->sendAbilities();
		}
	}

	//They finished loading types, now send pre-existing SimObjects
	pd->dynamics->sendAll(source);
	pd->statics->sendAll(source);
	pd->lights->sendAll(source);
	pd->emitters->sendAll(source);
	pd->vehicles->sendAll(source);
	//After everything they can be tied to
	pd->ropes->sendAll(source);
	sendVehicleState(pd, source);
	pd->bricks->sendAll(source);
	//After the bricks they're on, on the same channel
	sendDecals(pd, source);

	//Loops already playing and the reverb preset
	sendSoundState(pd, source);

	sendSkybox(pd, source);

	//What their game may do on its own: jets, the flashlight, and the free camera, which needs telling even when Lua never touched them
	if (std::shared_ptr<ClientData> client = pd->getClient(source->me))
		client->sendAbilities();

	//Time of day and water level, instead of waiting up to a second for the regular update
	pd->worldStateChanged = true;

	//And who's here, for them and for everyone who now has them to list
	pd->playerListChanged = true;
}
