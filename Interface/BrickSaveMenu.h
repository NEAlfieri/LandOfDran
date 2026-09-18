#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"
#include "../Bricks/BrickSaves.h"

/*
	Saves every brick on the server to a file in our own Saves folder, and uploads one of those for the server to load, opened from the escape menu
	The list is our own folder: every .lod and .bls in it, with a picture, how many bricks it has, and who saved it and when
	Saving asks the server for a copy of the build (see BrickSaveDataPacket), which anyone can do, so a build can be carried to
	another server, where an admin can load it. Only admins can load, since a load can replace the whole build
*/
class BrickSaveMenu : public Window
{
	struct Entry
	{
		BrickSaveListing listing;
		//The picture, 0 for none or one that couldn't be read
		GLuint texture = 0;
	};

	std::vector<Entry> entries;

	//Pictures drawn for saves that don't carry one, by path, so each is only drawn once unless the file changes
	struct DrawnThumbnail
	{
		uintmax_t size = 0;
		std::filesystem::file_time_type written;
		std::string jpeg;
	};
	std::map<std::string, DrawnThumbnail> drawnThumbnails;

	//Our brick types and prints, for reading the bricks of a save to draw its picture
	const BrickTypes* types = nullptr;
	const PrintTypes* prints = nullptr;

	int selected = -1;
	//Set by LoopClient every frame from whether the server gave us admin
	bool admin = false;

	char nameBuffer[65] = "";
	bool clearFirst = false;
	//Where a load goes compared to where the save was made, in studs, plates, and studs
	int offset[3] = { 0, 0, 0 };
	//Only saves whose name or saver has this in it are listed, "" for all of them
	char searchBuffer[65] = "";

	//What LoopClient polls for
	bool saveRequested = false;
	std::string saveName;
	bool loadRequested = false;
	bool loadClear = false;
	glm::ivec3 loadOffset = glm::ivec3(0);
	std::string loadName;

	//Set as the Save or Load button is clicked, and the popup asking to make sure opens on the next frame
	bool askOverwrite = false;
	bool askReplace = false;
	//A save without admin, which has only what our game can see, and whether a save by that name is there to replace after it
	bool askPartial = false;
	bool partialExists = false;

	//Frees every picture
	void clearTextures();

	//A picture for a save that has none of its own, drawn from its bricks, see drawBricksFromAbove
	std::string drawThumbnail(const BrickSaveListing& save);

	//Turns a JPEG into a texture, 0 if it couldn't be read
	GLuint makeTexture(const std::string& jpeg, const std::string& fileName);

	//The file name to put in the name box for an entry, without its extension, since a save is always written as .lod
	static std::string stemOf(const std::string& fileName);

	//"2026-09-18 14:02" in local time, or "" for 0
	static std::string dateText(int64_t unixSeconds);

	//Whether the search box lets an entry into the list
	bool matchesSearch(const Entry& entry) const;

	//Draws one row of the list: its picture, name, and details, picking it when clicked
	void renderEntry(int index, float thumbnailSize);

	//Draws the picture and details of the selected save beside the list
	void renderPreview(float previewSize);

	//Draws the popups asking to make sure before replacing a save or the whole build
	void renderPopups();

	public:

	//Reads the Saves folder again, keeping the same save picked if it's still there
	void refresh();

	//Refreshes and opens it
	void openMenu();

	//Whether the server gave us admin, which is what the load controls need
	void setAdmin(bool isAdmin) { admin = isAdmin; }

	//True once after a save is confirmed, with the .lod file name in Saves to write, which may already exist
	bool takeSaveRequest(std::string& fileName);

	//True once after a load is confirmed, with the file in Saves to upload, whether to take every brick away first, and how far from where it was saved it goes
	bool takeLoadRequest(std::string& fileName, bool& clear, glm::ivec3& offsetStuds);

	//Forgets the list and frees its pictures, for leaving a server
	void clearEntries();

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	BrickSaveMenu(const BrickTypes* _types, const PrintTypes* _prints);
	~BrickSaveMenu();
};
