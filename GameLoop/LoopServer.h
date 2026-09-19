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

	//SDL_GetTicks of the last PlayerList packet
	unsigned int lastPlayerListBroadcast = 0;

	//Sends everyone the name, ping, and score text of everyone here, see PlayerListPacket
	void broadcastPlayerList();

	//Buoyancy and drag for dynamics in the water, before the physics step, including ones clients simulate themselves
	void applyWaterForces(float deltaT);

	//Splash and ExitWater sounds for dynamics that just went into or came out of the water fast, after the physics step
	void playWaterSounds();

	//Before the physics step: removes ropes tied to something that's gone, and keeps the rest's constraints in the world, see Rope::updatePhysics
	void updateRopes();

	//Removes emitters whose type's lifetime is up, or whose dynamic or brick is gone
	void updateEmitters();

	//A projectile that touched something during the physics step, and what it touched, see recordProjectileHits
	struct ProjectileHit
	{
		std::weak_ptr<Dynamic> projectile;
		btRigidBody* hit = nullptr;
		btVector3 point;
		//Out of what it touched
		btVector3 normal;
	};

	//Hits seen during this frame's physics step, oldest first, taken by updateProjectiles
	std::vector<ProjectileHit> pendingProjectileHits;

	/*
		Before every physics substep: the wings, throttle and controls of every vehicle that flies, see
		Vehicle::flyStep. Here rather than once a frame because a force applied before a step is applied
		again by each substep it runs, and because flying wants a fresh look at how the plane is turned
	*/
	void flyVehicles(btScalar timeStep);

	/*
		Before every physics substep: sweeps each projectile's box along what the substep is about to move
		it, and stops it on the first thing in the way, noting the hit for updateProjectiles. This is what
		keeps a fast round from skipping through a thin brick. Bullet's own continuous collision would do
		it, but only for convex shapes, and a dynamic's is a compound, so it silently never ran
	*/
	void sweepProjectiles(btScalar timeStep);

	/*
		After every physics substep: notes each projectile that is touching something, and what, for
		updateProjectiles. A projectile is swept along its path each substep so it stops on whatever it
		reaches, but the bounce off it carries it away again in the next substep, so a frame that runs
		several substeps has nothing left to see by the end. Only the first touch of each is kept
	*/
	void recordProjectileHits();

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

	/*
		Called only when the program starts up, or when the graphical client hosts one of its own:
		single player ("Start Server"), and the demo playing behind the main menu, which runs its own
		Lua on its own port so it never collides with a real server, see LoopClient::startMenuDemo

		startScript is the Lua run once everything is registered, port is the UDP port to listen on, and
		loopbackAdmin is whether a client connecting over loopback is let into the eval console without a
		password, which single player's host wants and the menu demo (which nobody plays) does not
	*/
	LoopServer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings,
		const std::string& startScript = "serverstart.lua", int port = DEFAULT_PORT, bool loopbackAdmin = true);
	//Called only when the program finally shuts down
	~LoopServer();
};
