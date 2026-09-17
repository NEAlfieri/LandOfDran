#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../Networking/Server.h"
#include "../NetTypes/NetType.h"
#include "../GameLoop/ServerProgramData.h"

extern Server *LUA_server;
extern ServerProgramData* LUA_pd;

//TODO: Merge these with the ObjHolder functions that basically do the exact same thing
void pushClientLua(lua_State *L,std::shared_ptr<JoinedClient> client);
std::shared_ptr<JoinedClient> popClientLua(lua_State *L);

/*
	Paints a client's chosen colors onto the matching meshes of a dynamic and puts their face on it, broadcasting each change
	Parts previous painted that the client's current appearance doesn't go back to the model's own look
*/
void applyAppearance(Server const* server, ClientData& client, std::shared_ptr<Dynamic> dynamic, const PlayerAppearance* previous = nullptr);

//Every registered chat suggestion, sent to a client right after their connection is accepted
void sendChatSuggestions(const ServerProgramData* pd, JoinedClient* client);

void registerClientFunctions(lua_State *L);