#pragma once

/*
	Functions that are 'members' (in the loose Lua sense) of a SimObject class are handled in other files
	Functions *related* to SimObject classes are also there even if they aren't members
	Pushing and popping SimObjects would be handled in ObjHolder
	Schedule and Cancel are handled in LuaScheduler

	This is for random other global scope Lua functions that don't fit in the above categories
*/

#include "../LandOfDran.h"
#include "../Utility/ExecutableArguments.h"
#include "../NetTypes/NetType.h"

extern ExecutableArguments* LUA_args;

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

//Not callable from lua, used for debugging C++ calls to Lua
void stackDump(lua_State* L);

//Logger wrapper functions
static int LUA_info(lua_State* L);
static int LUA_error(lua_State* L);
static int LUA_debug(lua_State* L);

static int LUA_shutdown(lua_State* L);

//Time of day and water, see ServerProgramData::worldTimeSeconds
static int LUA_setTimeOfDay(lua_State* L);
static int LUA_getTimeOfDay(lua_State* L);
static int LUA_setTimeScale(lua_State* L);
static int LUA_getTimeScale(lua_State* L);
static int LUA_setWaterLevel(lua_State* L);
static int LUA_getWaterLevel(lua_State* L);

//Look of the day/night cycle, see ServerProgramData::dayCycle
static int LUA_setSkyColor(lua_State* L);
static int LUA_getSkyColor(lua_State* L);
static int LUA_setFogColor(lua_State* L);
static int LUA_getFogColor(lua_State* L);
static int LUA_setSunColor(lua_State* L);
static int LUA_getSunColor(lua_State* L);
static int LUA_setAmbientColor(lua_State* L);
static int LUA_getAmbientColor(lua_State* L);
static int LUA_setFogDistance(lua_State* L);
static int LUA_getFogDistance(lua_State* L);
static int LUA_resetDayCycle(lua_State* L);

//Add-on files clients that don't have them can be sent, see Networking/ServerFiles.h
static int LUA_addServerFile(lua_State* L);
static int LUA_addServerFolder(lua_State* L);

//Register all funcs in this file
void registerOtherFunctions(lua_State* L);