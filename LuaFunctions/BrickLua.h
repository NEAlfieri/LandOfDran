#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../GameLoop/ServerProgramData.h"

extern ServerProgramData* LUA_pd;

/*
	Makes, updates, or gets rid of the sound loop, Light, and Emitter a brick's attachments call for, so they match its settings
	Set as BrickHolder::spawnAttachments, for bricks loaded from saves
*/
void updateBrickAttachments(Brick* brick);

//Gets rid of the loop, light, and emitter a brick's attachments made, set as BrickHolder::removeAttachments
void removeBrickAttachments(Brick* brick);

/*
	Spawns the vehicle of every brick wrenched to spawn one whose vehicle is gone, once a second, see BrickAttachments::vehicleSpawnName
	Called every tick by LoopServer, and does nothing until a second has passed since it last looked
*/
void respawnBrickVehicles();

//The vehicle a brick's vehicle spawn made, if it's still around, and the display item its item spawn made, see BrickAttachments
std::shared_ptr<Vehicle> getBrickSpawnedVehicle(const Brick* brick);
std::shared_ptr<Item> getBrickDisplayItem(const Brick* brick);

//Whether a brick's type is a Vehicle Spawn brick, whose wrench dialog picks a vehicle to keep above it
bool brickIsVehicleSpawn(const Brick* brick);

//Replaces a brick's music, light, and emitter settings with these (clamped), then updates what they make
void setBrickAttachments(Brick* brick, const BrickAttachments& settings);

//Sends the client the wrench dialog for this brick, after which one WrenchSubmit from them can change it, see Networking/PacketsFromClient/Wrench.cpp
void openWrenchDialog(ClientData& client, const Brick* brick);

//Sends the client the print menu for this brick, after which one PrintSubmit from them can change its print, see Networking/PacketsFromClient/Print.cpp
void openPrintMenu(ClientData& client, const Brick* brick);

//Whether a brick's type has TEX:PRINT faces, so a print put on it would actually show
bool brickCanPrint(const Brick* brick);

//Pushes a table of the light fields in settings, the way brick:getLight returns them, see LuaAPI.md
void pushLightTable(lua_State* L, const BrickAttachments& settings);

/*
	Reads the light fields in the table at index into settings, leaving the ones it doesn't have alone
	Logs an error and returns false for an unknown field, a value of the wrong kind, or a direction of 0, 0, 0
*/
bool readLightTable(lua_State* L, int index, BrickAttachments& settings);

/*
	Registers global brick functions
	Returns brick methods to be passed to BrickHolder::makeLuaMetatable, which then deletes the list
*/
luaL_Reg* getBrickFunctions(lua_State* L);
