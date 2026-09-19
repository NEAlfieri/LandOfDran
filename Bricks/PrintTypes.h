#pragma once

#include "../LandOfDran.h"

#include <unordered_map>

class JoinedClient;

/*
	One print that can go on the TEX:PRINT faces of a print brick, from a Blockland style print add-on:
	Assets/brick/prints/Print_<group>[_Default]/prints/<image>.png, or a .webm video that plays there
*/
struct PrintType
{
	//"<group>/<image>", the same names Blockland saves and the old game use, e.g. "Letters/X" or "1x2f_BLPRemote/BLP"
	std::string name = "";

	//The image or .webm itself, only the client loads it
	std::string filePath = "";

	//The smaller picture of it in the add-on's icons folder next to its prints one, for the print menu's buttons
	//"" for one with no icon, like a video, see Interface/PrintMenu.h
	std::string iconPath = "";

	//Whether it's a .webm that plays rather than a still image, see Graphics/PrintVideos.h
	bool video = false;

	//Client only: which layer of the decal array it was loaded into, -1 if it didn't fit, see TextureManager::addDecal
	int decalLayer = -1;
};

/*
	Every print loaded from a folder like Assets/brick/prints
	The server keeps the names to offer in the print menu, the client also loads each one as a decal to draw with
*/
class PrintTypes
{
	//Sorted by name
	std::vector<PrintType> prints;

	//Lowercase name to index in prints
	std::unordered_map<std::string, size_t> byName;

	public:

	/*
		Every print in the game's own folder and in the add-ons folder, which is how a server adds prints:
		a print pack dropped in Add-ons works the same as one in Assets, named the same way by its folders,
		and goes out to clients that haven't got it with addServerFolder

		Ours win over an add-on's where both have a name, and both win over a copy a server sent us
	*/
	void load(const std::string& printsFolder, const std::string& addOnsFolder = "Add-ons");

	size_t size() const { return prints.size(); }

	//nullptr if index is out of range
	const PrintType* get(int index) const { return index < 0 || (size_t)index >= prints.size() ? nullptr : &prints[index]; }

	//Index of a print by name, case-insensitive, -1 if there isn't one
	int find(const std::string& name) const;

	//Client only: the icon of a print by name, "" for one we don't have or one with no icon, see PrintType::iconPath
	std::string getIconPath(const std::string& name) const { const PrintType* print = get(find(name)); return print ? print->iconPath : ""; }

	//Name of a print by index, "" for -1 or an index we don't have
	std::string getName(int index) const { const PrintType* print = get(index); return print ? print->name : ""; }

	//Every print's name, in order, for the print menu's buttons
	std::vector<std::string> getNames() const;

	//Client only, see PrintType::decalLayer
	void setDecalLayer(int index, int layer) { if (index >= 0 && (size_t)index < prints.size()) prints[index].decalLayer = layer; }

	//The layer to draw a brick's printID with, or -1 for a brick with no print or one we couldn't load
	int getDecalLayer(uint16_t printID) const { const PrintType* print = get((int)printID - 1); return print ? print->decalLayer : -1; }

	//Server: the ID and name of every print, to a client that just connected, see BrickPrintTypesPacket
	void sendTypes(JoinedClient const* client) const;
};
