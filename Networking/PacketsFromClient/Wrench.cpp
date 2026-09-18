#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"
#include "../../LuaFunctions/BrickLua.h"
#include "../../LuaFunctions/SoundLua.h"
#include "../../LuaFunctions/EmitterLua.h"
#include "../../LuaFunctions/VehicleLua.h"

#include <cmath>

//How far from the camera a brick can be wrenched
static constexpr float wrenchReach = 100.0f;

/*
	1 byte		-	packet type
	12 bytes	-	camera position
	12 bytes	-	camera direction

	Wrenches the brick or vehicle the camera points at, which opens its wrench dialog unless a ClientWrenchBrick or ClientWrenchVehicle listener says not to
*/
void wrenchRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(float) * 6)
		return;

	float values[6];
	memcpy(values, packet->data + 1, sizeof(values));
	for (float value : values)
		if (!std::isfinite(value))
			return;

	glm::vec3 start(values[0], values[1], values[2]);
	glm::vec3 direction(values[3], values[4], values[5]);
	if (glm::length(direction) < 0.0001f)
		return;
	direction = glm::normalize(direction);

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	//Not their own player, a third person camera looks past it
	btRigidBody* ignore = client->controlledObjects.empty() ? nullptr : client->controlledObjects[0]->body;
	btRigidBody* hit = pd->physicsWorld->doRaycast(g2b3(start), g2b3(start + direction * wrenchReach), ignore);
	if (!hit)
		return;

	lua_State* L = pd->luaState;

	if (std::shared_ptr<Vehicle> vehicle = vehicleFromBody(hit))
	{
		pushClientLua(L, source->me);
		pd->vehicles->pushLua(L, vehicle);
		pd->eventManager->callEvent(L, "ClientWrenchVehicle", 2);
		bool vetoed = lua_gettop(L) == 2 && lua_isnil(L, 2);
		lua_settop(L, 0);

		if (!vetoed && pd->vehicles->find(vehicle->getID()))
			openVehicleWrenchDialog(*client, *vehicle);
		return;
	}

	if (hit->getUserIndex() != brickBody)
		return;

	Brick* brick = (Brick*)hit->getUserPointer();
	netIDType brickID = brick->netId;

	pushClientLua(L, source->me);
	pd->bricks->pushLua(L, brick);
	pd->eventManager->callEvent(L, "ClientWrenchBrick", 2);

	//A listener can return nil instead of the brick to keep the dialog closed, or another brick to open that one's instead
	//One that broke leaves nothing on the stack, which doesn't stop wrenching
	if (lua_gettop(L) == 2)
	{
		if (lua_isnil(L, 2))
		{
			lua_settop(L, 0);
			return;
		}

		brick = pd->bricks->popLua(L);
	}
	else
		brick = pd->bricks->find(brickID);

	lua_settop(L, 0);

	if (brick)
		openWrenchDialog(*client, brick);
}

