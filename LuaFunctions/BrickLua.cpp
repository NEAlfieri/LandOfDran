#include "BrickLua.h"

#include "SoundLua.h"
#include "EmitterLua.h"
#include "VehicleLua.h"
#include "../Bricks/BrickSaves.h"

#include <cmath>

//How far above a Vehicle Spawn brick's top its vehicle's origin is put, world units, enough for a jeep's wheels to hang below it and land
static constexpr float vehicleSpawnHeight = 3.0f;

//How often bricks with a vehicle spawn are checked for a vehicle that's gone, and how long a spawner that failed waits before it's tried again, milliseconds
static constexpr unsigned int vehicleRespawnCheckMS = 1000;
static constexpr unsigned int vehicleSpawnRetryMS = 10000;

bool brickIsVehicleSpawn(const Brick* brick)
{
	if (!LUA_pd || !brick || !brick->isSpecial())
		return false;

	const SpecialBrickType* type = LUA_pd->brickTypes.getSpecial(brick->typeID - 1);
	return type && type->vehicleSpawn;
}

std::shared_ptr<Vehicle> getBrickSpawnedVehicle(const Brick* brick)
{
	if (!LUA_pd || !brick || !brick->attachments || brick->attachments->spawnedVehicleID == NO_ID)
		return nullptr;

	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->find(brick->attachments->spawnedVehicleID);
	return vehicle && vehicle->spawnBrickID == brick->netId ? vehicle : nullptr;
}

std::shared_ptr<Item> getBrickDisplayItem(const Brick* brick)
{
	if (!LUA_pd || !brick || !brick->attachments || brick->attachments->displayItemID == NO_ID)
		return nullptr;

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->find(brick->attachments->displayItemID);
	if (!dynamic || dynamic->getKind() != DynamicKind_Item)
		return nullptr;

	std::shared_ptr<Item> item = std::static_pointer_cast<Item>(dynamic);
	return item->display && item->displayBrickID == brick->netId ? item : nullptr;
}

//The middle of a brick's top face, world units
static glm::vec3 brickTop(const Brick* brick)
{
	return brick->getWorldCenter() + glm::vec3(0, brick->height * PLATE_SIZE * 0.5f, 0);
}

//Spawns the brick's vehicle above it, remembering it, or when to try again if the spawner didn't make one
static void spawnBrickVehicle(Brick* brick)
{
	BrickAttachments& settings = *brick->attachments;

	std::shared_ptr<Vehicle> vehicle = spawnRegisteredVehicle(settings.vehicleSpawnName, brickTop(brick) + glm::vec3(0, vehicleSpawnHeight, 0), brick);
	settings.spawnedVehicleID = vehicle ? vehicle->getID() : NO_ID;
	settings.nextVehicleSpawnMS = getTicksMS() + (vehicle ? 0 : vehicleSpawnRetryMS);
}

//Makes the display item floating over the brick, a copy of the item type named by its item spawn, NO_ID if there's no such item type
static netIDType makeDisplayItem(Brick* brick, const std::string& typeName)
{
	std::shared_ptr<DynamicType> type = nullptr;
	for (const std::shared_ptr<DynamicType>& candidate : LUA_pd->dynamicTypes)
	{
		if (candidate->isItemType && candidate->scriptName == typeName)
			type = candidate;
	}

	if (!type)
	{
		error("Brick " + std::to_string(brick->netId) + " offers an item of type " + typeName + ", which isn't an item type here, see newItemType");
		return NO_ID;
	}

	//Its collision box floats displayHover above the brick, wherever that box sits on the model
	std::shared_ptr<Model> model = type->getModel();
	glm::vec3 position = brickTop(brick) + glm::vec3(0, Item::displayHover + model->getColHalfExtents().y - model->getColOffset().y, 0);

	std::shared_ptr<Item> item = LUA_pd->dynamics->createDerived<Item>(type, g2b3(position), btQuaternion::getIdentity());
	item->makeDisplay(brick->netId);
	return item->getID();
}

static unsigned char colorByte(double value)
{
	return (unsigned char)std::clamp(value * 255.0 + 0.5, 0.0, 255.0);
}

