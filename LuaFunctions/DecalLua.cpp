#include "DecalLua.h"

#include <random>

//Decals per DecalUpdateAdd packet, which keeps each one under the MTU
static constexpr size_t decalsPerPacket = 30;

//As many as a DecalUpdateAdd packet's 2 byte limit can say
static constexpr unsigned int mostDecals = 65535;

static ENetPacket* makeDecalTypePacket(uint16_t id, const ServerProgramData::DecalType& type)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(DecalUpdate);
	bytes.push_back(DecalUpdateType);
	bytes.push_back(id & 255);
	bytes.push_back(id >> 8);
	bytes.push_back((unsigned char)type.name.length());
	bytes.insert(bytes.end(), type.name.begin(), type.name.end());
	bytes.push_back((unsigned char)type.materialPath.length());
	bytes.insert(bytes.end(), type.materialPath.begin(), type.materialPath.end());
	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation));
}

//Packets holding decals from first up to but not including last
template <typename Iterator>
static std::vector<ENetPacket*> makeDecalPackets(const ServerProgramData* pd, Iterator first, Iterator last)
{
	std::vector<ENetPacket*> packets;

	while (first != last)
	{
		size_t count = std::min(decalsPerPacket, (size_t)std::distance(first, last));

		ENetPacket* packet = enet_packet_create(NULL, 2 + sizeof(uint16_t) * 2 + count * WorldDecal::recordBytes, getFlagsFromChannel(BrickLoading));
		packet->data[0] = (unsigned char)DecalUpdate;
		packet->data[1] = (unsigned char)DecalUpdateAdd;

		uint16_t maxDecals = (uint16_t)pd->maxDecals;
		uint16_t count16 = (uint16_t)count;
		memcpy(packet->data + 2, &maxDecals, sizeof(uint16_t));
		memcpy(packet->data + 2 + sizeof(uint16_t), &count16, sizeof(uint16_t));

		size_t byteIterator = 2 + sizeof(uint16_t) * 2;
		for (size_t a = 0; a < count; a++, ++first)
		{
			first->write(packet->data + byteIterator);
			byteIterator += WorldDecal::recordBytes;
		}

		packets.push_back(packet);
	}

	return packets;
}

void sendDecalTypes(const ServerProgramData* pd, JoinedClient* client)
{
	for (size_t a = 0; a < pd->decalTypes.size(); a++)
		client->send(makeDecalTypePacket((uint16_t)a, pd->decalTypes[a]), JoinNegotiation);
}

void sendDecals(const ServerProgramData* pd, JoinedClient* client)
{
	//The ones made this tick are about to go to everyone, them included, see sendNewDecals
	size_t alreadyOut = pd->decals.size() - std::min(pd->decals.size(), pd->pendingDecals.size());

	for (ENetPacket* packet : makeDecalPackets(pd, pd->decals.begin(), pd->decals.begin() + alreadyOut))
		client->send(packet, BrickLoading);
}

void sendNewDecals(const ServerProgramData* pd, Server* server)
{
	//Clients notice for themselves when the brick under one goes, see WorldDecals::update
	for (auto iter = pd->decals.begin(); iter != pd->decals.end();)
	{
		if (iter->brickID != NO_ID && !pd->bricks->find(iter->brickID))
			iter = pd->decals.erase(iter);
		else
			++iter;
	}

	for (ENetPacket* packet : makeDecalPackets(pd, pd->pendingDecals.begin(), pd->pendingDecals.end()))
		server->broadcast(packet, BrickLoading);
	pd->pendingDecals.clear();

	//No decals in it, but the limit it carries has clients drop their oldest down to it, as setMaxDecals did here
	if (pd->decalLimitChanged)
	{
		pd->decalLimitChanged = false;

		ENetPacket* packet = enet_packet_create(NULL, 2 + sizeof(uint16_t) * 2, getFlagsFromChannel(BrickLoading));
		packet->data[0] = (unsigned char)DecalUpdate;
		packet->data[1] = (unsigned char)DecalUpdateAdd;
		uint16_t maxDecals = (uint16_t)pd->maxDecals;
		uint16_t count = 0;
		memcpy(packet->data + 2, &maxDecals, sizeof(uint16_t));
		memcpy(packet->data + 2 + sizeof(uint16_t), &count, sizeof(uint16_t));
		server->broadcast(packet, BrickLoading);
	}
}

