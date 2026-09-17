#pragma once

#include "Brick.h"
#include "BrickTypes.h"
#include "PrintTypes.h"

class PhysicsWorld;
class btRigidBody;

/*
	Client only: a translucent copy of a saved vehicle's bricks that follows the crosshair until a left click places it, see Interface/VehicleLoader.h
	The server places the file's bricks the same way, with the middle of their bottom at the spot, see loadVehicleSave
*/
class VehicleGhost
{
	bool active = false;

	//The save's name, its bytes to upload, and whether it's placed as a vehicle or as bricks
	std::string name = "";
	std::string fileBytes = "";
	bool asVehicle = true;

	//Our own type IDs, positioned from spot, without names or attachments
	std::vector<Brick> bricks;

	//Grid voxel the middle of their bottom goes at, and whether the crosshair has given it one yet
	glm::ivec3 spot = glm::ivec3(0);
	bool hasSpot = false;

	public:

	//How far from the camera the crosshair can place it, and how far out it goes when that's at nothing, which the server checks
	static constexpr float reach = 100.0f;
	static constexpr float inAir = 15.0f;

	//Most bricks a save can have, Vehicle::maxBricks and Vehicle::maxWheels
	static constexpr size_t maxBricks = 10024;

	//Starts placing a save, false with why if it isn't one or has no bricks of types this game has
	bool start(const std::string& saveName, const std::string& bytes, bool placeAsVehicle, const BrickTypes& types, const PrintTypes& prints, std::string& failure);

	void cancel();

	bool isActive() const { return active; }
	bool hasPlace() const { return active && hasSpot; }
	bool placesAsVehicle() const { return asVehicle; }
	const std::string& getName() const { return name; }
	const std::string& getFileBytes() const { return fileBytes; }
	glm::ivec3 getSpot() const { return spot; }

	//Moves it to where the crosshair points, ignoring up to two bodies like our player and what we're driving
	void update(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const PhysicsWorld& world, btRigidBody* ignore, const btRigidBody* ignoreAlso);

	//Each brick where it's shown
	void forEachPlaced(const std::function<void(const Brick&)>& draw) const;
};