void updateBrickAttachments(Brick* brick)
{
	if (!LUA_pd || !brick->attachments)
		return;

	BrickAttachments& settings = *brick->attachments;
	glm::vec3 center = brick->getWorldCenter();

	//Music, started again if Lua stopped it
	if (settings.musicName.empty())
	{
		if (settings.musicLoopID != NO_ID)
			stopSoundLoopByID(settings.musicLoopID);
		settings.musicLoopID = NO_ID;
	}
	else if (settings.musicLoopID == NO_ID || !isSoundLoopPlaying(settings.musicLoopID))
	{
		unsigned int loopID;
		settings.musicLoopID = startSoundLoopAt(settings.musicName, center, settings.musicPitch, settings.musicVolume, loopID) ? loopID : NO_ID;
	}

	//Light, made again if Lua destroyed it
	if (LUA_pd->lights)
	{
		std::shared_ptr<Light> light = settings.lightID != NO_ID ? LUA_pd->lights->find(settings.lightID) : nullptr;

		if (!settings.hasLight)
		{
			if (light)
				LUA_pd->lights->destroy(light);
			settings.lightID = NO_ID;
		}
		else
		{
			glm::vec3 position = center + settings.lightOffset;
			if (!light)
			{
				light = LUA_pd->lights->create(position, settings.lightColor, settings.lightBrightness, settings.lightFlicker, settings.lightCoronaWidth);
				settings.lightID = light->getID();
			}
			else
			{
				light->setPosition(position);
				light->setColor(settings.lightColor);
				light->setBrightness(settings.lightBrightness);
				light->setFlicker(settings.lightFlicker);
				light->setCoronaWidth(settings.lightCoronaWidth);
			}

			light->setConeAngle(settings.lightConeAngle);
			light->setDirection(settings.lightDirection);
			light->setSpin(settings.lightSpin);
			light->setBlink(settings.lightBlinkSpeed, settings.lightBlinkStrength);
		}
	}

	//Emitter, made again if Lua destroyed it or changed its type, and left alone if Lua took it off the brick
	if (LUA_pd->emitters)
	{
		std::shared_ptr<Emitter> emitter = settings.emitterID != NO_ID ? LUA_pd->emitters->find(settings.emitterID) : nullptr;
		if (emitter && emitter->brickID != brick->netId)
			emitter = nullptr;

		if (emitter && getEmitterTypeName(*emitter) != settings.emitterName)
			LUA_pd->emitters->destroy(emitter);

		if (!emitter)
		{
			settings.emitterID = NO_ID;

			emitter = settings.emitterName.empty() ? nullptr : spawnEmitterAt(settings.emitterName, center);
			if (emitter)
			{
				emitter->attachToBrick(brick->netId, center);
				settings.emitterID = emitter->getID();
			}
		}
	}

	//The item floating over it, made again if Lua destroyed it, or for a different item type than before
	if (LUA_pd->dynamics)
	{
		std::shared_ptr<Item> item = getBrickDisplayItem(brick);
		if (item && (settings.itemSpawnName.empty() || item->getType()->scriptName != settings.itemSpawnName))
		{
			std::shared_ptr<Dynamic> dynamic = item;
			LUA_pd->dynamics->destroy(dynamic);
			item = nullptr;
		}

		if (!item)
			settings.displayItemID = settings.itemSpawnName.empty() ? NO_ID : makeDisplayItem(brick, settings.itemSpawnName);
	}

	/*
		The vehicle it keeps spawned, taken away if it's not to spawn one anymore, or when what it spawns changes
		A brick whose vehicle was destroyed gets another from respawnBrickVehicles, which waits a moment rather than doing it mid-explosion
	*/
	if (LUA_pd->vehicles)
	{
		std::shared_ptr<Vehicle> vehicle = getBrickSpawnedVehicle(brick);
		if (vehicle && settings.vehicleSpawnName.empty())
		{
			destroyVehicle(vehicle);
			vehicle = nullptr;
		}

		if (!vehicle)
			settings.spawnedVehicleID = NO_ID;

		//Changed to another vehicle: the old one stays until it's gone, then the new kind takes its place, rather than pulling a vehicle out from under its driver
		if (!vehicle && !settings.vehicleSpawnName.empty())
			spawnBrickVehicle(brick);
	}
}

void respawnBrickVehicles()
{
	static unsigned int lastCheckMS = 0;

	if (!LUA_pd || !LUA_pd->bricks || !LUA_pd->vehicles || getTicksMS() - lastCheckMS < vehicleRespawnCheckMS)
		return;
	lastCheckMS = getTicksMS();

	//A spawner can add or remove bricks, so the list is walked by index and stops if it shrinks
	for (size_t a = 0; a < LUA_pd->bricks->size(); a++)
	{
		Brick* brick = LUA_pd->bricks->get(a);
		if (!brick->attachments || brick->attachments->vehicleSpawnName.empty())
			continue;

		if (getBrickSpawnedVehicle(brick) || getTicksMS() < brick->attachments->nextVehicleSpawnMS)
			continue;

		spawnBrickVehicle(brick);
	}
}

void removeBrickAttachments(Brick* brick)
{
	if (!brick->attachments)
		return;

	brick->attachments->musicName = "";
	brick->attachments->hasLight = false;
	brick->attachments->emitterName = "";
	brick->attachments->itemSpawnName = "";
	brick->attachments->vehicleSpawnName = "";
	updateBrickAttachments(brick);
}

void setBrickAttachments(Brick* brick, const BrickAttachments& requested)
{
	auto settings = std::make_shared<BrickAttachments>(requested);
	settings->clampValues();

	settings->musicLoopID = NO_ID;
	settings->lightID = NO_ID;
	settings->emitterID = NO_ID;
	settings->spawnedVehicleID = NO_ID;
	settings->displayItemID = NO_ID;
	settings->nextVehicleSpawnMS = 0;

	if (const BrickAttachments* old = brick->attachments.get())
	{
		settings->musicLoopID = old->musicLoopID;
		settings->lightID = old->lightID;
		settings->emitterID = old->emitterID;
		settings->spawnedVehicleID = old->spawnedVehicleID;
		settings->displayItemID = old->displayItemID;
		//Picking a vehicle again doesn't wait out a failed try
		settings->nextVehicleSpawnMS = old->vehicleSpawnName == settings->vehicleSpawnName ? old->nextVehicleSpawnMS : 0;

		//Loops can't be changed while they play, so different music, volume, or pitch starts it over
		bool musicChanged = old->musicName != settings->musicName || old->musicVolume != settings->musicVolume || old->musicPitch != settings->musicPitch;
		if (musicChanged && settings->musicLoopID != NO_ID)
		{
			stopSoundLoopByID(settings->musicLoopID);
			settings->musicLoopID = NO_ID;
		}
	}

	brick->attachments = settings;
	updateBrickAttachments(brick);

	if (settings->isEmpty())
		brick->attachments = nullptr;
}

