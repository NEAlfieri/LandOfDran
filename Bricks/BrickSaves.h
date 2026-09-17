#pragma once

#include "BrickHolder.h"
#include "BrickTypes.h"
#include "PrintTypes.h"

/*
	Save files live directly in the Saves folder
	Returns the full path for a save file name, or "" if the name could reach outside that folder
*/
std::string getSavePath(const std::string& fileName);

/*
	Vehicle saves live in Saves/Vehicles, on the server for Lua and on each player's own computer for the ones they save from a vehicle's wrench dialog
	Returns the full path for a name without its .lod extension, or "" if the name could reach outside that folder
*/
std::string getVehicleSavePath(const std::string& name);

/*
	Writes bricks in our binary save format, see saveLodBuild, types gives special bricks' names and prints their prints'
	Returns false if the stream failed
*/
bool writeLodBricks(std::ostream& file, const std::vector<const Brick*>& bricks, const BrickTypes* types, const PrintTypes* prints, bool omitOwnership);

//How readLodBricks went
struct LodReadResult
{
	//It started with a save format's number
	bool valid = false;
	//It didn't end early
	bool complete = false;
	//Special bricks of types we don't have, by name
	int skippedSpecial = 0;
	std::map<std::string, int> missingTypes;
	//Prints we don't have, by name, whose bricks are loaded plain
	std::map<std::string, int> missingPrints;
	//Bricks with invalid sizes, rotations, or positions
	int invalid = 0;
};

/*
	Reads either version of the old binary format or our own newer one from a stream, calling found with each brick's description
	Bricks of special types we don't have or with invalid values are skipped and counted instead
*/
LodReadResult readLodBricks(std::istream& file, const BrickTypes* types, const PrintTypes* prints, const std::function<void(Brick&)>& found);

/*
	Old Land of Dran binary save format, written like OldServer/lua/miscFunctions.h's saveBuild except that each brick's
	music, light, and emitter are written as BrickAttachments under a newer version number the old game can't read
	Returns false if the file couldn't be written
*/
bool saveLodBuild(const BrickHolder& bricks, const PrintTypes* prints, const std::string& path, bool omitOwnership);

/*
	Loads either version of the old binary format or our own newer one, offset by whole studs/plates
	Special bricks of types we don't have are skipped, and so are the old game's lights and music
	A brick's print is taken by name, from the old game's saves too, and dropped if we don't have that print
	Returns how many bricks were added, or -1 if the file couldn't be read
*/
int loadLodBuild(BrickHolder& bricks, const PrintTypes* prints, const std::string& path, int offsetX, int offsetY, int offsetZ);

/*
	How a Blockland import finds our versions of what a .bls save put on its bricks, by the uiName the save uses
	findEmitterType and findMusic return the name of our type, or "" if we don't have one
	setLight gives attachments our light for a Blockland light type, false if we don't have one
*/
struct BlocklandAttachmentLookup
{
	std::function<bool(const std::string&, BrickAttachments&)> setLight = nullptr;
	std::function<std::string(const std::string&)> findEmitterType = nullptr;
	std::function<std::string(const std::string&)> findMusic = nullptr;
};

/*
	Imports a Blockland .bls save, using the file's own color palette
	Bricks are resolved by name through types, special and unknown bricks are skipped and logged by name
	Brick lights, emitters, and music go through lookup, and any we don't have are skipped and logged by name
	Returns how many bricks were added, or -1 if the file couldn't be read
*/
int loadBlocklandBuild(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, const std::string& path, const BlocklandAttachmentLookup& lookup);
