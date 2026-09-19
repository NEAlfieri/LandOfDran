#pragma once

#include "../LandOfDran.h"
#include "../Utility/ContentFiles.h"
#include "UserInterface.h"

/*
	The add-on files a server we're joining has that we don't, listed for the player to pick through
	before any of them is sent, like the old game's content window

	Joining downloads whatever is ticked, cancelling leaves the server without downloading anything
	It only ever opens when something really is missing: a file we already have an identical copy of
	is never listed, so joining a server whose add-ons we have shows nothing at all
*/
class ContentDownloadWindow : public Window
{
	friend class UserInterface;

	struct Entry
	{
		ContentFile file;
		bool wanted = true;
	};
	std::vector<Entry> entries;

	//Which server's files these are, for the line at the top
	std::string serverName = "";

	//The player hasn't said yes or no yet, so leaving the window closes the connection rather than hanging the join
	bool awaitingChoice = false;

	//Set when they click the buttons, taken by LoopClient
	bool joinPicked = false;
	bool cancelPicked = false;

	//The list is answered and the files are on their way, so the window shows a progress bar instead
	bool downloading = false;

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	ContentDownloadWindow() = default;

	public:

	//List what the server has that we don't and put it to the player
	void show(const std::string& server, const std::vector<ContentFile>& missing);

	//True once after Join is clicked, with the IDs of the files that were ticked
	bool takeChoice(std::vector<uint16_t>& wanted);

	//True once if they cancelled, or closed the window without answering
	bool takeCancel();

	//Nothing is being asked or downloaded anymore, on leaving a server
	void reset();

	~ContentDownloadWindow() = default;
};
