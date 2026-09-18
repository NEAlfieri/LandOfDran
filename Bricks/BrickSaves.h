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
	What a save says about itself besides its bricks: who saved it, when, and a picture of the build
	Written at the front of our newest save version, and read back on its own by readLodSaveInfo without touching the bricks
*/
struct LodSaveInfo
{
	//Name of the player who saved it, "" for a save Lua or an older version of the game wrote
	std::string savedBy;
	//Unix seconds, 0 when the save doesn't say
	int64_t savedAt = 0;
	//How many bricks it has, from the header of any version
	unsigned int brickCount = 0;
	//A JPEG of the build looking down on it, from the client that saved it, "" for none
	std::string thumbnail;
};

//Biggest picture a save can carry, a 256 by 256 JPEG is a few tens of kilobytes
constexpr size_t maxSaveThumbnailBytes = 256 * 1024;

/*
	Writes bricks in our binary save format, see saveLodBuild, types gives special bricks' names and prints their prints'
	info's name, time, and thumbnail go at the front of the file, or nothing when it's nullptr; its brickCount is ignored
	Returns false if the stream failed
*/
bool writeLodBricks(std::ostream& file, const std::vector<const Brick*>& bricks, const BrickTypes* types, const PrintTypes* prints, bool omitOwnership, const LodSaveInfo* info = nullptr);

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
	info, if given, comes back with what the file says about itself, see LodSaveInfo
*/
LodReadResult readLodBricks(std::istream& file, const BrickTypes* types, const PrintTypes* prints, const std::function<void(Brick&)>& found, LodSaveInfo* info = nullptr);

/*
	Reads just the front of a save: who saved it, when, its picture, and how many bricks it has, without reading the bricks
	Returns false if the file couldn't be opened or isn't a Land of Dran save
*/
bool readLodSaveInfo(const std::string& path, LodSaveInfo& info);
bool readLodSaveInfo(std::istream& file, LodSaveInfo& info);

/*
	Puts a picture into a save file held in memory, in place of whatever picture it had, leaving its bricks as they are
	Returns false, changing nothing, for a save from before pictures or one that isn't a save at all
*/
bool setLodSaveThumbnail(std::string& file, const std::string& thumbnail);

/*
	Reads what a Blockland .bls save says about itself, which is only how many bricks it has (its Linecount line)
	Returns false if the file couldn't be opened or has no Linecount line
*/
bool readBlocklandSaveInfo(const std::string& path, LodSaveInfo& info);

//One save in the Saves folder, for a list players pick from, see listBrickSaves
struct BrickSaveListing
{
	//The file's name with its extension, which is what a load or save request names
	std::string fileName;
	//A Blockland .bls import rather than one of our own .lod saves
	bool blockland = false;
	//savedAt falls back to when the file was last written for saves that don't say
	LodSaveInfo info;
};

/*
	Every .lod and .bls save directly in the Saves folder, sorted by name ignoring case, with what each says about itself
	Thumbnails are left out unless withThumbnails is set, since a listing with them reads every file's picture
*/
std::vector<BrickSaveListing> listBrickSaves(bool withThumbnails);

/*
	Draws a picture of a build from above, for saves that don't carry one: each pixel is the color of the highest brick
	under it, shaded by height, on grass. bricks are as found by readLodBricks or readBlocklandBricks
	Returns a JPEG, or "" for no bricks
*/
std::string drawBricksFromAbove(const std::vector<Brick>& bricks);

//The same for every brick a holder has, for a server drawing a picture of its own build, like a preview for a server list
std::string drawBricksFromAbove(const BrickHolder& bricks);

/*
	Old Land of Dran binary save format, written like OldServer/lua/miscFunctions.h's saveBuild except that each brick's
	music, light, and emitter are written as BrickAttachments under a newer version number the old game can't read
	info says who saved it, when, and its picture, or nothing when it's nullptr, see writeLodBricks
	Returns false if the file couldn't be written
*/
bool saveLodBuild(const BrickHolder& bricks, const PrintTypes* prints, const std::string& path, bool omitOwnership, const LodSaveInfo* info = nullptr);

/*
	Loads either version of the old binary format or our own newer one, offset by whole studs/plates
	Special bricks of types we don't have are skipped, and so are the old game's lights and music
	A brick's print is taken by name, from the old game's saves too, and dropped if we don't have that print
	Returns how many bricks were added, or -1 if the file couldn't be read
*/
int loadLodBuild(BrickHolder& bricks, const PrintTypes* prints, const std::string& path, int offsetX, int offsetY, int offsetZ);

//The same from a stream that's already open, like a save a client uploaded, label being what the log calls it
int loadLodBricks(BrickHolder& bricks, const PrintTypes* prints, std::istream& file, const std::string& label, int offsetX, int offsetY, int offsetZ);

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

//How readBlocklandBricks went, everything it skipped, by name and count, for the log
struct BlocklandReadResult
{
	//It got as far as the color palette
	bool valid = false;
	int lights = 0;
	int emitters = 0;
	int music = 0;
	int turnedEmitters = 0;
	int droppedEffects = 0;
	std::map<std::string, int> skippedNames;
	std::map<std::string, int> missingLights;
	std::map<std::string, int> missingEmitters;
	std::map<std::string, int> missingMusic;
	std::map<std::string, int> missingPrints;
};

/*
	Reads a Blockland .bls save, using the file's own color palette, calling found with each brick's description
	Bricks are resolved by name through types, special and unknown bricks are skipped and counted by name
	Brick lights, emitters, and music go through lookup, and any we don't have are skipped and counted by name
*/
BlocklandReadResult readBlocklandBricks(std::istream& file, const BrickTypes& types, const PrintTypes* prints, const BlocklandAttachmentLookup& lookup, const std::function<void(Brick&)>& found);

/*
	Imports a Blockland .bls save with readBlocklandBricks, offset by whole studs/plates, logging everything it skipped
	Returns how many bricks were added, or -1 if the file couldn't be read
*/
int loadBlocklandBuild(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, const std::string& path, const BlocklandAttachmentLookup& lookup, int offsetX = 0, int offsetY = 0, int offsetZ = 0);

//The same from a stream that's already open, like a save a client uploaded, label being what the log calls it
int loadBlocklandBricks(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, std::istream& file, const std::string& label, const BlocklandAttachmentLookup& lookup, int offsetX, int offsetY, int offsetZ);