/*
	1 byte		-	packet type
	4 bytes		-	brick net ID
	1 byte		-	1 if it collides
	1 byte		-	name length
	0-255 bytes	-	name
	Then		-	BrickAttachments::write
*/
void wrenchSubmit(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2 + sizeof(netIDType) + 1)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	netIDType brickID;
	memcpy(&brickID, packet->data + 1, sizeof(netIDType));

	//Only the brick they were last sent a dialog for, and only once
	if (brickID != client->wrenchedBrickID)
		return;
	client->wrenchedBrickID = NO_ID;

	Brick* brick = pd->bricks->find(brickID);
	if (!brick)
		return;

	bool collides = packet->data[1 + sizeof(netIDType)] & 1;

	size_t at = 2 + sizeof(netIDType);
	size_t nameLength = packet->data[at++];
	if (at + nameLength > packet->dataLength)
		return;

	std::string name((char*)packet->data + at, nameLength);
	at += nameLength;

	BrickAttachments settings;
	if (!settings.read(packet->data, packet->dataLength, at))
		return;

	//Players pick from the server's music and emitter types, but whatever Lua put on the brick can stay
	const BrickAttachments* current = brick->attachments.get();
	if (!settings.musicName.empty() && !isMusicSoundType(settings.musicName) && !(current && current->musicName == settings.musicName))
		settings.musicName = "";
	if (!settings.emitterName.empty() && !emitterTypeExists(settings.emitterName) && !(current && current->emitterName == settings.emitterName))
		settings.emitterName = "";

	//Wheel settings only go on wheels and steering settings on steering wheels, other bricks keep whatever Lua gave them
	const SpecialBrickType* type = brick->isSpecial() ? pd->brickTypes.getSpecial(brick->typeID - 1) : nullptr;
	VehiclePart part = type ? type->vehiclePart : VehiclePart_None;
	if (part != VehiclePart_Wheel)
	{
		settings.hasWheel = current && current->hasWheel;
		settings.wheel = current ? current->wheel : WheelSettings();
	}
	if (part != VehiclePart_Steering)
	{
		settings.hasSteering = current && current->hasSteering;
		settings.steering = current ? current->steering : SteeringSettings();
		settings.hasHorn = current && current->hasHorn;
		settings.hornName = current ? current->hornName : "";
		settings.lightIsHeadlight = current && current->lightIsHeadlight;
	}
	else if (!settings.hornName.empty() && !soundTypeExists(settings.hornName) && !(current && current->hornName == settings.hornName))
		settings.hornName = "";

	//A vehicle only goes on a Vehicle Spawn brick, and only one Lua registered, and an item has to be an item type, though again whatever Lua put there can stay
	if (!brickIsVehicleSpawn(brick) || (!settings.vehicleSpawnName.empty() && !vehicleSpawnExists(settings.vehicleSpawnName) && !(current && current->vehicleSpawnName == settings.vehicleSpawnName)))
		settings.vehicleSpawnName = current ? current->vehicleSpawnName : "";

	if (!settings.itemSpawnName.empty() && !(current && current->itemSpawnName == settings.itemSpawnName))
	{
		bool isItemType = false;
		for (const std::shared_ptr<DynamicType>& type : pd->dynamicTypes)
			isItemType = isItemType || (type->isItemType && type->scriptName == settings.itemSpawnName);

		if (!isItemType)
			settings.itemSpawnName = "";
	}

	if (brick->collides != collides)
		pd->bricks->setColliding(brick, collides);
	pd->bricks->setName(brick, name);
	setBrickAttachments(brick, settings);
}

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
	The rest	-	BrickAttachments::write, with its music, its horn, and its headlight as the light
*/
void vehicleWrenchSubmit(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(netIDType) + 1)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	netIDType vehicleID;
	memcpy(&vehicleID, packet->data + 1, sizeof(netIDType));

	//Only the vehicle they were last sent a dialog for, and only once
	if (vehicleID != client->wrenchedVehicleID)
		return;
	client->wrenchedVehicleID = NO_ID;

	std::shared_ptr<Vehicle> vehicle = pd->vehicles->find(vehicleID);
	if (!vehicle)
		return;

	size_t at = 1 + sizeof(netIDType);
	BrickAttachments settings;
	if (!settings.read(packet->data, packet->dataLength, at))
		return;
	settings.clampValues();

	//Same as bricks: a music sound type, or whatever Lua already put on it
	if (!settings.musicName.empty() && !isMusicSoundType(settings.musicName) && settings.musicName != vehicle->musicName)
		settings.musicName = "";

	//Loops can't be changed while they play, so only a change starts it over
	if (settings.musicName != vehicle->musicName || settings.musicVolume != vehicle->musicVolume || settings.musicPitch != vehicle->musicPitch)
		setVehicleMusic(*vehicle, settings.musicName, settings.musicVolume, settings.musicPitch);

	//Any sound the server has can be the horn, or none, and whatever Lua already gave it
	if (settings.hasHorn && (settings.hornName.empty() || soundTypeExists(settings.hornName) || settings.hornName == vehicle->hornName))
		vehicle->hornName = settings.hornName;

	//The light is its headlight, which applying switches on, see setVehicleHeadlight
	if (settings.hasLight || vehicle->headlight.hasLight)
		setVehicleHeadlight(*vehicle, settings);
}

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID

	The client confirmed Remove vehicle in its wrench dialog
*/
void vehicleRemoveRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(netIDType))
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	netIDType vehicleID;
	memcpy(&vehicleID, packet->data + 1, sizeof(netIDType));

	//Like applying the dialog, only the vehicle they were last sent a dialog for, and only once
	if (vehicleID != client->wrenchedVehicleID)
		return;
	client->wrenchedVehicleID = NO_ID;

	std::shared_ptr<Vehicle> vehicle = pd->vehicles->find(vehicleID);
	if (!vehicle)
		return;

	lua_State* L = pd->luaState;
	pushClientLua(L, client->client);
	pd->vehicles->pushLua(L, vehicle);
	pd->eventManager->callEvent(L, "ClientRemoveVehicle", 2);
	bool vetoed = lua_gettop(L) == 2 && lua_isnil(L, 2);
	lua_settop(L, 0);

	//Listeners can remove it themselves
	if (vetoed || !pd->vehicles->find(vehicleID))
		return;

	destroyVehicle(vehicle);
}
