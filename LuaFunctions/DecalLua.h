#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../Networking/Server.h"
#include "../GameLoop/ServerProgramData.h"

extern Server* LUA_server;
extern ServerProgramData* LUA_pd;

//Every decal type, to a client that just connected
void sendDecalTypes(const ServerProgramData* pd, JoinedClient* client);

//Every decal already in the world, to a client that just finished loading, after its bricks
void sendDecals(const ServerProgramData* pd, JoinedClient* client);

//Once a tick after the new bricks went out: forgets decals whose brick is gone, and broadcasts the ones made since the last tick
void sendNewDecals(const ServerProgramData* pd, Server* server);

//addDecalType, addDecal, clearDecals, setMaxDecals, getMaxDecals, getNumDecals
void registerDecalFunctions(lua_State* L);
