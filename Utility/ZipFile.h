#pragma once

#include "../LandOfDran.h"

/*
	Just enough of the zip format to unpack an add-on that was dropped in as a .zip, see
	LuaFunctions/AddOns.h: the game has no virtual file system, everything a model, texture or
	sound loader opens is a plain path on disk, so a zipped add-on is unpacked into a folder of
	its own and loaded from there like any other

	Deflate and store are the only two methods anything writes in practice, and zip64 (a member
	or an archive over 4 Gb) is refused rather than half understood
*/

/*
	Unpacks every file in zipPath into destination, making the folders it needs as it goes

	If everything in the zip sits under one folder of its own, that folder is left off: an add-on
	zipped as Weapon_Sword/server.lua unpacks the same way as one zipped with server.lua at its root

	False with the reason in problem if the zip can't be read, or if a member's path would land
	outside destination. Whatever had already been written by then is left where it is
*/
bool extractZipFile(const std::string& zipPath, const std::string& destination, std::string& problem);
