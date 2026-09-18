#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../Networking/Server.h"
#include "../Networking/ObjHolder.h"
#include "../GameLoop/ServerProgramData.h"

extern Server* LUA_server;
extern ServerProgramData* LUA_pd;

/*
	Registers createRope, getRopeId, getRopeIdx, and getNumRopes, and gives dynamics and items the old game's
	attachByRope, attachByRopeBrick, and clearRopes, so it has to come after their metatables exist
	Returns a list to be passed to ObjHolder<Rope>::makeLuaMetatable which then deletes the list
	This function needs to be updated with each rope related function added to the Lua API
*/
luaL_Reg* getRopeFunctions(lua_State* L);
