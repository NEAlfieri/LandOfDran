#pragma once

#include "../LandOfDran.h"

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

/*
	Everything in the Add-ons folder, and which of it the server runs

	An add-on is a folder in Add-ons with a server.lua in it, the one file the game runs to load
	the whole thing. Add-ons/list.txt says which ones are loaded, a line per add-on:

		Weapon_Gun true
		Vehicle_Jeep false

	The list is rewritten each time the server starts: an add-on that has appeared in the folder
	since the last run is added to it as enabled, so dropping one in is all it takes, and one whose
	folder has gone is dropped from it. Lines are loaded in the order they're in, so moving one up
	or down decides what loads first

	An add-on that arrives as a .zip is unpacked into a folder of the same name and the zip is
	deleted, see Utility/ZipFile.h, so from then on it's a folder add-on like any other. A zip
	whose folder is already there is left alone rather than written over

	Folders with no server.lua (a print pack, say) aren't listed and aren't loaded: there's nothing
	in them to run, and their content is found by whatever loads that kind of content
*/

//Unpacks any zipped add-ons, brings Add-ons/list.txt up to date with what's in the folder, and runs
//the server.lua of every add-on the list enables. Called once, just before the server's start script
void loadAddOns(lua_State* L);

//requireAddOn, for an add-on that needs another one loaded before it can work
void registerAddOnFunctions(lua_State* L);
