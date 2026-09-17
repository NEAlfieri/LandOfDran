#include "VehicleGhost.h"
#include "BrickSaves.h"
#include "../Physics/PhysicsWorld.h"

#include <climits>
#include <cmath>
#include <sstream>

bool VehicleGhost::start(const std::string& saveName, const std::string& bytes, bool placeAsVehicle, const BrickTypes& types, const PrintTypes& prints, std::string& failure)
{
	cancel();

	std::vector<Brick> loaded;
	size_t total = 0;
	std::istringstream stream(bytes, std::ios::binary);
	LodReadResult result = readLodBricks(stream, &types, &prints, [&](Brick& brick)
	{
		total++;
		if (loaded.size() >= maxBricks)
			return;

		brick.name = "";
		brick.attachments = nullptr;
		loaded.push_back(brick);
	});

	if (!result.valid)
	{
		failure = "That isn't a Land of Dran save.";
		return false;
	}

	if (total > maxBricks)
	{
		failure = "That save has more bricks than a vehicle can.";
		return false;
	}

	if (loaded.empty())
	{
		failure = "That save has no bricks this game has.";
		return false;
	}

	glm::ivec3 low(INT_MAX);
	glm::ivec3 high(INT_MIN);
	for (const Brick& brick : loaded)
	{
		low = glm::min(low, glm::ivec3(brick.x, brick.y, brick.z));
		high = glm::max(high, glm::ivec3(brick.x + brick.footprintWidth(), brick.y + brick.height, brick.z + brick.footprintLength()));
	}

	//The same middle loadVehicleSave puts at the spot
	glm::ivec3 anchor((low.x + high.x) / 2, low.y, (low.z + high.z) / 2);
	for (Brick& brick : loaded)
	{
		brick.x -= anchor.x;
		brick.y -= anchor.y;
		brick.z -= anchor.z;
	}

	bricks.swap(loaded);
	name = saveName;
	fileBytes = bytes;
	asVehicle = placeAsVehicle;
	active = true;
	return true;
}

void VehicleGhost::cancel()
{
	active = false;
	hasSpot = false;
	bricks.clear();
	fileBytes.clear();
}

void VehicleGhost::update(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const PhysicsWorld& world, btRigidBody* ignore, const btRigidBody* ignoreAlso)
{
	if (!active || glm::length(cameraDirection) < 0.0001f)
		return;

	glm::vec3 direction = glm::normalize(cameraDirection);
	glm::vec3 end = cameraPosition + direction * reach;
	glm::vec3 place = cameraPosition + direction * inAir;

	btVector3 hitPosition, hitNormal;
	if (world.doRaycast(btVector3(cameraPosition.x, cameraPosition.y, cameraPosition.z), btVector3(end.x, end.y, end.z), ignore, hitPosition, hitNormal, ignoreAlso))
		place = glm::vec3(hitPosition.x(), hitPosition.y(), hitPosition.z());

	//Standing on whatever it hit
	spot = glm::ivec3((int)std::floor(place.x / STUD_SIZE), std::max((int)std::ceil(place.y / PLATE_SIZE - 0.01f), 0), (int)std::floor(place.z / STUD_SIZE));
	hasSpot = true;
}

void VehicleGhost::forEachPlaced(const std::function<void(const Brick&)>& draw) const
{
	if (!hasPlace())
		return;

	Brick placed;
	for (const Brick& brick : bricks)
	{
		placed = brick;
		placed.x += spot.x;
		placed.y += spot.y;
		placed.z += spot.z;
		draw(placed);
	}
}