void openWrenchDialog(ClientData& client, const Brick* brick)
{
	if (!client.client)
		return;

	/*
		1 byte		-	packet type
		4 bytes		-	brick net ID
		1 byte		-	1 if it collides
		1 byte		-	name length
		0-255 bytes	-	name
		Then		-	BrickAttachments::write
		4 bytes		-	net ID of the light the brick already has, NO_ID for none
		1 byte		-	how many vehicle spawns follow, 0 unless the brick is a Vehicle Spawn brick
		Each		-	a length byte and the name of a vehicle spawn the dialog can pick, see registerVehicleSpawn
	*/
	std::string name = brick->name.substr(0, 255);

	std::vector<unsigned char> bytes;
	bytes.push_back(OpenWrenchDialog);
	bytes.resize(1 + sizeof(netIDType));
	memcpy(bytes.data() + 1, &brick->netId, sizeof(netIDType));
	bytes.push_back(brick->collides ? 1 : 0);
	bytes.push_back((unsigned char)name.length());
	bytes.insert(bytes.end(), name.begin(), name.end());
	(brick->attachments ? *brick->attachments : BrickAttachments()).write(bytes);

	//So the client can leave it out while it shines the light being edited instead, see WrenchDialog::getLightPreview
	netIDType lightID = brick->attachments ? brick->attachments->lightID : NO_ID;
	size_t lightAt = bytes.size();
	bytes.resize(lightAt + sizeof(netIDType));
	memcpy(bytes.data() + lightAt, &lightID, sizeof(netIDType));

	//What a Vehicle Spawn brick can spawn, any other brick gets none and no section for it
	size_t spawnCount = brickIsVehicleSpawn(brick) ? std::min<size_t>(LUA_pd->vehicleSpawns.size(), 255) : 0;
	bytes.push_back((unsigned char)spawnCount);
	for (size_t a = 0; a < spawnCount; a++)
	{
		std::string spawnName = LUA_pd->vehicleSpawns[a].name.substr(0, 255);
		bytes.push_back((unsigned char)spawnName.length());
		bytes.insert(bytes.end(), spawnName.begin(), spawnName.end());
	}

	client.client->send(enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable)), OtherReliable);
	client.wrenchedBrickID = brick->netId;
}

bool brickCanPrint(const Brick* brick)
{
	if (!brick || !brick->isSpecial())
		return false;

	const SpecialBrickType* type = LUA_pd->brickTypes.getSpecial(brick->typeID - 1);
	return type && type->groupCount[BrickTexturePrint] > 0;
}

void openPrintMenu(ClientData& client, const Brick* brick)
{
	if (!client.client)
		return;

	/*
		1 byte		-	packet type
		4 bytes		-	brick net ID
		1 byte		-	print name length
		0-255 bytes	-	the print it already wears, "" for none
	*/
	std::string printName = LUA_pd->prints.getName((int)brick->printID - 1).substr(0, 255);

	std::vector<unsigned char> bytes;
	bytes.push_back(OpenPrintMenu);
	bytes.resize(1 + sizeof(netIDType));
	memcpy(bytes.data() + 1, &brick->netId, sizeof(netIDType));
	bytes.push_back((unsigned char)printName.length());
	bytes.insert(bytes.end(), printName.begin(), printName.end());

	client.client->send(enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable)), OtherReliable);
	client.printedBrickID = brick->netId;
}

//Methods are called as brick:method(...), so the brick is always argument 1
static Brick* brickArgument(lua_State* L)
{
	lua_pushvalue(L, 1);
	return LUA_pd->bricks->popLua(L);
}

static int LUA_addBrick(lua_State* L)
{
	scope("(LUA) addBrick");

	int args = lua_gettop(L);

	if (args != 10 && args != 11)
	{
		error("Expected 10 or 11 arguments addBrick(x, y, z, width, height, length, r, g, b, a[, angleID])");
		lua_settop(L, 0);
		return 0;
	}

	int width = (int)lua_tonumber(L, 4);
	int height = (int)lua_tonumber(L, 5);
	int length = (int)lua_tonumber(L, 6);
	int angleID = args == 11 ? (int)lua_tonumber(L, 11) : 0;

	if (width < 1 || width > 255 || height < 1 || height > 255 || length < 1 || length > 255)
	{
		error("Brick width, height, and length must be between 1 and 255");
		lua_settop(L, 0);
		return 0;
	}

	if (angleID < 0 || angleID > 3)
	{
		error("Brick angleID must be between 0 and 3");
		lua_settop(L, 0);
		return 0;
	}

	Brick desc;
	desc.x = (int)floor(lua_tonumber(L, 1));
	desc.y = (int)floor(lua_tonumber(L, 2));
	desc.z = (int)floor(lua_tonumber(L, 3));
	desc.width = width;
	desc.height = height;
	desc.length = length;
	desc.color = glm::u8vec4(colorByte(lua_tonumber(L, 7)), colorByte(lua_tonumber(L, 8)), colorByte(lua_tonumber(L, 9)), colorByte(lua_tonumber(L, 10)));
	desc.angleID = angleID;

	lua_settop(L, 0);

	Brick* brick = LUA_pd->bricks->add(desc);
	if (brick)
		LUA_pd->bricks->pushLua(L, brick);
	else
		lua_pushnil(L);

	return 1;
}

static int LUA_addSpecialBrick(lua_State* L)
{
	scope("(LUA) addSpecialBrick");

	int args = lua_gettop(L);

	if ((args != 8 && args != 9) || !lua_isstring(L, 4))
	{
		error("Expected 8 or 9 arguments addSpecialBrick(x, y, z, typeName, r, g, b, a[, angleID])");
		lua_settop(L, 0);
		return 0;
	}

	std::string typeName = lua_tostring(L, 4);
	int special = LUA_pd->brickTypes.findSpecial(typeName);
	int angleID = args == 9 ? (int)lua_tonumber(L, 9) : 0;

	if (special < 0)
	{
		error("No special brick type named " + typeName);
		lua_settop(L, 0);
		return 0;
	}

	if (angleID < 0 || angleID > 3)
	{
		error("Brick angleID must be between 0 and 3");
		lua_settop(L, 0);
		return 0;
	}

	Brick desc;
	desc.x = (int)floor(lua_tonumber(L, 1));
	desc.y = (int)floor(lua_tonumber(L, 2));
	desc.z = (int)floor(lua_tonumber(L, 3));
	desc.typeID = (uint16_t)(special + 1);
	desc.color = glm::u8vec4(colorByte(lua_tonumber(L, 5)), colorByte(lua_tonumber(L, 6)), colorByte(lua_tonumber(L, 7)), colorByte(lua_tonumber(L, 8)));
	desc.angleID = angleID;

	lua_settop(L, 0);

	Brick* brick = LUA_pd->bricks->add(desc);
	if (brick)
		LUA_pd->bricks->pushLua(L, brick);
	else
		lua_pushnil(L);

	return 1;
}

