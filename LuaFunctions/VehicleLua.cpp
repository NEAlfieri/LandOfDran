#include "VehicleLua.h"
#include "ClientLua.h"
#include "SoundLua.h"
#include "EmitterLua.h"
#include "../Bricks/SelectionBox.h"
#include "../Bricks/BrickSaves.h"
#include "../Physics/RadiusImpulse.h"

#include <climits>
#include <cmath>
#include <sstream>

static const glm::vec3 gridScale = glm::vec3(STUD_SIZE, PLATE_SIZE, STUD_SIZE);

//Unturned, the steering wheel brick's driver stands on its -x side and faces +x, so that's the way a vehicle around it drives
static glm::vec3 steeringForward(const Brick& steering)
{
	float angle = steering.getAngle();
	return glm::round(glm::vec3(std::cos(angle), 0.0f, -std::sin(angle)));
}

static VehiclePart partOf(const Brick& brick)
{
	const SpecialBrickType* type = brick.isSpecial() ? LUA_pd->brickTypes.getSpecial(brick.typeID - 1) : nullptr;
	return type ? type->vehiclePart : VehiclePart_None;
}

//Calls an event taking a client, a vehicle, and which seat (nil for the driver's), true if a listener returned nil instead of the vehicle
static bool vehicleEventVetoed(const std::string& event, ClientData& client, const std::shared_ptr<Vehicle>& vehicle, int seat)
{
	lua_State* L = LUA_pd->luaState;
	pushClientLua(L, client.client);
	LUA_pd->vehicles->pushLua(L, vehicle);
	if (seat == Vehicle::driverSeat)
		lua_pushnil(L);
	else
		lua_pushinteger(L, seat);
	LUA_pd->eventManager->callEvent(L, event, 3);
	bool vetoed = lua_gettop(L) == 3 && lua_isnil(L, 2);
	lua_settop(L, 0);
	return vetoed;
}

/*
	Fires VehicleCreated for a vehicle that's finished being made, with who made it (nil for Lua)
	False if a listener removed it
*/
static bool announceVehicle(const std::shared_ptr<Vehicle>& vehicle, ClientData* builder)
{
	if (!LUA_pd->eventManager)
		return true;

	lua_State* L = LUA_pd->luaState;
	LUA_pd->vehicles->pushLua(L, vehicle);
	if (builder && builder->client)
		pushClientLua(L, builder->client);
	else
		lua_pushnil(L);
	LUA_pd->eventManager->callEvent(L, "VehicleCreated", 2);
	lua_settop(L, 0);

	return LUA_pd->vehicles->find(vehicle->getID()) != nullptr;
}

