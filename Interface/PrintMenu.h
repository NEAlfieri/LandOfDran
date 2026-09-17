#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"
#include "../Bricks/PrintTypes.h"
#include "../Graphics/Texture.h"

#include <unordered_map>

/*
	Picks the print that goes on a brick's printed faces: every print the server has as a button with its
	picture on it, and Cancel. Clicking one puts it on the brick straight away, and so does hitting the key
	of a print that's a single letter, number, or piece of punctuation

	The server opens it when a player shoots a print brick with the print gun, see Networking/PacketsFromServer/OpenPrintMenu.h
*/
class PrintMenu : public Window
{
	std::shared_ptr<TextureManager> textures;

	//The prints this game loaded, which is where each button's picture comes from, see PrintType::iconPath
	const PrintTypes* prints = nullptr;

	//The brick that was shot, and the print it already wears, which is the button drawn lit up
	netIDType brickID = NO_ID;
	std::string current = "";

	//What the server has to offer, which isn't always what this game loaded, see Simulation::serverPrintNames
	std::vector<std::string> names;

	//One per name, nullptr for a print with no icon or one this game doesn't have
	std::vector<Texture*> icons;

	//Every icon loaded so far by print name, since the same prints come back every time the menu opens
	//A name that has no icon is remembered as nullptr so it isn't looked for again
	std::unordered_map<std::string, Texture*> iconCache;

	//A print was picked, with the one to send
	bool submitted = false;
	std::string chosen = "";

	//Focused the frame after it opens, and centered for a few frames, since its size is only known once the grid has been measured
	bool justOpened = false;
	int framesToCenter = 0;

	//The icon of a print, loading it the first time it's asked for, nullptr for one without a picture
	Texture* iconFor(const std::string& printName);

	//Picks a print and closes, ignoring one the server didn't offer
	void apply(const std::string& printName);

	public:

	//Shows the prints the server has for a brick, replacing anything that was open
	void openFor(netIDType brick, const std::string& currentPrint, const std::vector<std::string>& printNames);

	//True once after a print is picked, with the brick and what to put on it
	bool takeSubmission(netIDType& brick, std::string& printName);

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	PrintMenu(std::shared_ptr<TextureManager> _textures, const PrintTypes* _prints);
	~PrintMenu();
};
