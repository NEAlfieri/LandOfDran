#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"

//One row of the player list, as the server last sent it
struct PlayerListEntry
{
	netIDType id = NO_ID;
	bool admin = false;
	unsigned int pingMS = 0;
	std::string name = "";
	//Whatever the server's Lua put there with client:setScoreText, a score to start with
	std::string scoreText = "";
};

/*
	Everyone on the server: their name, the score text the server's Lua gave them, and their ping
	Opened and closed with the Player List key (F2) or from the escape menu

	It's part of the HUD like chat is, so it can stay up while playing without taking the mouse or any keys

	The server sends the whole list whenever anything in it changes, and every couple of seconds for the pings, see Networking/PacketsFromServer/PlayerList.h
*/
class PlayerListWindow : public Window
{
	friend class Window;
	friend class UserInterface;

	std::vector<PlayerListEntry> players;

	//The name we joined with, whose row is drawn lit up. Names are unique on a server
	std::string ownName = "";

	//Centered each time it opens, and left wherever it's dragged after that
	bool justOpened = false;

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	PlayerListWindow();

public:

	//Replaces the whole list with what the server sent
	void setPlayers(std::vector<PlayerListEntry>&& newPlayers);

	//Nobody, for leaving a server
	void clear();

	void setOwnName(const std::string& name) { ownName = name; }

	//Opens it in the middle of the screen
	void show();

	//Opens it if it's closed and closes it if it's open
	void toggle();

	~PlayerListWindow();
};