static int LUA_addDecalType(lua_State* L)
{
	scope("(LUA) addDecalType");

	int args = lua_gettop(L);
	if (args != 2 || lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING)
	{
		error("Expected addDecalType(name, materialPath)");
		lua_pop(L, args);
		return 0;
	}

	ServerProgramData::DecalType type;
	type.name = lua_tostring(L, 1);
	type.materialPath = lua_tostring(L, 2);
	lua_pop(L, args);

	if (type.name.empty() || type.name.length() > 255)
	{
		error("A decal type's name has to be 1 to 255 characters");
		return 0;
	}

	if (type.materialPath.length() > 255 || !isPathInsideGameFolder(type.materialPath))
	{
		error("A decal type's material has to be a relative path inside the game folder, 255 characters at most: " + type.materialPath);
		return 0;
	}

	//Clients load their own copy from the same path, this is only so a typo shows up here rather than as nothing drawn
	if (!doesFileExist(type.materialPath))
	{
		error("Decal material " + type.materialPath + " doesn't exist");
		return 0;
	}

	//Adding a name again gives it a new material and keeps its ID
	size_t id = LUA_pd->decalTypes.size();
	for (size_t a = 0; a < LUA_pd->decalTypes.size(); a++)
	{
		if (LUA_pd->decalTypes[a].name == type.name)
			id = a;
	}

	if (id >= 65535)
	{
		error("Too many decal types");
		return 0;
	}

	if (id == LUA_pd->decalTypes.size())
		LUA_pd->decalTypes.push_back(type);
	else
		LUA_pd->decalTypes[id] = type;

	//Anyone already here, anyone who joins later gets it in sendDecalTypes
	LUA_server->broadcast(makeDecalTypePacket((uint16_t)id, type), JoinNegotiation);

	lua_pushinteger(L, (lua_Integer)id);
	return 1;
}

