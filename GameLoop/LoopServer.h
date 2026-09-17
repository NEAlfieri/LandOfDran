#pragma once

#include "../LandOfDran.h"

#include "../Networking/Server.h"
#include "ServerProgramData.h"
#include "../LuaFunctions/OtherFunctions.h"
#include "../LuaFunctions/Dynamic.h"
#include "../LuaFunctions/Scheduler.h"
#include "../LuaFunctions/ClientLua.h"
#include "../LuaFunctions/BrickLua.h"

//LoopServer is responsible for managing all of this
//Global state that only exists for lua functions to use:
extern ExecutableArguments* LUA_args;
extern ServerProgramData* LUA_pd;

/*
	This is the big bad class that allows us to separate our server hosting loop from
	our client playing loop along with all the variables and structures specific to it
*/
class LoopServer
{
	//Stuff we need to *play* the game, as opposed to host it, except our net interface itself
	ServerProgramData pd;
	//Network connection manager, its methods take ClientProgramData as a parameter, so it's separate
	Server* server = nullptr;
	//Essentially a singleton class that just handles the Lua schedule and cancel functions, just needs to be run in main loop
	LuaScheduler* scheduler = nullptr;

	bool valid = false;

	//Every 40 ticks, or hopefully every second, we will send the duration in MS of the slowest and average frame to clients
	unsigned int slowestTickCounter = 0;
	float totalTicks = 0;
	float totalTicksMS = 0;
	float slowestTickMS = 0;
	float lastSlowestTickMS = 0;

	//SDL_GetTicks of the last WorldStateUpdate packet
	unsigned int lastWorldStateBroadcast = 0;

	//Sends time of day and water level to every client
	void broadcastWorldState();

	//Buoyancy and drag for dynamics in the water, before the physics step, including ones clients simulate themselves
	void applyWaterForces(float deltaT);

	//Splash and ExitWater sounds for dynamics that just went into or came out of the water fast, after the physics step
	void playWaterSounds();

	//Removes emitters whose type's lifetime is up, or whose dynamic or brick is gone
	void updateEmitters();

	//Fires ProjectileHit for and removes projectiles that touched something, and turns the rest the way they're going, after the physics step
	void updateProjectiles();

	//Jet flames on players who are jetting and off everyone else, and flashlights turned to where their players look, after controllers run
	void updatePlayerAbilities();

	//Ends talking for clients whose voice stopped arriving without them saying they let go of push to talk, like a lost last packet or being muted mid-sentence
	void endQuietTalkers();

	//Keeps carried items with their holders, and sends everyone the state of items that changed, after dynamics' creations go out
	void updateItems();

	//Drivers' keys drive their vehicles, drivers who are gone let go, and wheels in the water float, before the physics step
	void updateVehicles(float deltaT);

	//Removes vehicles that flew off, sends wheel states, splashes, and keeps drivers in their seats, after the physics step
	void updateVehiclesAfterStep();

	//Sends the bricks of vehicles made since the last tick, after vehicles' creations go out
	void sendNewVehicleBricks();

	/*
		Works out where each client is watching from and how many bytes of object updates they can be sent, once for
		the whole tick, before the sendRecent calls that spend it. See ObjHolder::sendRecent
	*/
	void startUpdateBudgets();

public:

	//Constructor have any issues?
	bool isValid() const { return valid; }

	//Lot less in this so far given no input or rendering on the server...
	void run(float deltaT, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	//Called only when the program starts up
	LoopServer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);
	//Called only when the program finally shuts down
	~LoopServer();
};
