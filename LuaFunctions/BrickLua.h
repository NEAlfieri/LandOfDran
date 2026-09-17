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

//Replaces a brick's music, light, and emitter settings with these (clamped), then updates what they make
void setBrickAttachments(Brick* brick, const BrickAttachments& settings);

//Sends the client the wrench dialog for this brick, after which one WrenchSubmit from them can change it, see Networking/PacketsFromClient/Wrench.cpp
void openWrenchDialog(ClientData& client, const Brick* brick);

//Sends the client the print menu for this brick, after which one PrintSubmit from them can change its print, see Networking/PacketsFromClient/Print.cpp
void openPrintMenu(ClientData& client, const Brick* brick);

//Whether a brick's type has TEX:PRINT faces, so a print put on it would actually show
bool brickCanPrint(const Brick* brick);

/*
	Registers global brick functions
	Returns brick methods to be passed to BrickHolder::makeLuaMetatable, which then deletes the list
*/
luaL_Reg* getBrickFunctions(lua_State* L);