/*
	Makes a vehicle from copies of bricks with world grid positions, which don't have to be in the world
	approve runs once they're known to make a valid vehicle, and can stop it by returning false (setting failure if it wants to say why)
*/
static std::shared_ptr<Vehicle> buildVehicle(ClientData* builder, const std::vector<Brick>& bricks, std::string& failure, const std::function<bool()>& approve)
{
	const Brick* steeringBrick = nullptr;
	std::vector<const Brick*> wheelBricks;
	std::vector<const Brick*> seatBricks;
	std::vector<const Brick*> bodyBricks;

	for (const Brick& brick : bricks)
	{
		VehiclePart part = partOf(brick);

		if (part == VehiclePart_Wheel)
		{
			wheelBricks.push_back(&brick);
			continue;
		}

		if (part == VehiclePart_Steering)
		{
			if (steeringBrick)
			{
				failure = "A vehicle can only have one steering wheel.";
				return nullptr;
			}
			steeringBrick = &brick;
		}

		if (part == VehiclePart_Seat)
			seatBricks.push_back(&brick);

		//The steering wheel and seats are part of the body too
		bodyBricks.push_back(&brick);
	}

	if (!steeringBrick)
	{
		failure = "A vehicle needs a steering wheel.";
		return nullptr;
	}

	if (wheelBricks.empty())
	{
		failure = "A vehicle needs at least one wheel.";
		return nullptr;
	}

	if (wheelBricks.size() > Vehicle::maxWheels)
	{
		failure = "A vehicle can have at most " + std::to_string(Vehicle::maxWheels) + " wheels.";
		return nullptr;
	}

	if (seatBricks.size() > Vehicle::maxSeats)
	{
		failure = "A vehicle can have at most " + std::to_string(Vehicle::maxSeats) + " seats.";
		return nullptr;
	}

	if (bodyBricks.size() > Vehicle::maxBricks)
	{
		failure = "A vehicle can have at most " + std::to_string(Vehicle::maxBricks) + " bricks.";
		return nullptr;
	}

	glm::vec3 forward = steeringForward(*steeringBrick);
	bool drivesAlongX = std::abs(forward.x) > 0.5f;

	//A wheel rolls along its longer side
	for (const Brick* wheel : wheelBricks)
	{
		if ((drivesAlongX && wheel->footprintLength() > wheel->footprintWidth()) || (!drivesAlongX && wheel->footprintWidth() > wheel->footprintLength()))
		{
			failure = "Every wheel has to roll the way the steering wheel faces.";
			return nullptr;
		}
	}

	glm::ivec3 gridMin(INT_MAX);
	glm::ivec3 gridMax(INT_MIN);
	for (const Brick* brick : bodyBricks)
	{
		gridMin = glm::min(gridMin, glm::ivec3(brick->x, brick->y, brick->z));
		gridMax = glm::max(gridMax, glm::ivec3(brick->x + brick->footprintWidth(), brick->y + brick->height, brick->z + brick->footprintLength()));
	}

	glm::vec3 worldMin = glm::vec3(gridMin) * gridScale;
	glm::vec3 worldMax = glm::vec3(gridMax) * gridScale;
	if (glm::any(glm::greaterThan(worldMax - worldMin, glm::vec3(Vehicle::maxSize))))
	{
		failure = "A vehicle can be at most " + std::to_string((int)Vehicle::maxSize) + " studs long, wide, or tall.";
		return nullptr;
	}

	if (approve && !approve())
		return nullptr;

	SteeringSettings steering;
	if (steeringBrick->attachments && steeringBrick->attachments->hasSteering)
		steering = steeringBrick->attachments->steering;
	steering.clampValues();

	auto wheelSettingsOf = [](const Brick* wheel)
	{
		WheelSettings settings;
		if (wheel->attachments && wheel->attachments->hasWheel)
			settings = wheel->attachments->wheel;
		settings.clampValues();
		return settings;
	};

	//Like the old game, the body turns around a point near the wheels' tops unless the steering wheel asks for the middle of its bricks, which flips more easily
	glm::vec3 middle = (worldMin + worldMax) * 0.5f;
	float heightCorrection = 0.0f;
	if (!steering.realisticCenterOfMass)
	{
		float brickY = 0.0f;
		for (const Brick* brick : bodyBricks)
			brickY += brick->getWorldCenter().y;
		brickY /= (float)bodyBricks.size();

		float wheelY = 0.0f;
		for (const Brick* wheel : wheelBricks)
			wheelY += wheel->getWorldCenter().y + wheelSettingsOf(wheel).suspensionLength;
		wheelY /= (float)wheelBricks.size();

		heightCorrection = wheelY - brickY + 0.4f;
	}
	glm::vec3 origin = middle + glm::vec3(0, heightCorrection, 0);

	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->create();
	vehicle->builderID = builder && builder->client ? builder->client->getNetId() : NO_ID;
	vehicle->forward = forward;
	vehicle->steering = steering;
	vehicle->brickOffset = worldMin - origin;
	//Standing behind the steering wheel, a little below its middle, like the old game
	vehicle->seat = steeringBrick->getWorldCenter() - origin + glm::vec3(0, -1, 0) - forward * 2.0f;

	//Passengers stand on top of seat bricks
	for (const Brick* seatBrick : seatBricks)
	{
		PassengerSeat passengerSeat;
		passengerSeat.top = seatBrick->getWorldCenter() + glm::vec3(0, seatBrick->height * PLATE_SIZE * 0.5f, 0) - origin;
		vehicle->passengerSeats.push_back(passengerSeat);
	}

	int dirtType = findEmitterTypeIndex(LUA_pd->vehicleDirtEmitter);
	vehicle->dirtEmitterType = dirtType == -1 ? Vehicle::noEmitterType : (uint16_t)dirtType;

	//Its own copies of everything, the world's bricks get their attachments cleared as they're removed
	auto keep = [&gridMin](const Brick& brick)
	{
		Brick copy = brick;
		copy.x -= gridMin.x;
		copy.y -= gridMin.y;
		copy.z -= gridMin.z;
		copy.body = nullptr;
		copy.netId = 0;
		copy.holderIndex = 0;
		copy.renderIndex = 0;
		if (copy.attachments)
		{
			copy.attachments = std::make_shared<BrickAttachments>(*copy.attachments);
			copy.attachments->musicLoopID = NO_ID;
			copy.attachments->lightID = NO_ID;
			copy.attachments->emitterID = NO_ID;
		}
		return copy;
	};

	for (const Brick* brick : bodyBricks)
		vehicle->bricks.push_back(keep(*brick));

	for (const Brick* brick : wheelBricks)
	{
		vehicle->wheelBricks.push_back(keep(*brick));

		VehicleWheel wheel;
		wheel.settings = wheelSettingsOf(brick);
		wheel.radius = brick->height * PLATE_SIZE * 0.5f;
		wheel.width = std::min(brick->footprintWidth(), brick->footprintLength()) * STUD_SIZE;
		wheel.connection = brick->getWorldCenter() + glm::vec3(0, wheel.settings.suspensionLength, 0) - origin;
		vehicle->wheels.push_back(wheel);
	}

	if (!vehicle->buildServer(&LUA_pd->brickTypes, g2b3(origin)))
	{
		LUA_pd->vehicles->destroy(vehicle);
		failure = "None of those bricks are solid.";
		return nullptr;
	}

	//Lights and emitters on its bricks come along with it, and its copies of those bricks know which are theirs, for when they break off
	for (size_t a = 0; a < bodyBricks.size(); a++)
	{
		const Brick* brick = bodyBricks[a];
		if (!brick->attachments)
			continue;

		const BrickAttachments& settings = *brick->attachments;
		BrickAttachments* kept = vehicle->bricks[a].attachments.get();
		glm::vec3 center = brick->getWorldCenter();

		if (settings.hasLight && LUA_pd->lights)
		{
			std::shared_ptr<Light> light = LUA_pd->lights->create(center, settings.lightColor, settings.lightBrightness, settings.lightFlicker, settings.lightCoronaWidth);
			light->setConeAngle(settings.lightConeAngle);
			light->setDirection(settings.lightDirection);
			light->setSpin(settings.lightSpin);
			light->setBlink(settings.lightBlinkSpeed, settings.lightBlinkStrength);
			light->attachToVehicle(vehicle, center + settings.lightOffset - origin);
			vehicle->lightIDs.push_back(light->getID());
			if (kept)
				kept->lightID = light->getID();
		}

		if (!settings.emitterName.empty())
		{
			if (std::shared_ptr<Emitter> emitter = spawnEmitterAt(settings.emitterName, center))
			{
				emitter->attachToVehicle(vehicle, center - origin);
				vehicle->emitterIDs.push_back(emitter->getID());
				if (kept)
					kept->emitterID = emitter->getID();
			}
		}
	}

	//Music on the steering wheel becomes the vehicle's, which is also how a saved vehicle keeps its music
	if (steeringBrick->attachments && !steeringBrick->attachments->musicName.empty())
	{
		const BrickAttachments& music = *steeringBrick->attachments;
		setVehicleMusic(*vehicle, music.musicName, music.musicVolume, music.musicPitch);
	}

	LUA_pd->vehiclesAwaitingBricks.push_back(vehicle);

	info((builder && builder->client ? builder->client->name : std::string("Lua")) + " made vehicle " + std::to_string(vehicle->getID()) + " out of " +
		std::to_string(bodyBricks.size()) + " bricks and " + std::to_string(wheelBricks.size()) + " wheels");

	return vehicle;
}

std::shared_ptr<Vehicle> sliceVehicle(ClientData* builder, const glm::ivec3& low, const glm::ivec3& high, std::string& failure)
{
	scope("sliceVehicle");

	if (!LUA_pd || !LUA_pd->vehicles || !LUA_pd->bricks)
	{
		failure = "Vehicles can't be made right now.";
		return nullptr;
	}

	glm::ivec3 size = high - low + glm::ivec3(1);
	if (glm::any(glm::lessThan(size, glm::ivec3(1))) || size.x > SelectionBox::maxStuds || size.y > SelectionBox::maxPlates || size.z > SelectionBox::maxStuds)
	{
		failure = "That box is too big to slice.";
		return nullptr;
	}

	std::vector<Brick> found;
	std::vector<netIDType> ids;
	LUA_pd->bricks->forEachInBox(low, high, [&](Brick* brick)
	{
		found.push_back(*brick);
		ids.push_back(brick->netId);
	});

	if (found.empty())
	{
		failure = "There are no bricks in that box.";
		return nullptr;
	}

	//Listeners can remove bricks, so they're found again by ID afterwards
	auto approve = [&]() -> bool
	{
		if (!builder || !builder->client || !LUA_pd->eventManager)
			return true;

		lua_State* L = LUA_pd->luaState;
		pushClientLua(L, builder->client);
		lua_pushinteger(L, (lua_Integer)found.size());
		LUA_pd->eventManager->callEvent(L, "ClientSliceBricks", 2);
		bool vetoed = lua_gettop(L) == 2 && lua_isnil(L, 2);
		lua_settop(L, 0);

		if (vetoed)
		{
			failure = "";
			return false;
		}

		for (netIDType id : ids)
		{
			if (!LUA_pd->bricks->find(id))
			{
				failure = "Some of those bricks were removed.";
				return false;
			}
		}

		return true;
	};

	std::shared_ptr<Vehicle> vehicle = buildVehicle(builder, found, failure, approve);
	if (!vehicle)
		return nullptr;

	//Its bricks leave the world, taking their own lights, emitters, and music with them
	for (netIDType id : ids)
	{
		if (Brick* brick = LUA_pd->bricks->find(id))
			LUA_pd->bricks->remove(brick);
	}

	if (!announceVehicle(vehicle, builder))
	{
		failure = "";
		return nullptr;
	}

	return vehicle;
}

