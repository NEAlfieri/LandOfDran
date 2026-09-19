#pragma once

#include "../LandOfDran.h"
#include "../External/CRC.h"

/*
	Scripts and add-ons aren't meant to be able to access/edit files outside the LoD directory
	Or certain files that come with LoD itself
*/
bool okayFilePath(const std::string &path);

/*
	False for absolute paths and ones with a .. in them, for file paths the server sends clients
	Unlike okayFilePath, any file name is fine as long as it stays inside the game's folder
*/
bool isPathInsideGameFolder(const std::string &path);

/*
	CRC32 checksum of the file at path
*/
unsigned int getFileChecksum(const char* filePath);

/*
	The same CRC32 of a file already read into memory, for one that arrived over the network
	before anything writes it out, see ContentFiles::takeChunk
*/
unsigned int getBufferChecksum(const char* data, size_t length);

/*
	Gets filesize in bytes, 0 if file invalid
*/
long GetFileSize(const std::string &filename);

/*
	Gets the file name and extension from a full file path
	e.g. "assets/bob/bob.png" as filepath returns "bob.png"
*/
std::string getFileFromPath(const std::string &in);

/*
	Gets the folder from a full file path
	e.g. "assets/bob/bob.png" as filepath returns "assets/bob/"
*/
std::string getFolderFromPath(const std::string &in);

/*
	Adds a suffix to the end of a file name before the extension
	E.g. in = "test/bob.png", suffix = "_a" will return "test/bob_a.png"
*/
std::string addSuffixToFile(std::string in, std::string suffix);

bool doesFileExist(const std::string &filePath);