static int LUA_addDecal(lua_State* L)
{
	scope("(LUA) addDecal");

	int args = lua_gettop(L);
	bool okay = args >= 7 && args <= 10;
	for (int a = 2; a <= 7 && okay; a++)
		okay = lua_isnumber(L, a);
	okay = okay && (lua_type(L, 1) == LUA_TSTRING || lua_isinteger(L, 1));
	okay = okay && (args < 8 || lua_isnil(L, 8) || lua_isnumber(L, 8));
	okay = okay && (args < 9 || lua_isnil(L, 9) || lua_istable(L, 9));
	okay = okay && (args < 10 || lua_isnil(L, 10) || lua_isnumber(L, 10));

	if (!okay)
	{
		error("Expected addDecal(type, x, y, z, normalX, normalY, normalZ[, size[, brick[, roll]]])");
		lua_pop(L, args);
		return 0;
	}

	WorldDecal decal;

	//By name like emitters, or by the ID addDecalType gave
	int typeID = -1;
	if (lua_type(L, 1) == LUA_TSTRING)
	{
		std::string name = lua_tostring(L, 1);
		for (size_t a = 0; a < LUA_pd->decalTypes.size(); a++)
		{
			if (LUA_pd->decalTypes[a].name == name)
				typeID = (int)a;
		}

		if (typeID == -1)
		{
			error("No decal type named " + name);
			lua_pop(L, args);
			return 0;
		}
	}
	else
	{
		lua_Integer id = lua_tointeger(L, 1);
		if (id < 0 || (size_t)id >= LUA_pd->decalTypes.size())
		{
			error("No decal type with ID " + std::to_string(id));
			lua_pop(L, args);
			return 0;
		}
		typeID = (int)id;
	}
	decal.typeID = (uint16_t)typeID;

	decal.position = glm::vec3(lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	decal.normal = glm::vec3(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	if (args >= 8 && !lua_isnil(L, 8))
		decal.size = (float)lua_tonumber(L, 8);

	//Turned at random unless told how, so a wall of holes doesn't look stamped, and the same for every client
	if (args >= 10 && !lua_isnil(L, 10))
		decal.roll = (float)lua_tonumber(L, 10);
	else
	{
		static std::mt19937 random(std::random_device{}());
		decal.roll = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(random);
	}

	bool hasBrick = args >= 9 && !lua_isnil(L, 9);
	if (hasBrick)
	{
		//popLua wants the brick on top
		lua_settop(L, 9);
		Brick* brick = LUA_pd->bricks->popLua(L);
		if (!brick)
		{
			lua_settop(L, 0);
			return 0;
		}
		decal.brickID = brick->netId;
	}
	lua_settop(L, 0);

	float length = glm::length(decal.normal);
	if (length < 0.0001f || !std::isfinite(length) || !std::isfinite(decal.position.x + decal.position.y + decal.position.z))
	{
		error("A decal needs a real position, and a normal that isn't zero");
		return 0;
	}
	decal.normal /= length;

	if (!(decal.size > 0.0f) || decal.size > 64.0f)
	{
		error("A decal's size has to be above 0 and no more than 64 studs");
		return 0;
	}

	LUA_pd->decals.push_back(decal);
	while (LUA_pd->decals.size() > LUA_pd->maxDecals)
		LUA_pd->decals.pop_front();
	LUA_pd->pendingDecals.push_back(decal);

	return 0;
}

static int LUA_clearDecals(lua_State* L)
{
	lua_pop(L, lua_gettop(L));

	LUA_pd->decals.clear();
	LUA_pd->pendingDecals.clear();

	//On the channel decals come in on, so it can't overtake ones sent before it
	ENetPacket* packet = enet_packet_create(NULL, 2, getFlagsFromChannel(BrickLoading));
	packet->data[0] = (unsigned char)DecalUpdate;
	packet->data[1] = (unsigned char)DecalUpdateClear;
	LUA_server->broadcast(packet, BrickLoading);

	return 0;
}

static int LUA_setMaxDecals(lua_State* L)
{
	scope("(LUA) setMaxDecals");

	int args = lua_gettop(L);
	if (args != 1 || !lua_isnumber(L, 1))
	{
		error("Expected setMaxDecals(amount)");
		lua_pop(L, args);
		return 0;
	}

	lua_Number amount = lua_tonumber(L, 1);
	lua_pop(L, args);

	LUA_pd->maxDecals = (unsigned int)std::clamp(amount, (lua_Number)0, (lua_Number)mostDecals);
	while (LUA_pd->decals.size() > LUA_pd->maxDecals)
		LUA_pd->decals.pop_front();

	LUA_pd->decalLimitChanged = true;

	return 0;
}

static int LUA_getMaxDecals(lua_State* L)
{
	lua_pop(L, lua_gettop(L));
	lua_pushinteger(L, LUA_pd->maxDecals);
	return 1;
}

static int LUA_getNumDecals(lua_State* L)
{
	lua_pop(L, lua_gettop(L));
	lua_pushinteger(L, (lua_Integer)LUA_pd->decals.size());
	return 1;
}

void registerDecalFunctions(lua_State* L)
{
	lua_register(L, "addDecalType", LUA_addDecalType);
	lua_register(L, "addDecal", LUA_addDecal);
	lua_register(L, "clearDecals", LUA_clearDecals);
	lua_register(L, "setMaxDecals", LUA_setMaxDecals);
	lua_register(L, "getMaxDecals", LUA_getMaxDecals);
	lua_register(L, "getNumDecals", LUA_getNumDecals);
}
