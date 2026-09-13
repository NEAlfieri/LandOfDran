#pragma once

#include "../LandOfDran.h"

/*
	What state is the client in
	Only valid if there *is* a client running, i.e dedicated is false
	And we're not shutting down or loading, i.e. mainLoopRun is true
*/
enum GameState
{
	NotInGame = 0,			//We're in the main menu or something
	Connecting = 1,			//Attempting to connect to a server
	LoadingTypes = 2,		//We're loading types of objects
	InGame = 3				//We're finished loading and are playing the game
};

/*
	Contains possible flags and data parsed from command line arguments
	Will be expanded in the future
*/
struct ExecutableArguments
{
	//False if the entire program should shut down
	//Not loaded from command line arguments, but frequently passed with them
	bool mainLoopRun = true;

	//Also not loaded from command line arguments
	//Only valid if dedicated is false and mainLoopRun is true
	GameState gameState = GameState::NotInGame;

	//Are we hosting from a headless no UI dedicated servver
	bool dedicated = false;

	//Are we forgoing attempting to place our server on the server list
	bool silentMode = false;

	//Skip the server browser menu and immediately host+join a local single-player game, same as clicking
	//"Start Server". Meant for automated testing (screenshots, etc) where nothing can click through the menu
	bool autoSinglePlayer = false;

	//Parse flags and data from command line arguments on start-up
	ExecutableArguments(int argc, char** argv);
};