bool enterVehicle(ClientData& client, const std::shared_ptr<Vehicle>& vehicle, int seat, bool callEvent)
{
	if (!LUA_pd || !vehicle || !vehicle->body || !client.vehicle.expired())
		return false;

	bool driving = seat == Vehicle::driverSeat;
	if (!driving && (seat < 0 || seat >= (int)vehicle->passengerSeats.size()))
		return false;

	auto seatTaken = [&]() { return driving ? vehicle->driverID != NO_ID : (vehicle->passengerSeats[seat].riderID != NO_ID || vehicle->passengerSeats[seat].broken); };
	if (seatTaken())
		return false;

	std::shared_ptr<Dynamic> player = client.controllers.empty() ? nullptr : client.controllers[0].target.lock();
	if (!player || !player->isInWorld() || player->getKind() != DynamicKind_Plain)
		return false;

	if (callEvent && client.client)
	{
		if (vehicleEventVetoed("ClientEnterVehicle", client, vehicle, seat))
			return false;

		//Listeners can do plenty in the meantime
		if (seatTaken() || !client.vehicle.expired() || !player->isInWorld() || !LUA_pd->vehicles->find(vehicle->getID()))
			return false;
	}

	player->removeFromWorld();

	if (driving)
	{
		player->body->setWorldTransform(vehicle->getSeatTransform(false));
		vehicle->driverID = player->getID();
		vehicle->driver = client.me;
	}
	else
	{
		player->body->setWorldTransform(vehicle->getPassengerTransform(seat, *player, player->lookDirection, false));
		vehicle->passengerSeats[seat].riderID = player->getID();
		vehicle->passengerSeats[seat].rider = client.me;
	}

	client.vehicle = vehicle;
	client.vehicleSeat = seat;
	vehicle->body->activate();

	if (LUA_server)
		LUA_server->broadcast(vehicle->makeDriverPacket(), OtherReliable);

	//The old game's, if Lua registered it
	playSoundOn("PlayerMount", player, 1.0f, 1.0f);
	return true;
}

//Puts a player back into the world standing upright, going as fast as the vehicle was: a driver just above the driver's seat, a passenger just above where they stood
static void letOut(const Vehicle& vehicle, const std::shared_ptr<Dynamic>& player, int seatIndex)
{
	if (!player || player->isInWorld())
		return;

	bool driving = seatIndex == Vehicle::driverSeat;
	btTransform seat = driving ? vehicle.getSeatTransform(false) : player->body->getWorldTransform();
	btVector3 ahead = seat.getBasis() * btVector3(0, 0, -1);
	btTransform out(btQuaternion(btVector3(0, 1, 0), std::atan2(-ahead.x(), -ahead.z())), seat.getOrigin() + btVector3(0, driving ? vehicle.exitHeight : Vehicle::passengerExitLift, 0));

	player->returnToWorld(out);
	if (vehicle.body)
		player->setVelocity(vehicle.body->getLinearVelocity());

	//A client moving its own player still takes this
	player->forcePlayerUpdate = true;
}

void exitVehicle(ClientData& client, bool callEvent)
{
	std::shared_ptr<Vehicle> vehicle = client.vehicle.lock();
	int seat = client.vehicleSeat;
	client.vehicle.reset();
	client.vehicleSeat = Vehicle::driverSeat;
	if (!vehicle || !LUA_pd)
		return;

	netIDType playerID = NO_ID;
	if (seat == Vehicle::driverSeat)
	{
		playerID = vehicle->driverID;
		vehicle->driverID = NO_ID;
		vehicle->driver.reset();
		vehicle->park();
	}
	else if (seat >= 0 && seat < (int)vehicle->passengerSeats.size())
	{
		PassengerSeat& passengerSeat = vehicle->passengerSeats[seat];
		playerID = passengerSeat.riderID;
		passengerSeat.riderID = NO_ID;
		passengerSeat.rider.reset();
	}
	else
		return;

	std::shared_ptr<Dynamic> player = playerID != NO_ID ? LUA_pd->dynamics->find(playerID) : nullptr;

	if (LUA_server)
		LUA_server->broadcast(vehicle->makeDriverPacket(), OtherReliable);

	letOut(*vehicle, player, seat);

	if (callEvent && client.client)
		vehicleEventVetoed("ClientExitVehicle", client, vehicle, seat);
}

void destroyVehicle(std::shared_ptr<Vehicle> vehicle)
{
	if (!vehicle || !LUA_pd)
		return;

	if (std::shared_ptr<ClientData> driver = vehicle->driver.lock())
		exitVehicle(*driver, false);
	else if (vehicle->driverID != NO_ID)
	{
		letOut(*vehicle, LUA_pd->dynamics->find(vehicle->driverID), Vehicle::driverSeat);
		vehicle->driverID = NO_ID;
	}

	for (int a = 0; a < (int)vehicle->passengerSeats.size(); a++)
	{
		PassengerSeat& passengerSeat = vehicle->passengerSeats[a];
		std::shared_ptr<ClientData> rider = passengerSeat.rider.lock();
		if (rider && rider->vehicle.lock() == vehicle && rider->vehicleSeat == a)
			exitVehicle(*rider, false);
		else if (passengerSeat.riderID != NO_ID)
		{
			letOut(*vehicle, LUA_pd->dynamics->find(passengerSeat.riderID), a);
			passengerSeat.riderID = NO_ID;
		}
	}

	if (vehicle->musicLoopID != NO_ID)
		stopSoundLoopByID(vehicle->musicLoopID);
	vehicle->musicLoopID = NO_ID;

	for (netIDType id : vehicle->lightIDs)
	{
		if (std::shared_ptr<Light> light = LUA_pd->lights->find(id))
		{
			if (light->getVehicleID() == vehicle->getID())
				LUA_pd->lights->destroy(light);
		}
	}

	for (netIDType id : vehicle->emitterIDs)
	{
		if (std::shared_ptr<Emitter> emitter = LUA_pd->emitters->find(id))
		{
			if (emitter->getAttachKind() == EmitterAttachVehicle && emitter->getDynamicID() == vehicle->getID())
				LUA_pd->emitters->destroy(emitter);
		}
	}

	LUA_pd->vehicles->destroy(vehicle);
}

void sendVehicleState(const ServerProgramData* pd, JoinedClient* client)
{
	for (size_t a = 0; a < pd->vehicles->size(); a++)
	{
		for (ENetPacket* packet : pd->vehicles->get(a)->makeBrickPackets())
			client->send(packet, OtherReliable);
	}
}

