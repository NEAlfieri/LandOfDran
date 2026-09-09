#pragma once

#include "../LandOfDran.h"

#include "../Networking/Client.h"
#include "ClientProgramData.h"
#include "Simulation.h"
#include "../Networking/ClientPacketCreators.h"
#include "../Graphics/BrickRenderer.h"
#include "LoopServer.h"

/*
	This is the big bad class that allows us to separate our client playing loop from
	our server hosting loop along with all the variables and structures specific to it
*/
class LoopClient
{
	//Stuff we need to *play* the game, as opposed to host it, except our net interface itself
	ClientProgramData pd;

	//Anything with state specific to and maintained by the current server we're playing on
	Simulation simulation;

	//Network connection manager, its methods take ClientProgramData as a parameter, so it's separate
	//This could be nullptr so be careful, cmdArgs should be NotInGame if that's the case as well
	Client* client = nullptr;

	//Non-null only for single player: a server hosted in-process that we also connect to as a client
	LoopServer* localServer = nullptr;

	bool valid = false;

	//Per land of dran kino agent special request
	//Technically some UI specific calculations might happen during rendering, oh well
	void renderEverything(float deltaT);

	//Called every frame the program runs. Every frame: in a game, not in a game, loading into a game...
	void handleInput(float deltaT, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	unsigned int lastSentControlledObjects = 0;

	//Send simulation.controlledObjects physics/transform data to server
	void sendControlledObjects();

	//Handle controllers bound to dynamics
	void updateControllers(float deltaT);

	//Gives dynamics the player's own body is actually touching an immediate local visual reaction instead of waiting
	//for the server to notice the same contact and broadcast a correction. Purely a client-side prediction - see
	//Dynamic::predictLocallyUntil for the tradeoffs
	void predictLocalCollisions();

	BrickRenderer testBricks;

public:

	//Constructor have any issues?
	bool isValid() const { return valid; }

	//Clean up performed when leaving a server to return to main menu or joining another server
	void leaveServer(ExecutableArguments& cmdArgs);

	//Joining new server from main menu or another server
	void connectToServer(std::string ip,unsigned int port,std::string userName, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	//Hosts a server in-process on localhost, then connects to it as a client. Used for the "Start Server" button
	void hostSinglePlayer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	//Called every frame the program runs. Every frame: in a game, not in a game, loading into a game...
	void run(float deltaT,ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	/*
		Only called when the program starts initially
		LoopClient is *not* created and destroyed each time you join a server
	*/
	LoopClient(ExecutableArguments & cmdArgs,std::shared_ptr<SettingManager> settings);
	//Only called when the program finally shuts down, *not* called when leaving a server
	~LoopClient();
};