static int LUA_getNumBricks(lua_State* L)
{
	lua_settop(L, 0);
	lua_pushinteger(L, LUA_pd->bricks->size());
	return 1;
}

static int LUA_getBrickIdx(lua_State* L)
{
	scope("(LUA) getBrickIdx");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument getBrickIdx(index)");
		lua_settop(L, 0);
		return 0;
	}

	lua_Integer index = lua_tointeger(L, 1);
	lua_settop(L, 0);

	if (index < 0 || index >= (lua_Integer)LUA_pd->bricks->size())
	{
		error("Brick index out of range");
		return 0;
	}

	LUA_pd->bricks->pushLua(L, LUA_pd->bricks->get(index));
	return 1;
}

static int LUA_getNumNamedBricks(lua_State* L)
{
	scope("(LUA) getNumNamedBricks");

	if (lua_gettop(L) != 1 || !lua_isstring(L, 1))
	{
		error("Expected 1 argument getNumNamedBricks(name)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 1);
	lua_settop(L, 0);

	lua_pushinteger(L, LUA_pd->bricks->numNamed(name));
	return 1;
}

static int LUA_getNamedBrickIdx(lua_State* L)
{
	scope("(LUA) getNamedBrickIdx");

	if (lua_gettop(L) != 2 || !lua_isstring(L, 1))
	{
		error("Expected 2 arguments getNamedBrickIdx(name, index)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 1);
	lua_Integer index = lua_tointeger(L, 2);
	lua_settop(L, 0);

	Brick* brick = index < 0 ? nullptr : LUA_pd->bricks->getNamed(name, (size_t)index);
	if (!brick)
	{
		error("Named brick index out of range");
		return 0;
	}

	LUA_pd->bricks->pushLua(L, brick);
	return 1;
}

static int LUA_getBrickId(lua_State* L)
{
	scope("(LUA) getBrickId");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument getBrickId(id)");
		lua_settop(L, 0);
		return 0;
	}

	Brick* brick = LUA_pd->bricks->find((netIDType)lua_tointeger(L, 1));
	lua_settop(L, 0);

	if (brick)
		LUA_pd->bricks->pushLua(L, brick);
	else
		lua_pushnil(L);

	return 1;
}

static int LUA_getBrickAt(lua_State* L)
{
	scope("(LUA) getBrickAt");

	if (lua_gettop(L) != 3)
	{
		error("Expected 3 arguments getBrickAt(x, y, z)");
		lua_settop(L, 0);
		return 0;
	}

	Brick* brick = LUA_pd->bricks->getAt((int)floor(lua_tonumber(L, 1)), (int)floor(lua_tonumber(L, 2)), (int)floor(lua_tonumber(L, 3)));
	lua_settop(L, 0);

	if (brick)
		LUA_pd->bricks->pushLua(L, brick);
	else
		lua_pushnil(L);

	return 1;
}

static int LUA_clearAllBricks(lua_State* L)
{
	lua_settop(L, 0);
	LUA_pd->bricks->clear();
	return 0;
}

//Save functions take a file name inside the Saves folder, logs an error and returns "" for anything else
static std::string saveFileArgument(lua_State* L, int index)
{
	const char* fileName = lua_tostring(L, index);
	std::string path = fileName ? getSavePath(fileName) : "";
	if (path.empty())
		error("Expected a save file name inside the Saves folder, with no folders in it");
	return path;
}

static int LUA_saveBuild(lua_State* L)
{
	scope("(LUA) saveBuild");

	int args = lua_gettop(L);
	if (args != 1 && args != 2)
	{
		error("Expected 1 or 2 arguments saveBuild(fileName[, omitOwnership])");
		lua_settop(L, 0);
		return 0;
	}

	std::string path = saveFileArgument(L, 1);
	bool omitOwnership = args == 2 && lua_toboolean(L, 2);
	lua_settop(L, 0);

	lua_pushboolean(L, !path.empty() && saveLodBuild(*LUA_pd->bricks, &LUA_pd->prints, path, omitOwnership));
	return 1;
}

static int LUA_loadLodSave(lua_State* L)
{
	scope("(LUA) loadLodSave");

	int args = lua_gettop(L);
	if (args != 1 && args != 4)
	{
		error("Expected 1 or 4 arguments loadLodSave(fileName[, x, y, z])");
		lua_settop(L, 0);
		return 0;
	}

	std::string path = saveFileArgument(L, 1);
	int offsetX = args == 4 ? (int)floor(lua_tonumber(L, 2)) : 0;
	int offsetY = args == 4 ? (int)floor(lua_tonumber(L, 3)) : 0;
	int offsetZ = args == 4 ? (int)floor(lua_tonumber(L, 4)) : 0;
	lua_settop(L, 0);

	int loaded = path.empty() ? -1 : loadLodBuild(*LUA_pd->bricks, &LUA_pd->prints, path, offsetX, offsetY, offsetZ);
	if (loaded < 0)
		lua_pushnil(L);
	else
		lua_pushinteger(L, loaded);
	return 1;
}

//For loadBlocklandSave: the light from addBlocklandLight for a Blockland light type, false if there's none
static bool setBlocklandLight(const std::string& uiName, BrickAttachments& attachments)
{
	auto found = LUA_pd->blocklandLights.find(lowercase(uiName));
	if (found == LUA_pd->blocklandLights.end())
		return false;

	const BrickAttachments& light = found->second;
	attachments.hasLight = true;
	attachments.lightColor = light.lightColor;
	attachments.lightBrightness = light.lightBrightness;
	attachments.lightFlicker = light.lightFlicker;
	attachments.lightBlinkSpeed = light.lightBlinkSpeed;
	attachments.lightBlinkStrength = light.lightBlinkStrength;
	attachments.lightCoronaWidth = light.lightCoronaWidth;
	attachments.lightConeAngle = light.lightConeAngle;
	attachments.lightDirection = light.lightDirection;
	attachments.lightSpin = light.lightSpin;
	attachments.lightOffset = light.lightOffset;
	return true;
}

//For loadBlocklandSave: the emitter type addBlocklandEmitter gave a Blockland emitter name, else the one with that uiName, "" if neither exists
static std::string findBlocklandEmitter(const std::string& uiName)
{
	auto found = LUA_pd->blocklandEmitters.find(lowercase(uiName));
	if (found != LUA_pd->blocklandEmitters.end())
		return emitterTypeExists(found->second) ? found->second : "";

	return findEmitterTypeByUiName(uiName);
}

static int LUA_loadBlocklandSave(lua_State* L)
{
	scope("(LUA) loadBlocklandSave");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument loadBlocklandSave(fileName)");
		lua_settop(L, 0);
		return 0;
	}

	std::string path = saveFileArgument(L, 1);
	lua_settop(L, 0);

	BlocklandAttachmentLookup lookup;
	lookup.setLight = setBlocklandLight;
	lookup.findEmitterType = findBlocklandEmitter;
	lookup.findMusic = findMusicByName;

	int loaded = path.empty() ? -1 : loadBlocklandBuild(*LUA_pd->bricks, LUA_pd->brickTypes, &LUA_pd->prints, path, lookup);
	if (loaded < 0)
		lua_pushnil(L);
	else
		lua_pushinteger(L, loaded);
	return 1;
}

static int LUA_brickGetPosition(lua_State* L)
{
	scope("(LUA) brick:getPosition");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getPosition()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushinteger(L, brick->x);
	lua_pushinteger(L, brick->y);
	lua_pushinteger(L, brick->z);
	return 3;
}

static int LUA_brickGetDimensions(lua_State* L)
{
	scope("(LUA) brick:getDimensions");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getDimensions()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushinteger(L, brick->width);
	lua_pushinteger(L, brick->height);
	lua_pushinteger(L, brick->length);
	return 3;
}

static int LUA_brickGetAngleID(lua_State* L)
{
	scope("(LUA) brick:getAngleID");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getAngleID()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushinteger(L, brick->angleID);
	return 1;
}

static int LUA_brickGetColor(lua_State* L)
{
	scope("(LUA) brick:getColor");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getColor()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	for (int channel = 0; channel < 4; channel++)
		lua_pushnumber(L, brick->color[channel] / 255.0);
	return 4;
}

static int LUA_brickSetColor(lua_State* L)
{
	scope("(LUA) brick:setColor");

	if (lua_gettop(L) != 5)
	{
		error("Expected 5 arguments brick:setColor(r, g, b, a)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	LUA_pd->bricks->setColor(brick, glm::u8vec4(colorByte(lua_tonumber(L, 2)), colorByte(lua_tonumber(L, 3)), colorByte(lua_tonumber(L, 4)), colorByte(lua_tonumber(L, 5))));
	return 0;
}

static int LUA_brickGetMaterial(lua_State* L)
{
	scope("(LUA) brick:getMaterial");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getMaterial()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushstring(L, brickMaterialNames[brick->material]);
	return 1;
}

static int LUA_brickSetMaterial(lua_State* L)
{
	scope("(LUA) brick:setMaterial");

	if (lua_gettop(L) != 2 || !lua_isstring(L, 2))
	{
		error("Expected 2 arguments brick:setMaterial(materialName)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	std::string name = lua_tostring(L, 2);
	int material = findBrickMaterial(name);
	if (material < 0)
	{
		std::string names = "";
		for (int a = 0; a < BrickMaterialCount; a++)
			names += (a > 0 ? ", " : "") + std::string(brickMaterialNames[a]);
		error("No brick material named " + name + ", the materials are " + names);
		return 0;
	}

	LUA_pd->bricks->setMaterial(brick, (unsigned char)material);
	return 0;
}

static int LUA_brickGetPrint(lua_State* L)
{
	scope("(LUA) brick:getPrint");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getPrint()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushstring(L, LUA_pd->prints.getName((int)brick->printID - 1).c_str());
	return 1;
}

static int LUA_brickCanPrint(lua_State* L)
{
	scope("(LUA) brick:canPrint");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:canPrint()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushboolean(L, brickCanPrint(brick));
	return 1;
}

static int LUA_brickSetPrint(lua_State* L)
{
	scope("(LUA) brick:setPrint");

	if (lua_gettop(L) != 2 || !lua_isstring(L, 2))
	{
		error("Expected 2 arguments brick:setPrint(printName)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	std::string name = lua_tostring(L, 2);
	int print = name.empty() ? -1 : LUA_pd->prints.find(name);
	if (!name.empty() && print < 0)
	{
		error("No print named " + name + ", prints are named like \"2x2f/arrow\" after their folder in Assets/brick/prints");
		return 0;
	}

	//Nothing stops a print going on a brick with no printed face, it just won't show
	LUA_pd->bricks->setPrint(brick, (uint16_t)(print + 1));
	return 0;
}

static int LUA_brickIsColliding(lua_State* L)
{
	scope("(LUA) brick:isColliding");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:isColliding()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushboolean(L, brick->collides);
	return 1;
}

static int LUA_brickSetColliding(lua_State* L)
{
	scope("(LUA) brick:setColliding");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments brick:setColliding(collides)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	LUA_pd->bricks->setColliding(brick, lua_toboolean(L, 2));
	return 0;
}

static int LUA_brickGetOwner(lua_State* L)
{
	scope("(LUA) brick:getOwner");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getOwner()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushinteger(L, brick->ownerID);
	return 1;
}

static int LUA_brickGetName(lua_State* L)
{
	scope("(LUA) brick:getName");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getName()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushstring(L, brick->name.c_str());
	return 1;
}

static int LUA_brickSetName(lua_State* L)
{
	scope("(LUA) brick:setName");

	if (lua_gettop(L) != 2 || !lua_isstring(L, 2))
	{
		error("Expected 2 arguments brick:setName(name)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	LUA_pd->bricks->setName(brick, lua_tostring(L, 2));
	return 0;
}

static int LUA_brickIsSpecial(lua_State* L)
{
	scope("(LUA) brick:isSpecial");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:isSpecial()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushboolean(L, brick->isSpecial());
	return 1;
}

static int LUA_brickGetTypeName(lua_State* L)
{
	scope("(LUA) brick:getTypeName");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getTypeName()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	const SpecialBrickType* type = LUA_pd->brickTypes.getSpecial(brick->typeID - 1);
	lua_pushstring(L, type ? type->uiName.c_str() : "");
	return 1;
}

static int LUA_brickGetMusic(lua_State* L)
{
	scope("(LUA) brick:getMusic");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getMusic()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	if (!brick->attachments || brick->attachments->musicName.empty())
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, brick->attachments->musicName.c_str());
	lua_pushnumber(L, brick->attachments->musicVolume);
	lua_pushnumber(L, brick->attachments->musicPitch);
	return 3;
}

static int LUA_brickSetMusic(lua_State* L)
{
	scope("(LUA) brick:setMusic");

	const std::string usage = "brick:setMusic(soundName[, volume, pitch]) or brick:setMusic(nil)";

	int args = lua_gettop(L);
	bool valid = (args == 2 || args == 4) && (lua_isnil(L, 2) || lua_type(L, 2) == LUA_TSTRING) && (args == 2 || (lua_isnumber(L, 3) && lua_isnumber(L, 4)));
	if (!valid)
	{
		error("Expected " + usage);
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	BrickAttachments settings = brick->attachments ? *brick->attachments : BrickAttachments();
	settings.musicName = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);

	if (!settings.musicName.empty() && !soundTypeExists(settings.musicName))
	{
		error("No sound type named " + settings.musicName + ", see newSoundType");
		return 0;
	}

	if (args == 4)
	{
		settings.musicVolume = (float)lua_tonumber(L, 3);
		settings.musicPitch = (float)lua_tonumber(L, 4);
	}

	setBrickAttachments(brick, settings);
	return 0;
}

//Pushes a table like {x, y, z}
static void pushVector(lua_State* L, const glm::vec3& vector)
{
	lua_newtable(L);
	for (int axis = 0; axis < 3; axis++)
	{
		lua_pushnumber(L, vector[axis]);
		lua_rawseti(L, -2, axis + 1);
	}
}

static int LUA_brickGetLight(lua_State* L)
{
	scope("(LUA) brick:getLight");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getLight()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	if (!brick->attachments || !brick->attachments->hasLight)
	{
		lua_pushnil(L);
		return 1;
	}

	pushLightTable(L, *brick->attachments);
	return 1;
}

void pushLightTable(lua_State* L, const BrickAttachments& settings)
{
	lua_newtable(L);

	pushVector(L, settings.lightColor);
	lua_setfield(L, -2, "color");
	pushVector(L, settings.lightDirection);
	lua_setfield(L, -2, "direction");
	pushVector(L, settings.lightOffset);
	lua_setfield(L, -2, "offset");

	const std::pair<const char*, float> numberFields[] = {
		{ "brightness", settings.lightBrightness },
		{ "flicker", settings.lightFlicker },
		{ "blinkSpeed", settings.lightBlinkSpeed },
		{ "blinkStrength", settings.lightBlinkStrength },
		{ "coronaWidth", settings.lightCoronaWidth },
		{ "coneAngle", settings.lightConeAngle },
		{ "spin", settings.lightSpin } };

	for (const auto& field : numberFields)
	{
		lua_pushnumber(L, field.second);
		lua_setfield(L, -2, field.first);
	}
}

//Reads a table of three finite numbers at index into out, false if it's anything else
static bool readVectorTable(lua_State* L, int index, glm::vec3& out)
{
	if (!lua_istable(L, index))
		return false;

	for (int axis = 0; axis < 3; axis++)
	{
		lua_rawgeti(L, index, axis + 1);
		bool number = lua_isnumber(L, -1);
		out[axis] = number ? (float)lua_tonumber(L, -1) : 0.0f;
		lua_pop(L, 1);

		if (!number || !std::isfinite(out[axis]))
			return false;
	}

	return true;
}

bool readLightTable(lua_State* L, int index, BrickAttachments& settings)
{
	const std::pair<const char*, glm::vec3*> vectorFields[] = {
		{ "color", &settings.lightColor },
		{ "direction", &settings.lightDirection },
		{ "offset", &settings.lightOffset } };

	const std::pair<const char*, float*> numberFields[] = {
		{ "brightness", &settings.lightBrightness },
		{ "flicker", &settings.lightFlicker },
		{ "blinkSpeed", &settings.lightBlinkSpeed },
		{ "blinkStrength", &settings.lightBlinkStrength },
		{ "coronaWidth", &settings.lightCoronaWidth },
		{ "coneAngle", &settings.lightConeAngle },
		{ "spin", &settings.lightSpin } };

	lua_pushnil(L);
	while (lua_next(L, index))
	{
		std::string field = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : "";
		int value = lua_gettop(L);
		bool known = false;
		bool fits = false;

		for (const auto& vectorField : vectorFields)
		{
			if (field == vectorField.first)
			{
				known = true;
				fits = readVectorTable(L, value, *vectorField.second);
			}
		}

		for (const auto& numberField : numberFields)
		{
			if (field == numberField.first)
			{
				known = true;
				fits = lua_isnumber(L, value);
				*numberField.second = fits ? (float)lua_tonumber(L, value) : 0.0f;
			}
		}

		if (!known || !fits)
		{
			error(known ? "Light field " + field + " has the wrong kind of value, see LuaAPI.md" : "Lights have no field named " + field + ", see LuaAPI.md");
			lua_pop(L, 2);
			return false;
		}

		lua_pop(L, 1);
	}

	if (glm::length(settings.lightDirection) < 0.0001f)
	{
		error("A light's direction can't be 0, 0, 0");
		return false;
	}

	return true;
}

static int LUA_brickSetLight(lua_State* L)
{
	scope("(LUA) brick:setLight");

	const std::string usage = "brick:setLight(table) or brick:setLight(nil)";

	if (lua_gettop(L) != 2 || !(lua_istable(L, 2) || lua_isnil(L, 2)))
	{
		error("Expected " + usage);
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	BrickAttachments settings = brick->attachments ? *brick->attachments : BrickAttachments();

	if (lua_isnil(L, 2))
	{
		settings.hasLight = false;
		setBrickAttachments(brick, settings);
		return 0;
	}

	//Fields left out keep the light's current values, or a new light's
	if (!settings.hasLight)
		settings.resetLight();
	settings.hasLight = true;

	if (!readLightTable(L, 2, settings))
	{
		lua_settop(L, 0);
		return 0;
	}

	setBrickAttachments(brick, settings);
	return 0;
}

static int LUA_addBlocklandLight(lua_State* L)
{
	scope("(LUA) addBlocklandLight");

	if (lua_gettop(L) != 2 || lua_type(L, 1) != LUA_TSTRING || !(lua_istable(L, 2) || lua_isnil(L, 2)))
	{
		error("Expected addBlocklandLight(uiName, table) or addBlocklandLight(uiName, nil)");
		lua_settop(L, 0);
		return 0;
	}

	std::string uiName = lowercase(lua_tostring(L, 1));
	if (uiName.empty() || uiName.length() > BrickAttachments::maxNameLength)
	{
		error("A Blockland light type's name has to be 1-255 characters");
		lua_settop(L, 0);
		return 0;
	}

	if (lua_isnil(L, 2))
	{
		LUA_pd->blocklandLights.erase(uiName);
		lua_settop(L, 0);
		return 0;
	}

	//Fields left out get a new light's defaults
	BrickAttachments light;
	light.hasLight = true;

	if (!readLightTable(L, 2, light))
	{
		lua_settop(L, 0);
		return 0;
	}

	light.clampValues();
	LUA_pd->blocklandLights[uiName] = light;

	lua_settop(L, 0);
	return 0;
}

static int LUA_addBlocklandEmitter(lua_State* L)
{
	scope("(LUA) addBlocklandEmitter");

	if (lua_gettop(L) != 2 || lua_type(L, 1) != LUA_TSTRING || !(lua_type(L, 2) == LUA_TSTRING || lua_isnil(L, 2)))
	{
		error("Expected addBlocklandEmitter(uiName, emitterTypeName) or addBlocklandEmitter(uiName, nil)");
		lua_settop(L, 0);
		return 0;
	}

	std::string uiName = lowercase(lua_tostring(L, 1));
	std::string typeName = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);
	lua_settop(L, 0);

	if (uiName.empty() || uiName.length() > BrickAttachments::maxNameLength)
	{
		error("A Blockland emitter's name has to be 1-255 characters");
		return 0;
	}

	if (typeName.empty())
	{
		LUA_pd->blocklandEmitters.erase(uiName);
		return 0;
	}

	if (!emitterTypeExists(typeName))
	{
		error("There's no emitter type named " + typeName);
		return 0;
	}

	LUA_pd->blocklandEmitters[uiName] = typeName;
	return 0;
}

static int LUA_brickGetEmitter(lua_State* L)
{
	scope("(LUA) brick:getEmitter");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getEmitter()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	if (!brick->attachments || brick->attachments->emitterName.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, brick->attachments->emitterName.c_str());
	return 1;
}

static int LUA_brickSetEmitter(lua_State* L)
{
	scope("(LUA) brick:setEmitter");

	if (lua_gettop(L) != 2 || !(lua_isnil(L, 2) || lua_type(L, 2) == LUA_TSTRING))
	{
		error("Expected brick:setEmitter(emitterTypeName) or brick:setEmitter(nil)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	BrickAttachments settings = brick->attachments ? *brick->attachments : BrickAttachments();
	settings.emitterName = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);

	if (!settings.emitterName.empty() && !emitterTypeExists(settings.emitterName))
	{
		error("There's no emitter type named " + settings.emitterName);
		return 0;
	}

	setBrickAttachments(brick, settings);
	return 0;
}

static int LUA_brickGetVehicleSpawn(lua_State* L)
{
	scope("(LUA) brick:getVehicleSpawn");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getVehicleSpawn()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	if (!brick->attachments || brick->attachments->vehicleSpawnName.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, brick->attachments->vehicleSpawnName.c_str());
	return 1;
}

static int LUA_brickSetVehicleSpawn(lua_State* L)
{
	scope("(LUA) brick:setVehicleSpawn");

	if (lua_gettop(L) != 2 || !(lua_isnil(L, 2) || lua_type(L, 2) == LUA_TSTRING))
	{
		error("Expected brick:setVehicleSpawn(spawnName) or brick:setVehicleSpawn(nil)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	BrickAttachments settings = brick->attachments ? *brick->attachments : BrickAttachments();
	settings.vehicleSpawnName = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);

	if (!settings.vehicleSpawnName.empty() && !vehicleSpawnExists(settings.vehicleSpawnName))
	{
		error("There's no vehicle spawn named " + settings.vehicleSpawnName + ", see registerVehicleSpawn");
		return 0;
	}

	setBrickAttachments(brick, settings);
	return 0;
}

static int LUA_brickGetSpawnedVehicle(lua_State* L)
{
	scope("(LUA) brick:getSpawnedVehicle");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getSpawnedVehicle()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	std::shared_ptr<Vehicle> vehicle = getBrickSpawnedVehicle(brick);
	if (vehicle)
		LUA_pd->vehicles->pushLua(L, vehicle);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_brickGetItemSpawn(lua_State* L)
{
	scope("(LUA) brick:getItemSpawn");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getItemSpawn()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	if (!brick->attachments || brick->attachments->itemSpawnName.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, brick->attachments->itemSpawnName.c_str());
	return 1;
}

static int LUA_brickSetItemSpawn(lua_State* L)
{
	scope("(LUA) brick:setItemSpawn");

	if (lua_gettop(L) != 2 || !(lua_isnil(L, 2) || lua_type(L, 2) == LUA_TSTRING))
	{
		error("Expected brick:setItemSpawn(itemTypeName) or brick:setItemSpawn(nil)");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	BrickAttachments settings = brick->attachments ? *brick->attachments : BrickAttachments();
	settings.itemSpawnName = lua_isnil(L, 2) ? "" : lua_tostring(L, 2);

	if (!settings.itemSpawnName.empty())
	{
		bool found = false;
		for (const std::shared_ptr<DynamicType>& type : LUA_pd->dynamicTypes)
			found = found || (type->isItemType && type->scriptName == settings.itemSpawnName);

		if (!found)
		{
			error("There's no item type named " + settings.itemSpawnName + ", see newItemType");
			return 0;
		}
	}

	setBrickAttachments(brick, settings);
	return 0;
}

static int LUA_brickGetDisplayItem(lua_State* L)
{
	scope("(LUA) brick:getDisplayItem");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:getDisplayItem()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	std::shared_ptr<Item> item = getBrickDisplayItem(brick);
	if (item)
		LUA_pd->dynamics->pushLua(L, item);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_brickIsVehicleSpawn(lua_State* L)
{
	scope("(LUA) brick:isVehicleSpawn");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument brick:isVehicleSpawn()");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	lua_pushboolean(L, brickIsVehicleSpawn(brick));
	return 1;
}

static int LUA_brickRemove(lua_State* L)
{
	scope("(LUA) brick:remove");

	int args = lua_gettop(L);
	if (args != 1 && args != 2)
	{
		error("Expected 1 or 2 arguments brick:remove([showEffect])");
		return 0;
	}

	Brick* brick = brickArgument(L);
	if (!brick)
		return 0;

	LUA_pd->bricks->remove(brick, args == 2 && lua_toboolean(L, 2));
	return 0;
}

luaL_Reg* getBrickFunctions(lua_State* L)
{
	lua_register(L, "addBrick", LUA_addBrick);
	lua_register(L, "addSpecialBrick", LUA_addSpecialBrick);
	lua_register(L, "getNumBricks", LUA_getNumBricks);
	lua_register(L, "getBrickIdx", LUA_getBrickIdx);
	lua_register(L, "getNumNamedBricks", LUA_getNumNamedBricks);
	lua_register(L, "getNamedBrickIdx", LUA_getNamedBrickIdx);
	lua_register(L, "getBrickId", LUA_getBrickId);
	lua_register(L, "getBrickAt", LUA_getBrickAt);
	lua_register(L, "clearAllBricks", LUA_clearAllBricks);
	lua_register(L, "saveBuild", LUA_saveBuild);
	lua_register(L, "loadLodSave", LUA_loadLodSave);
	lua_register(L, "loadBlocklandSave", LUA_loadBlocklandSave);
	lua_register(L, "addBlocklandLight", LUA_addBlocklandLight);
	lua_register(L, "addBlocklandEmitter", LUA_addBlocklandEmitter);

	luaL_Reg* methods = new luaL_Reg[32];
	methods[0] = { "getPosition", LUA_brickGetPosition };
	methods[1] = { "getDimensions", LUA_brickGetDimensions };
	methods[2] = { "getAngleID", LUA_brickGetAngleID };
	methods[3] = { "getColor", LUA_brickGetColor };
	methods[4] = { "setColor", LUA_brickSetColor };
	methods[5] = { "isColliding", LUA_brickIsColliding };
	methods[6] = { "setColliding", LUA_brickSetColliding };
	methods[7] = { "getOwner", LUA_brickGetOwner };
	methods[8] = { "getName", LUA_brickGetName };
	methods[9] = { "setName", LUA_brickSetName };
	methods[10] = { "remove", LUA_brickRemove };
	methods[11] = { "isSpecial", LUA_brickIsSpecial };
	methods[12] = { "getTypeName", LUA_brickGetTypeName };
	methods[13] = { "getMusic", LUA_brickGetMusic };
	methods[14] = { "setMusic", LUA_brickSetMusic };
	methods[15] = { "getLight", LUA_brickGetLight };
	methods[16] = { "setLight", LUA_brickSetLight };
	methods[17] = { "getEmitter", LUA_brickGetEmitter };
	methods[18] = { "setEmitter", LUA_brickSetEmitter };
	methods[19] = { "getMaterial", LUA_brickGetMaterial };
	methods[20] = { "setMaterial", LUA_brickSetMaterial };
	methods[21] = { "getPrint", LUA_brickGetPrint };
	methods[22] = { "setPrint", LUA_brickSetPrint };
	methods[23] = { "canPrint", LUA_brickCanPrint };
	methods[24] = { "getVehicleSpawn", LUA_brickGetVehicleSpawn };
	methods[25] = { "setVehicleSpawn", LUA_brickSetVehicleSpawn };
	methods[26] = { "getSpawnedVehicle", LUA_brickGetSpawnedVehicle };
	methods[27] = { "getItemSpawn", LUA_brickGetItemSpawn };
	methods[28] = { "setItemSpawn", LUA_brickSetItemSpawn };
	methods[29] = { "getDisplayItem", LUA_brickGetDisplayItem };
	methods[30] = { "isVehicleSpawn", LUA_brickIsVehicleSpawn };
	methods[31] = { NULL, NULL };
	return methods;
}