void openVehicleWrenchDialog(ClientData& client, const Vehicle& vehicle)
{
	if (!client.client)
		return;

	/*
		1 byte		-	packet type
		4 bytes		-	vehicle net ID
		4 bytes		-	how many bricks it has
		The rest	-	BrickAttachments::write with just its music
	*/
	netIDType id = vehicle.getID();
	uint32_t brickCount = (uint32_t)vehicle.bricks.size();

	std::vector<unsigned char> bytes(1 + sizeof(netIDType) + sizeof(uint32_t));
	bytes[0] = OpenVehicleWrench;
	memcpy(bytes.data() + 1, &id, sizeof(netIDType));
	memcpy(bytes.data() + 1 + sizeof(netIDType), &brickCount, sizeof(uint32_t));

	BrickAttachments music;
	music.musicName = vehicle.musicName;
	music.musicVolume = vehicle.musicVolume;
	music.musicPitch = vehicle.musicPitch;
	music.write(bytes);

	client.client->send(enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable)), OtherReliable);
	client.wrenchedVehicleID = id;
}

void setVehicleMusic(Vehicle& vehicle, const std::string& name, float volume, float pitch)
{
	if (vehicle.musicLoopID != NO_ID)
		stopSoundLoopByID(vehicle.musicLoopID);
	vehicle.musicLoopID = NO_ID;

	vehicle.musicName = name.substr(0, BrickAttachments::maxNameLength);
	vehicle.musicVolume = std::clamp(std::isfinite(volume) ? volume : 1.0f, 0.0f, 1.0f);
	vehicle.musicPitch = std::clamp(std::isfinite(pitch) ? pitch : 1.0f, 0.05f, 10.0f);

	if (vehicle.musicName.empty())
		return;

	unsigned int loopID;
	std::shared_ptr<Vehicle> shared = std::static_pointer_cast<Vehicle>(vehicle.getMe());
	if (shared && startSoundLoopOnVehicle(vehicle.musicName, shared, vehicle.musicPitch, vehicle.musicVolume, loopID))
		vehicle.musicLoopID = loopID;
}

std::string makeVehicleSaveFile(const Vehicle& vehicle)
{
	std::vector<Brick> copies = vehicle.bricks;
	copies.insert(copies.end(), vehicle.wheelBricks.begin(), vehicle.wheelBricks.end());

	for (Brick& brick : copies)
	{
		if (partOf(brick) != VehiclePart_Steering)
			continue;

		auto settings = brick.attachments ? std::make_shared<BrickAttachments>(*brick.attachments) : std::make_shared<BrickAttachments>();
		settings->musicName = vehicle.musicName;
		settings->musicVolume = vehicle.musicVolume;
		settings->musicPitch = vehicle.musicPitch;
		brick.attachments = settings->isEmpty() ? nullptr : settings;
	}

	std::vector<const Brick*> pointers;
	for (const Brick& brick : copies)
		pointers.push_back(&brick);

	std::ostringstream stream(std::ios::binary);
	writeLodBricks(stream, pointers, &LUA_pd->brickTypes, &LUA_pd->prints, true);
	return stream.str();
}

bool loadVehicleSave(ClientData* builder, const std::string& data, const glm::ivec3& spot, bool asVehicle, std::string& message, std::shared_ptr<Vehicle>* made)
{
	scope("loadVehicleSave");

	if (!LUA_pd || !LUA_pd->vehicles || !LUA_pd->bricks)
	{
		message = "Vehicles can't be loaded right now.";
		return false;
	}

	static constexpr size_t mostBricks = Vehicle::maxBricks + Vehicle::maxWheels;

	std::vector<Brick> loaded;
	size_t total = 0;
	std::istringstream stream(data, std::ios::binary);
	LodReadResult result = readLodBricks(stream, &LUA_pd->brickTypes, &LUA_pd->prints, [&](Brick& brick)
	{
		total++;
		if (loaded.size() < mostBricks)
			loaded.push_back(brick);
	});

	if (!result.valid)
	{
		message = "That isn't a Land of Dran save.";
		return false;
	}

	if (total > mostBricks)
	{
		message = "That save has more bricks than a vehicle can.";
		return false;
	}

	if (loaded.empty())
	{
		message = "That save has no bricks this server can load.";
		return false;
	}

	glm::ivec3 low(INT_MAX);
	glm::ivec3 high(INT_MIN);
	for (const Brick& brick : loaded)
	{
		low = glm::min(low, glm::ivec3(brick.x, brick.y, brick.z));
		high = glm::max(high, glm::ivec3(brick.x + brick.footprintWidth(), brick.y + brick.height, brick.z + brick.footprintLength()));
	}

	glm::ivec3 offset(spot.x - (low.x + high.x) / 2, spot.y - low.y, spot.z - (low.z + high.z) / 2);
	for (Brick& brick : loaded)
	{
		brick.x += offset.x;
		brick.y += offset.y;
		brick.z += offset.z;
		brick.ownerID = builder && builder->client ? (int)builder->client->getNetId() : -1;
	}

	if (builder && builder->client && LUA_pd->eventManager)
	{
		lua_State* L = LUA_pd->luaState;
		pushClientLua(L, builder->client);
		lua_pushinteger(L, (lua_Integer)loaded.size());
		lua_pushboolean(L, asVehicle);
		LUA_pd->eventManager->callEvent(L, "ClientLoadVehicle", 3);
		bool vetoed = lua_gettop(L) == 3 && lua_isnil(L, 2);
		lua_settop(L, 0);

		if (vetoed)
		{
			message = "";
			return false;
		}
	}

	std::string missing = result.skippedSpecial > 0 ? " " + std::to_string(result.skippedSpecial) + " bricks of types this server doesn't have were left out." : "";

	if (asVehicle)
	{
		std::string failure;
		std::shared_ptr<Vehicle> vehicle = buildVehicle(builder, loaded, failure, nullptr);
		if (!vehicle)
		{
			message = failure;
			return false;
		}

		size_t brickCount = vehicle->bricks.size();
		if (!announceVehicle(vehicle, builder))
		{
			message = "";
			return false;
		}

		if (made)
			*made = vehicle;
		message = "Loaded a vehicle with " + std::to_string(brickCount) + " bricks." + missing;
		return true;
	}

	int placed = 0;
	int overlapped = 0;
	for (const Brick& brick : loaded)
	{
		if (Brick* added = LUA_pd->bricks->add(brick))
		{
			placed++;
			if (builder)
				builder->plantedBricks.push_back(added->netId);
		}
		else
			overlapped++;
	}

	message = "Loaded " + std::to_string(placed) + " bricks." + (overlapped > 0 ? " " + std::to_string(overlapped) + " were in the way of other bricks." : "") + missing;
	return placed > 0;
}

//Methods are called as vehicle:method(...), so the vehicle is always argument 1
static std::shared_ptr<Vehicle> vehicleArgument(lua_State* L, const std::string& usage)
{
	lua_pushvalue(L, 1);
	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->popLua(L);
	if (!vehicle)
		error("Invalid vehicle passed to " + usage + ", was it removed already?");
	return vehicle;
}

//Reads count finite numbers starting at stack index first, logging usage and returning false if any aren't
static bool readNumbers(lua_State* L, int first, int count, float* out, const std::string& usage)
{
	for (int a = 0; a < count; a++)
	{
		if (!lua_isnumber(L, first + a) || !std::isfinite((float)lua_tonumber(L, first + a)))
		{
			error("Argument " + std::to_string(first + a - 1) + " isn't a finite number: " + usage);
			return false;
		}
		out[a] = (float)lua_tonumber(L, first + a);
	}
	return true;
}

