#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../GameLoop/ServerProgramData.h"

extern ServerProgramData* LUA_pd;

/*
	Slices every brick with any part in the grid voxels from low to high (both inclusive) out of the world into a vehicle, like the old game
	The bricks need exactly one steering wheel and at least one wheel brick, every wheel rolling the way the steering wheel faces
	builder is the client doing it, who gets a ClientSliceBricks event to veto it, or nullptr for Lua
	Returns nullptr and why in failure if it can't, failure is empty if a listener stopped it
*/
std::shared_ptr<Vehicle> sliceVehicle(ClientData* builder, const glm::ivec3& low, const glm::ivec3& high, std::string& failure);

/*
	Puts the client's player (the target of their first controller) in the vehicle's driver's seat for Vehicle::driverSeat, or on one of its passenger seats
	False if someone's in that seat, they're already in a vehicle, their player isn't in the world, or with callEvent, a ClientEnterVehicle listener said no
*/
bool enterVehicle(ClientData& client, const std::shared_ptr<Vehicle>& vehicle, int seat, bool callEvent);

//Lets the client's player out of whatever they drive or ride, just above where they were, firing ClientExitVehicle with callEvent
void exitVehicle(ClientData& client, bool callEvent);

/*
	Moves the client's player from the seat they're in to another of the same vehicle's, the driver's for Vehicle::driverSeat, without
	getting out in between. With callEvent a ClientEnterVehicle listener can say no. False if they aren't in a vehicle, that's the seat
	they're in, or it's taken or broken
*/
bool switchSeat(ClientData& client, int seat, bool callEvent);

//The next free seat after the one the client is in, going driver's seat then passenger seats in order and around again, or the seat they're in if there's no other
int nextFreeSeat(const ClientData& client, const Vehicle& vehicle);

//Lets out its driver, removes the lights, emitters, and music loop it has, and destroys it
void destroyVehicle(std::shared_ptr<Vehicle> vehicle);

//Whether Lua's registerVehicleSpawn registered a spawner by that name, for a Vehicle Spawn brick's wrench dialog to pick from
bool vehicleSpawnExists(const std::string& name);

/*
	Spawns the vehicle a registered spawner makes, by calling its Lua function with a position and the brick spawning it, see registerVehicleSpawn
	The vehicle is turned to drive the way the brick faces and remembers the brick as its spawnBrickID
	nullptr with an error logged if there's no spawner by that name, its function is missing, or it didn't return a vehicle
*/
std::shared_ptr<Vehicle> spawnRegisteredVehicle(const std::string& name, const glm::vec3& position, const Brick* brick);

//Every vehicle's bricks, to a client that just finished loading, after the vehicles themselves
void sendVehicleState(const ServerProgramData* pd, JoinedClient* client);

//Sends the client the wrench dialog for a vehicle, after which one VehicleWrenchSubmit from them can change it, see Networking/PacketsFromClient/Wrench.cpp
void openVehicleWrenchDialog(ClientData& client, const Vehicle& vehicle);

//Loops music from the vehicle for everyone, replacing whatever it played, "" for none
void setVehicleMusic(Vehicle& vehicle, const std::string& name, float volume, float pitch);

//What a new vehicle honks with: the old game's Honk sound if Lua registered it, otherwise nothing
std::string defaultVehicleHorn();

//A headlight for a vehicle that hasn't got one yet, for its wrench dialog to start from: a white spotlight shining the way it drives
BrickAttachments defaultVehicleHeadlight(const Vehicle& vehicle);

/*
	Gives the vehicle the light in settings as its headlight (only its light fields matter, hasLight false takes the headlight away),
	switching it on so the change can be seen, see Vehicle::headlight
*/
void setVehicleHeadlight(Vehicle& vehicle, const BrickAttachments& settings);

//Switches the vehicle's headlight on or off, making or destroying its Light, with LightOn or LightOff from the vehicle for playSounds. On does nothing without a headlight
void setVehicleHeadlightOn(Vehicle& vehicle, bool on, bool playSounds);

/*
	The bytes of a Land of Dran save of the vehicle's bricks as they were before slicing, wheels included,
	with its music on its steering wheel, which becomes the music of a vehicle loaded from it
*/
std::string makeVehicleSaveFile(const Vehicle& vehicle);

/*
	Places the bricks of a save from makeVehicleSaveFile with the middle of their bottom at the grid voxel spot, as a vehicle or as plain bricks
	builder gets a ClientLoadVehicle event to veto it, and owns (and can undo) bricks placed as bricks, nullptr for Lua
	message says how it went, for whoever loaded it, empty if a listener stopped it. vehicle is set to a vehicle it makes
	Returns true if anything was placed
*/
bool loadVehicleSave(ClientData* builder, const std::string& data, const glm::ivec3& spot, bool asVehicle, std::string& message, std::shared_ptr<Vehicle>* vehicle = nullptr);

/*
	Registers global vehicle functions and the client methods for driving
	Returns vehicle methods to be passed to ObjHolder<Vehicle>::makeLuaMetatable, which then deletes the list
	Has to come after registerClientFunctions
*/
luaL_Reg* getVehicleFunctions(lua_State* L);