//A method with just the vehicle, and a usage message if it's called with anything else
static std::shared_ptr<Vehicle> plainVehicleMethod(lua_State* L, const std::string& usage)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected no arguments " + usage);
		lua_settop(L, 0);
		return nullptr;
	}
	return vehicleArgument(L, usage);
}

//A point's distance to a box, 0 inside it
static float distanceToBox(const glm::vec3& point, const glm::vec3& low, const glm::vec3& high)
{
	return glm::distance(point, glm::clamp(point, low, high));
}

/*
	Breaks off a destructable vehicle's bricks near enough to an impulse, as if each were hammered, see Physics/RadiusImpulse.h
	Never its steering wheel, which keeps it a vehicle. Lights and emitters on broken bricks go with them, and anyone on a broken seat gets off
	Returns how many broke
*/
static int breakVehicleBricks(const std::shared_ptr<Vehicle>& vehicle, const glm::vec3& center, float strength, float radius)
{
	const btTransform& transform = vehicle->body->getWorldTransform();
	btQuaternion turn = transform.getRotation();
	glm::quat rotation(turn.w(), turn.x(), turn.y(), turn.z());

	//Where the impulse is in the space its bricks' positions are in
	glm::vec3 local = glm::inverse(rotation) * (center - b2g3(transform.getOrigin())) - vehicle->brickOffset;

	std::vector<uint16_t> broken;
	for (size_t a = 0; a < vehicle->bricks.size(); a++)
	{
		const Brick& brick = vehicle->bricks[a];
		if (partOf(brick) == VehiclePart_Steering)
			continue;

		glm::vec3 size(brick.footprintWidth() * STUD_SIZE, brick.height * PLATE_SIZE, brick.footprintLength() * STUD_SIZE);
		glm::vec3 low = brick.getWorldCenter() - size * 0.5f;
		float distance = distanceToBox(local, low, low + size);
		if (distance <= radius && impulseBreaksBrick(strength, distance, size.x * size.y * size.z))
			broken.push_back((uint16_t)a);
	}

	if (broken.empty())
		return 0;

	//Riders on broken seats get off first, ClientExitVehicle listeners can do anything, including removing the vehicle
	for (uint16_t index : broken)
	{
		const Brick& brick = vehicle->bricks[index];
		if (partOf(brick) != VehiclePart_Seat)
			continue;

		glm::vec3 top = vehicle->brickOffset + brick.getWorldCenter() + glm::vec3(0, brick.height * PLATE_SIZE * 0.5f, 0);
		for (int s = 0; s < (int)vehicle->passengerSeats.size(); s++)
		{
			PassengerSeat& seat = vehicle->passengerSeats[s];
			if (seat.broken || glm::distance(seat.top, top) > 0.01f)
				continue;

			seat.broken = true;
			std::shared_ptr<ClientData> rider = seat.rider.lock();
			if (rider && rider->vehicle.lock() == vehicle && rider->vehicleSeat == s)
				exitVehicle(*rider, true);
			break;
		}
	}

	if (!LUA_pd->vehicles->find(vehicle->getID()) || !vehicle->body)
		return 0;

	for (uint16_t index : broken)
	{
		const std::shared_ptr<BrickAttachments>& attachments = vehicle->bricks[index].attachments;
		if (!attachments)
			continue;

		if (attachments->lightID != NO_ID)
		{
			if (std::shared_ptr<Light> light = LUA_pd->lights->find(attachments->lightID))
				LUA_pd->lights->destroy(light);
		}

		if (attachments->emitterID != NO_ID)
		{
			if (std::shared_ptr<Emitter> emitter = LUA_pd->emitters->find(attachments->emitterID))
				LUA_pd->emitters->destroy(emitter);
		}
	}

	uint16_t before = (uint16_t)vehicle->bricks.size();
	vehicle->removeBricks(broken, &LUA_pd->brickTypes);

	if (LUA_server)
		LUA_server->broadcast(vehicle->makeBricksBrokenPacket(before, broken, center, strength), OtherReliable);

	return (int)broken.size();
}

static int LUA_radiusImpulse(lua_State* L)
{
	scope("(LUA) radiusImpulse");

	const std::string usage = "radiusImpulse(x, y, z, strength)";
	float values[4];
	if (lua_gettop(L) != 4 || !readNumbers(L, 1, 4, values, usage))
	{
		if (lua_gettop(L) != 4)
			error("Expected 4 arguments " + usage);
		lua_settop(L, 0);
		return 0;
	}
	lua_settop(L, 0);

	glm::vec3 center(values[0], values[1], values[2]);
	float strength = values[3];
	float radius = impulseRadius(strength);

	int pushed = 0;
	int brokenBricks = 0;

	//Copies of the lists, since breaking seats fires events that can remove things
	std::vector<std::shared_ptr<Dynamic>> dynamics;
	for (unsigned int a = 0; a < LUA_pd->dynamics->size(); a++)
		dynamics.push_back(LUA_pd->dynamics->get(a));

	std::vector<std::shared_ptr<Vehicle>> vehicles;
	for (unsigned int a = 0; a < LUA_pd->vehicles->size(); a++)
		vehicles.push_back(LUA_pd->vehicles->get(a));

	for (const std::shared_ptr<Vehicle>& vehicle : vehicles)
	{
		if (!vehicle->body || !LUA_pd->vehicles->find(vehicle->getID()))
			continue;

		btVector3 low, high;
		vehicle->body->getAabb(low, high);
		float distance = distanceToBox(center, b2g3(low), b2g3(high));
		if (distance > radius)
			continue;

		if (vehicle->destructable)
		{
			brokenBricks += breakVehicleBricks(vehicle, center, strength, radius);
			if (!vehicle->body || !LUA_pd->vehicles->find(vehicle->getID()))
				continue;
		}

		glm::vec3 direction = impulseDirection(center, b2g3(vehicle->body->getCenterOfMassPosition()));
		vehicle->body->applyCentralImpulse(g2b3(direction * strength * impulseFalloff(distance, radius)));
		vehicle->body->activate();
		pushed++;
	}

	for (const std::shared_ptr<Dynamic>& dynamic : dynamics)
	{
		//Carried items and players riding vehicles go wherever what they're on goes
		if (!LUA_pd->dynamics->find(dynamic->getID()) || !dynamic->isInWorld() || dynamic->isSnappedToCursor() || dynamic->body->getInvMass() <= 0)
			continue;

		btVector3 low, high;
		dynamic->body->getAabb(low, high);
		float distance = distanceToBox(center, b2g3(low), b2g3(high));
		if (distance > radius)
			continue;

		//As a velocity, so players moving themselves get it too, like dynamic:setVelocity
		glm::vec3 direction = impulseDirection(center, b2g3(dynamic->body->getCenterOfMassPosition()));
		glm::vec3 change = direction * strength * impulseFalloff(distance, radius) * dynamic->body->getInvMass();
		dynamic->setVelocity(dynamic->getVelocity() + g2b3(change));
		dynamic->forcePlayerUpdate = true;
		dynamic->activate();
		pushed++;
	}

	lua_pushinteger(L, pushed);
	lua_pushinteger(L, brokenBricks);
	return 2;
}

static int LUA_sliceBricks(lua_State* L)
{
	scope("(LUA) sliceBricks");

	const std::string usage = "sliceBricks(x1, y1, z1, x2, y2, z2)";
	float values[6];
	if (lua_gettop(L) != 6 || !readNumbers(L, 1, 6, values, usage))
	{
		if (lua_gettop(L) != 6)
			error("Expected 6 arguments " + usage);
		lua_settop(L, 0);
		return 0;
	}
	lua_settop(L, 0);

	glm::ivec3 a((int)std::floor(values[0]), (int)std::floor(values[1]), (int)std::floor(values[2]));
	glm::ivec3 b((int)std::floor(values[3]), (int)std::floor(values[4]), (int)std::floor(values[5]));

	std::string failure;
	std::shared_ptr<Vehicle> vehicle = sliceVehicle(nullptr, glm::min(a, b), glm::max(a, b), failure);
	if (!vehicle)
	{
		lua_pushnil(L);
		lua_pushstring(L, failure.c_str());
		return 2;
	}

	LUA_pd->vehicles->pushLua(L, vehicle);
	return 1;
}

static int LUA_loadVehicleFile(lua_State* L)
{
	scope("(LUA) loadVehicleFile");

	const std::string usage = "loadVehicleFile(fileName, x, y, z[, asBricks])";
	int args = lua_gettop(L);
	float spot[3];
	if ((args != 4 && args != 5) || lua_type(L, 1) != LUA_TSTRING || !readNumbers(L, 2, 3, spot, usage))
	{
		if (args != 4 && args != 5)
			error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	std::string path = getVehicleSavePath(lua_tostring(L, 1));
	bool asBricks = args == 5 && lua_toboolean(L, 5);
	lua_settop(L, 0);

	if (path.empty())
	{
		error("Vehicle save names can't reach outside Saves/Vehicles: " + usage);
		return 0;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		lua_pushnil(L);
		lua_pushstring(L, ("Couldn't open " + path).c_str());
		return 2;
	}
	std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	std::string message;
	std::shared_ptr<Vehicle> vehicle;
	glm::ivec3 grid((int)std::floor(spot[0]), (int)std::floor(spot[1]), (int)std::floor(spot[2]));
	if (!loadVehicleSave(nullptr, data, grid, !asBricks, message, &vehicle))
	{
		lua_pushnil(L);
		lua_pushstring(L, message.c_str());
		return 2;
	}

	if (vehicle)
		LUA_pd->vehicles->pushLua(L, vehicle);
	else
		lua_pushboolean(L, true);
	lua_pushstring(L, message.c_str());
	return 2;
}

static int LUA_getNumVehicles(lua_State* L)
{
	lua_settop(L, 0);
	lua_pushinteger(L, (lua_Integer)LUA_pd->vehicles->size());
	return 1;
}

static int LUA_getVehicleIdx(lua_State* L)
{
	scope("(LUA) getVehicleIdx");

	if (lua_gettop(L) != 1 || !lua_isinteger(L, 1))
	{
		error("Expected getVehicleIdx(index)");
		lua_settop(L, 0);
		return 0;
	}

	lua_Integer index = lua_tointeger(L, 1);
	lua_settop(L, 0);

	if (index < 0 || index >= (lua_Integer)LUA_pd->vehicles->size())
	{
		error("Vehicle index out of range");
		return 0;
	}

	LUA_pd->vehicles->pushLua(L, LUA_pd->vehicles->get((size_t)index));
	return 1;
}

static int LUA_getVehicleId(lua_State* L)
{
	scope("(LUA) getVehicleId");

	if (lua_gettop(L) != 1 || !lua_isinteger(L, 1))
	{
		error("Expected getVehicleId(id)");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->find((netIDType)lua_tointeger(L, 1));
	lua_settop(L, 0);

	if (vehicle)
		LUA_pd->vehicles->pushLua(L, vehicle);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_clearAllVehicles(lua_State* L)
{
	lua_settop(L, 0);
	while (LUA_pd->vehicles->size() > 0)
		destroyVehicle(LUA_pd->vehicles->get(LUA_pd->vehicles->size() - 1));
	return 0;
}

static int LUA_setVehicleDirtEmitter(lua_State* L)
{
	scope("(LUA) setVehicleDirtEmitter");

	if (lua_gettop(L) != 1 || !(lua_type(L, 1) == LUA_TSTRING || lua_isnil(L, 1)))
	{
		error("Expected setVehicleDirtEmitter(typeName) or setVehicleDirtEmitter(nil)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_isnil(L, 1) ? "" : lua_tostring(L, 1);
	lua_settop(L, 0);

	if (!name.empty() && !emitterTypeExists(name))
	{
		error("There's no emitter type named " + name);
		return 0;
	}

	LUA_pd->vehicleDirtEmitter = name;
	return 0;
}

static int LUA_vehicleDestroy(lua_State* L)
{
	scope("(LUA) vehicle:destroy");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:destroy()");
	lua_settop(L, 0);
	if (vehicle)
		destroyVehicle(vehicle);
	return 0;
}

static int LUA_vehicleSaveToFile(lua_State* L)
{
	scope("(LUA) vehicle:saveToFile");

	if (lua_gettop(L) != 2 || lua_type(L, 2) != LUA_TSTRING)
	{
		error("Expected vehicle:saveToFile(fileName)");
		lua_settop(L, 0);
		return 0;
	}

	std::string path = getVehicleSavePath(lua_tostring(L, 2));
	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, "vehicle:saveToFile(fileName)");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	if (path.empty())
	{
		error("Vehicle save names can't reach outside Saves/Vehicles");
		lua_pushboolean(L, false);
		return 1;
	}

	std::error_code errorCode;
	std::filesystem::create_directories("Saves/Vehicles", errorCode);

	std::string data = makeVehicleSaveFile(*vehicle);
	std::ofstream file(path, std::ios::binary);
	bool written = file.is_open() && file.write(data.data(), data.size());
	if (!written)
		error("Couldn't write " + path);

	lua_pushboolean(L, written);
	return 1;
}

static int LUA_vehicleGetNumBricks(lua_State* L)
{
	scope("(LUA) vehicle:getNumBricks");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getNumBricks()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	lua_pushinteger(L, (lua_Integer)vehicle->bricks.size());
	return 1;
}

static int LUA_vehicleGetNumWheels(lua_State* L)
{
	scope("(LUA) vehicle:getNumWheels");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getNumWheels()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	lua_pushinteger(L, (lua_Integer)vehicle->wheels.size());
	return 1;
}

//Shared by the getters of a vector off the vehicle's body
static int pushBodyVector(lua_State* L, const std::string& usage, const std::function<btVector3(const btRigidBody&)>& get)
{
	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, usage);
	lua_settop(L, 0);
	if (!vehicle || !vehicle->body)
		return 0;

	btVector3 value = get(*vehicle->body);
	lua_pushnumber(L, value.x());
	lua_pushnumber(L, value.y());
	lua_pushnumber(L, value.z());
	return 3;
}

//Shared by the setters of a vector on the vehicle's body
static int setBodyVector(lua_State* L, const std::string& usage, const std::function<void(btRigidBody&, const btVector3&)>& set)
{
	float values[3];
	if (lua_gettop(L) != 4 || !readNumbers(L, 2, 3, values, usage))
	{
		if (lua_gettop(L) != 4)
			error("Expected 3 arguments " + usage);
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, usage);
	lua_settop(L, 0);
	if (!vehicle || !vehicle->body)
		return 0;

	set(*vehicle->body, btVector3(values[0], values[1], values[2]));
	vehicle->body->activate();
	return 0;
}

static int LUA_vehicleGetPosition(lua_State* L)
{
	scope("(LUA) vehicle:getPosition");
	return pushBodyVector(L, "vehicle:getPosition()", [](const btRigidBody& body) { return body.getWorldTransform().getOrigin(); });
}

static int LUA_vehicleSetPosition(lua_State* L)
{
	scope("(LUA) vehicle:setPosition");
	return setBodyVector(L, "vehicle:setPosition(x, y, z)", [](btRigidBody& body, const btVector3& value)
	{
		btTransform transform = body.getWorldTransform();
		transform.setOrigin(value);
		body.setWorldTransform(transform);
	});
}

static int LUA_vehicleGetVelocity(lua_State* L)
{
	scope("(LUA) vehicle:getVelocity");
	return pushBodyVector(L, "vehicle:getVelocity()", [](const btRigidBody& body) { return body.getLinearVelocity(); });
}

static int LUA_vehicleSetVelocity(lua_State* L)
{
	scope("(LUA) vehicle:setVelocity");
	return setBodyVector(L, "vehicle:setVelocity(x, y, z)", [](btRigidBody& body, const btVector3& value) { body.setLinearVelocity(value); });
}

static int LUA_vehicleGetAngularVelocity(lua_State* L)
{
	scope("(LUA) vehicle:getAngularVelocity");
	return pushBodyVector(L, "vehicle:getAngularVelocity()", [](const btRigidBody& body) { return body.getAngularVelocity(); });
}

static int LUA_vehicleSetAngularVelocity(lua_State* L)
{
	scope("(LUA) vehicle:setAngularVelocity");
	return setBodyVector(L, "vehicle:setAngularVelocity(x, y, z)", [](btRigidBody& body, const btVector3& value) { body.setAngularVelocity(value); });
}

static int LUA_vehicleSetGravity(lua_State* L)
{
	scope("(LUA) vehicle:setGravity");
	return setBodyVector(L, "vehicle:setGravity(x, y, z)", [](btRigidBody& body, const btVector3& value) { body.setGravity(value); });
}

static int LUA_vehicleGetRotation(lua_State* L)
{
	scope("(LUA) vehicle:getRotation");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getRotation()");
	lua_settop(L, 0);
	if (!vehicle || !vehicle->body)
		return 0;

	btQuaternion rotation = vehicle->body->getWorldTransform().getRotation();
	lua_pushnumber(L, rotation.w());
	lua_pushnumber(L, rotation.x());
	lua_pushnumber(L, rotation.y());
	lua_pushnumber(L, rotation.z());
	return 4;
}

static int LUA_vehicleSetRotation(lua_State* L)
{
	scope("(LUA) vehicle:setRotation");

	const std::string usage = "vehicle:setRotation(w, x, y, z)";
	float values[4];
	if (lua_gettop(L) != 5 || !readNumbers(L, 2, 4, values, usage))
	{
		if (lua_gettop(L) != 5)
			error("Expected 4 arguments " + usage);
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, usage);
	lua_settop(L, 0);
	if (!vehicle || !vehicle->body)
		return 0;

	btQuaternion rotation(values[1], values[2], values[3], values[0]);
	if (rotation.length2() < 0.0001f)
	{
		error("A rotation can't be all zeros: " + usage);
		return 0;
	}

	btTransform transform = vehicle->body->getWorldTransform();
	transform.setRotation(rotation.normalized());
	vehicle->body->setWorldTransform(transform);
	vehicle->body->activate();
	return 0;
}

static int LUA_vehicleGetDriver(lua_State* L)
{
	scope("(LUA) vehicle:getDriver");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getDriver()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	std::shared_ptr<ClientData> driver = vehicle->driver.lock();
	if (driver && driver->client)
		pushClientLua(L, driver->client);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_vehicleSetDestructable(lua_State* L)
{
	scope("(LUA) vehicle:setDestructable");

	if (lua_gettop(L) != 2 || !lua_isboolean(L, 2))
	{
		error("Expected vehicle:setDestructable(bool)");
		lua_settop(L, 0);
		return 0;
	}

	bool destructable = lua_toboolean(L, 2);
	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, "vehicle:setDestructable(bool)");
	lua_settop(L, 0);
	if (vehicle)
		vehicle->destructable = destructable;
	return 0;
}

static int LUA_vehicleIsDestructable(lua_State* L)
{
	scope("(LUA) vehicle:isDestructable");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:isDestructable()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	lua_pushboolean(L, vehicle->destructable);
	return 1;
}

static int LUA_vehicleGetNumSeats(lua_State* L)
{
	scope("(LUA) vehicle:getNumSeats");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getNumSeats()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	lua_pushinteger(L, (lua_Integer)vehicle->passengerSeats.size());
	return 1;
}

static int LUA_vehicleGetPassenger(lua_State* L)
{
	scope("(LUA) vehicle:getPassenger");

	if (lua_gettop(L) != 2 || !lua_isinteger(L, 2))
	{
		error("Expected vehicle:getPassenger(seat)");
		lua_settop(L, 0);
		return 0;
	}

	lua_Integer seat = lua_tointeger(L, 2);
	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, "vehicle:getPassenger(seat)");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	if (seat < 0 || seat >= (lua_Integer)vehicle->passengerSeats.size())
	{
		error("Seat out of range in vehicle:getPassenger(seat)");
		return 0;
	}

	std::shared_ptr<ClientData> rider = vehicle->passengerSeats[(size_t)seat].rider.lock();
	if (rider && rider->client)
		pushClientLua(L, rider->client);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_vehicleEjectDriver(lua_State* L)
{
	scope("(LUA) vehicle:ejectDriver");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:ejectDriver()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	if (std::shared_ptr<ClientData> driver = vehicle->driver.lock())
		exitVehicle(*driver, false);
	return 0;
}

static int LUA_vehicleGetBuilder(lua_State* L)
{
	scope("(LUA) vehicle:getBuilder");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getBuilder()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	std::shared_ptr<JoinedClient> builder = vehicle->builderID != NO_ID && LUA_server ? LUA_server->getClientByNetId(vehicle->builderID) : nullptr;
	if (builder)
		pushClientLua(L, builder);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_vehicleGetBuilderID(lua_State* L)
{
	scope("(LUA) vehicle:getBuilderID");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getBuilderID()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	lua_pushinteger(L, vehicle->builderID == NO_ID ? -1 : (lua_Integer)vehicle->builderID);
	return 1;
}

static int LUA_vehicleGetMusic(lua_State* L)
{
	scope("(LUA) vehicle:getMusic");

	std::shared_ptr<Vehicle> vehicle = plainVehicleMethod(L, "vehicle:getMusic()");
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	if (vehicle->musicName.empty())
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, vehicle->musicName.c_str());
	lua_pushnumber(L, vehicle->musicVolume);
	lua_pushnumber(L, vehicle->musicPitch);
	return 3;
}

static int LUA_vehicleSetMusic(lua_State* L)
{
	scope("(LUA) vehicle:setMusic");

	const std::string usage = "vehicle:setMusic(soundName[, volume, pitch]) or vehicle:setMusic(nil)";
	int args = lua_gettop(L);
	if ((args != 2 && args != 4) || !(lua_type(L, 2) == LUA_TSTRING || lua_isnil(L, 2)))
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);
	float levels[2] = { 1.0f, 1.0f };
	if (args == 4 && !readNumbers(L, 3, 2, levels, usage))
	{
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Vehicle> vehicle = vehicleArgument(L, usage);
	lua_settop(L, 0);
	if (!vehicle)
		return 0;

	if (!name.empty() && !soundTypeExists(name))
	{
		error("There's no sound type named " + name);
		return 0;
	}

	if (args == 2)
	{
		levels[0] = vehicle->musicVolume;
		levels[1] = vehicle->musicPitch;
	}

	setVehicleMusic(*vehicle, name, levels[0], levels[1]);
	return 0;
}

//Client methods, called as client:method(...)
static std::shared_ptr<ClientData> clientOnTop(lua_State* L, const std::string& usage)
{
	std::shared_ptr<JoinedClient> joined = popClientLua(L);
	std::shared_ptr<ClientData> client = joined ? LUA_pd->getClient(joined) : nullptr;
	if (!client)
		error("Invalid client passed to " + usage);
	return client;
}

static int LUA_clientGetVehicle(lua_State* L)
{
	scope("(LUA) client:getVehicle");

	if (lua_gettop(L) != 1)
	{
		error("Expected no arguments client:getVehicle()");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<ClientData> client = clientOnTop(L, "client:getVehicle()");
	lua_settop(L, 0);
	if (!client)
		return 0;

	std::shared_ptr<Vehicle> vehicle = client->vehicle.lock();
	if (!vehicle)
	{
		lua_pushnil(L);
		return 1;
	}

	LUA_pd->vehicles->pushLua(L, vehicle);
	if (client->vehicleSeat == Vehicle::driverSeat)
		lua_pushnil(L);
	else
		lua_pushinteger(L, client->vehicleSeat);
	return 2;
}

static int LUA_clientEnterVehicle(lua_State* L)
{
	scope("(LUA) client:enterVehicle");

	int args = lua_gettop(L);
	if ((args != 2 && args != 3) || (args == 3 && !lua_isinteger(L, 3)))
	{
		error("Expected client:enterVehicle(vehicle[, seat])");
		lua_settop(L, 0);
		return 0;
	}

	int seat = Vehicle::driverSeat;
	bool badSeat = false;
	if (args == 3)
	{
		lua_Integer given = lua_tointeger(L, 3);
		badSeat = given < 0 || given > (lua_Integer)Vehicle::maxSeats;
		seat = badSeat ? 0 : (int)given;
		lua_pop(L, 1);
	}

	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->popLua(L);
	std::shared_ptr<ClientData> client = clientOnTop(L, "client:enterVehicle(vehicle[, seat])");
	lua_settop(L, 0);
	if (!vehicle || !client)
		return 0;

	lua_pushboolean(L, !badSeat && enterVehicle(*client, vehicle, seat, false));
	return 1;
}

static int LUA_clientExitVehicle(lua_State* L)
{
	scope("(LUA) client:exitVehicle");

	if (lua_gettop(L) != 1)
	{
		error("Expected no arguments client:exitVehicle()");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<ClientData> client = clientOnTop(L, "client:exitVehicle()");
	lua_settop(L, 0);
	if (client)
		exitVehicle(*client, false);
	return 0;
}

luaL_Reg* getVehicleFunctions(lua_State* L)
{
	lua_register(L, "sliceBricks", LUA_sliceBricks);
	lua_register(L, "loadVehicleFile", LUA_loadVehicleFile);
	lua_register(L, "getNumVehicles", LUA_getNumVehicles);
	lua_register(L, "getVehicleIdx", LUA_getVehicleIdx);
	lua_register(L, "getVehicleId", LUA_getVehicleId);
	lua_register(L, "clearAllVehicles", LUA_clearAllVehicles);
	lua_register(L, "setVehicleDirtEmitter", LUA_setVehicleDirtEmitter);
	lua_register(L, "radiusImpulse", LUA_radiusImpulse);

	//Added to the client metatable registerClientFunctions made
	lua_getglobal(L, "metatable_client");
	if (lua_istable(L, -1))
	{
		const luaL_Reg clientMethods[] = {
			{ "getVehicle", LUA_clientGetVehicle },
			{ "enterVehicle", LUA_clientEnterVehicle },
			{ "exitVehicle", LUA_clientExitVehicle },
			{ NULL, NULL }
		};
		luaL_setfuncs(L, clientMethods, 0);
	}
	else
		error("getVehicleFunctions needs registerClientFunctions to have run first");
	lua_pop(L, 1);

	const luaL_Reg methods[] = {
		{ "destroy", LUA_vehicleDestroy },
		{ "remove", LUA_vehicleDestroy },
		{ "saveToFile", LUA_vehicleSaveToFile },
		{ "getNumBricks", LUA_vehicleGetNumBricks },
		{ "getNumWheels", LUA_vehicleGetNumWheels },
		{ "getPosition", LUA_vehicleGetPosition },
		{ "setPosition", LUA_vehicleSetPosition },
		{ "getRotation", LUA_vehicleGetRotation },
		{ "setRotation", LUA_vehicleSetRotation },
		{ "getVelocity", LUA_vehicleGetVelocity },
		{ "setVelocity", LUA_vehicleSetVelocity },
		{ "getAngularVelocity", LUA_vehicleGetAngularVelocity },
		{ "setAngularVelocity", LUA_vehicleSetAngularVelocity },
		{ "setGravity", LUA_vehicleSetGravity },
		{ "getDriver", LUA_vehicleGetDriver },
		{ "getNumSeats", LUA_vehicleGetNumSeats },
		{ "setDestructable", LUA_vehicleSetDestructable },
		{ "isDestructable", LUA_vehicleIsDestructable },
		{ "getPassenger", LUA_vehicleGetPassenger },
		{ "ejectDriver", LUA_vehicleEjectDriver },
		{ "getBuilder", LUA_vehicleGetBuilder },
		{ "getBuilderID", LUA_vehicleGetBuilderID },
		{ "getMusic", LUA_vehicleGetMusic },
		{ "setMusic", LUA_vehicleSetMusic },
		{ NULL, NULL }
	};

	size_t count = sizeof(methods) / sizeof(luaL_Reg);
	luaL_Reg* list = new luaL_Reg[count];
	std::copy(methods, methods + count, list);
	return list;
}
