#include "EmitterLua.h"

#include <cmath>
#include <sstream>

template <typename T>
static void put(std::vector<unsigned char>& bytes, const T& value)
{
	size_t at = bytes.size();
	bytes.resize(at + sizeof(T));
	memcpy(bytes.data() + at, &value, sizeof(T));
}

static ENetPacket* makeParticleTypePacket(size_t id, const ParticleTypeData& data)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(ParticleEmitterType);
	bytes.push_back(ParticleTypeKind);
	put(bytes, (uint16_t)id);
	data.write(bytes);
	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation));
}

static ENetPacket* makeEmitterTypePacket(size_t id, const EmitterTypeData& data)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(ParticleEmitterType);
	bytes.push_back(EmitterTypeKind);
	put(bytes, (uint16_t)id);
	data.write(bytes);
	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation));
}

void sendParticleEmitterTypes(const ServerProgramData* pd, JoinedClient* client)
{
	//Particle types first, emitter types refer to them
	for (size_t a = 0; a < pd->particleTypes.size(); a++)
		client->send(makeParticleTypePacket(a, pd->particleTypes[a]), JoinNegotiation);

	for (size_t a = 0; a < pd->emitterTypes.size(); a++)
		client->send(makeEmitterTypePacket(a, pd->emitterTypes[a]), JoinNegotiation);
}

static int findParticleType(const std::string& name)
{
	for (size_t a = 0; a < LUA_pd->particleTypes.size(); a++)
	{
		if (LUA_pd->particleTypes[a].name == name)
			return (int)a;
	}
	return -1;
}

static int findEmitterType(const std::string& name)
{
	for (size_t a = 0; a < LUA_pd->emitterTypes.size(); a++)
	{
		if (LUA_pd->emitterTypes[a].name == name)
			return (int)a;
	}
	return -1;
}

std::shared_ptr<Emitter> spawnEmitterAt(const std::string& typeName, const glm::vec3& position)
{
	if (!LUA_pd || !LUA_pd->emitters)
		return nullptr;

	int type = findEmitterType(typeName);
	if (type == -1)
		return nullptr;

	return LUA_pd->emitters->create((uint16_t)type, position);
}

std::string getEmitterTypeName(const Emitter& emitter)
{
	if (!LUA_pd || emitter.getTypeID() >= LUA_pd->emitterTypes.size())
		return "";

	return LUA_pd->emitterTypes[emitter.getTypeID()].name;
}

bool emitterTypeExists(const std::string& typeName)
{
	return LUA_pd && findEmitterType(typeName) != -1;
}

int findEmitterTypeIndex(const std::string& typeName)
{
	return LUA_pd ? findEmitterType(typeName) : -1;
}

std::string findEmitterTypeByUiName(const std::string& uiName)
{
	if (!LUA_pd || uiName.empty())
		return "";

	//Later ones win, like datablocks with the same uiName in Blockland
	std::string wanted = lowercase(uiName);
	for (size_t a = LUA_pd->emitterTypes.size(); a-- > 0;)
	{
		if (lowercase(LUA_pd->emitterTypes[a].uiName) == wanted)
			return LUA_pd->emitterTypes[a].name;
	}

	return "";
}

//Reads count numbers starting at stack index first into out, logging usage and returning false if any is missing or not a finite number
static bool readNumbers(lua_State* L, int first, int count, float* out, const std::string& usage)
{
	for (int a = 0; a < count; a++)
	{
		if (!lua_isnumber(L, first + a))
		{
			error("Argument " + std::to_string(a + 1) + " isn't a number: " + usage);
			return false;
		}

		out[a] = (float)lua_tonumber(L, first + a);
		if (!std::isfinite(out[a]))
		{
			error("Argument " + std::to_string(a + 1) + " isn't a finite number: " + usage);
			return false;
		}
	}

	return true;
}

/*
	Reads count numbers from the value at index into out: a table like {1, 0.5, 0}, or a string like "1 0.5 0" the way the old game's scripts wrote them
	With allowSingle, one plain number sets every component
*/
static bool readVector(lua_State* L, int index, int count, float* out, bool allowSingle)
{
	index = lua_absindex(L, index);
	std::vector<float> numbers;

	if (lua_type(L, index) == LUA_TNUMBER)
	{
		if (!allowSingle)
			return false;
		numbers.assign(count, (float)lua_tonumber(L, index));
	}
	else if (lua_type(L, index) == LUA_TSTRING)
	{
		std::istringstream words(lua_tostring(L, index));
		std::string word;
		while (words >> word)
		{
			char* end = nullptr;
			float value = std::strtof(word.c_str(), &end);
			if (end == word.c_str() || *end != '\0')
				return false;
			numbers.push_back(value);
		}
	}
	else if (lua_istable(L, index))
	{
		lua_Integer length = (lua_Integer)lua_rawlen(L, index);
		for (lua_Integer a = 1; a <= length; a++)
		{
			lua_rawgeti(L, index, a);
			bool isNumber = lua_type(L, -1) == LUA_TNUMBER;
			if (isNumber)
				numbers.push_back((float)lua_tonumber(L, -1));
			lua_pop(L, 1);

			if (!isNumber)
				return false;
		}
	}

	if ((int)numbers.size() != count)
		return false;

	for (int a = 0; a < count; a++)
	{
		if (!std::isfinite(numbers[a]))
			return false;
		out[a] = numbers[a];
	}

	return true;
}

//The value on top of the stack, if it's a finite number
static bool readNumberField(lua_State* L, float& out)
{
	if (lua_type(L, -1) != LUA_TNUMBER)
		return false;

	float value = (float)lua_tonumber(L, -1);
	if (!std::isfinite(value))
		return false;

	out = value;
	return true;
}

//0 to 3 if key is prefix followed by that digit, like color2, -1 otherwise
static int keyIndex(const std::string& key, const std::string& prefix)
{
	if (key.length() != prefix.length() + 1 || key.compare(0, prefix.length(), prefix) != 0)
		return -1;

	char digit = key.back();
	return (digit >= '0' && digit <= '3') ? digit - '0' : -1;
}

static void pushNumbers(lua_State* L, const float* values, int count)
{
	lua_createtable(L, count, 0);
	for (int a = 0; a < count; a++)
	{
		lua_pushnumber(L, values[a]);
		lua_rawseti(L, -2, a + 1);
	}
}

static int LUA_addParticleType(lua_State* L)
{
	scope("(LUA) addParticleType");

	if (lua_gettop(L) != 2 || lua_type(L, 1) != LUA_TSTRING || !lua_istable(L, 2))
	{
		error("Expected addParticleType(name, table)");
		lua_settop(L, 0);
		return 0;
	}

	ParticleTypeData data;
	data.name = lua_tostring(L, 1);
	const std::string context = "addParticleType " + data.name + ": ";

	const std::pair<const char*, float*> numberFields[] = {
		{ "inheritedVelFactor", &data.inheritedVelFactor },
		{ "lifetimeMS", &data.lifetimeMS },
		{ "lifetimeVarianceMS", &data.lifetimeVarianceMS },
		{ "spinSpeed", &data.spinSpeed } };

	const std::pair<const char*, bool*> boolFields[] = {
		{ "useInvAlpha", &data.useInvAlpha },
		{ "needsSorting", &data.needsSorting },
		{ "lit", &data.lit } };

	bool valid = true;
	bool hasTexture = false;

	lua_pushnil(L);
	while (lua_next(L, 2))
	{
		//lua_tostring would turn a number key into a string in place, which breaks lua_next
		if (lua_type(L, -2) != LUA_TSTRING)
		{
			error(context + "ignoring a field that isn't named");
			lua_pop(L, 1);
			continue;
		}

		std::string key = lua_tostring(L, -2);
		bool known = true;
		bool fieldValid = true;
		int keyed = -1;

		if (key == "texture")
		{
			fieldValid = lua_type(L, -1) == LUA_TSTRING;
			if (fieldValid)
			{
				data.texturePath = lua_tostring(L, -1);
				hasTexture = true;
			}
		}
		else if ((keyed = keyIndex(key, "color")) != -1)
			fieldValid = readVector(L, -1, 4, &data.colors[keyed][0], false);
		else if ((keyed = keyIndex(key, "size")) != -1)
			fieldValid = readNumberField(L, data.sizes[keyed]);
		else if ((keyed = keyIndex(key, "time")) != -1)
			fieldValid = readNumberField(L, data.times[keyed]);
		else if (key == "drag")
			fieldValid = readVector(L, -1, 3, &data.drag[0], true);
		else if (key == "gravity")
			fieldValid = readVector(L, -1, 3, &data.gravity[0], false);
		else
		{
			known = false;

			for (const auto& field : numberFields)
			{
				if (key == field.first)
				{
					known = true;
					fieldValid = readNumberField(L, *field.second);
				}
			}

			for (const auto& field : boolFields)
			{
				if (key == field.first)
				{
					known = true;
					fieldValid = lua_isboolean(L, -1);
					*field.second = lua_toboolean(L, -1);
				}
			}
		}

		if (!known)
		{
			error(context + "there's no particle type field named " + key);
			valid = false;
		}
		else if (!fieldValid)
		{
			error(context + key + " has a value of the wrong kind, see LuaAPI.md");
			valid = false;
		}

		lua_pop(L, 1);
	}

	lua_settop(L, 0);

	if (!valid)
		return 0;

	if (data.name.length() < 1 || data.name.length() > 255)
	{
		error("Particle type names have to be 1-255 characters");
		return 0;
	}

	if (!hasTexture || data.texturePath.length() < 1 || data.texturePath.length() > 255)
	{
		error(context + "needs a texture path of 1-255 characters");
		return 0;
	}

	if (!std::filesystem::exists(data.texturePath))
	{
		error(context + "texture " + data.texturePath + " doesn't exist");
		return 0;
	}

	data.clampValues();

	int id = findParticleType(data.name);
	if (id == -1)
	{
		if (LUA_pd->particleTypes.size() >= 65535)
		{
			error("Too many particle types, can't add " + data.name);
			return 0;
		}

		id = (int)LUA_pd->particleTypes.size();
		LUA_pd->particleTypes.push_back(data);
	}
	else
		LUA_pd->particleTypes[id] = data;

	//Anyone already here, anyone who joins later gets it in sendParticleEmitterTypes
	LUA_server->broadcast(makeParticleTypePacket(id, data), JoinNegotiation);

	return 0;
}

static int LUA_addEmitterType(lua_State* L)
{
	scope("(LUA) addEmitterType");

	if (lua_gettop(L) != 2 || lua_type(L, 1) != LUA_TSTRING || !lua_istable(L, 2))
	{
		error("Expected addEmitterType(name, table)");
		lua_settop(L, 0);
		return 0;
	}

	EmitterTypeData data;
	data.name = lua_tostring(L, 1);
	const std::string context = "addEmitterType " + data.name + ": ";

	const std::pair<const char*, float*> numberFields[] = {
		{ "ejectionPeriodMS", &data.ejectionPeriodMS },
		{ "periodVarianceMS", &data.periodVarianceMS },
		{ "ejectionVelocity", &data.ejectionVelocity },
		{ "velocityVariance", &data.velocityVariance },
		{ "ejectionOffset", &data.ejectionOffset },
		{ "thetaMin", &data.thetaMin },
		{ "thetaMax", &data.thetaMax },
		{ "phiReferenceVel", &data.phiReferenceVel },
		{ "phiVariance", &data.phiVariance },
		{ "lifetimeMS", &data.lifetimeMS } };

	bool valid = true;

	lua_pushnil(L);
	while (lua_next(L, 2))
	{
		if (lua_type(L, -2) != LUA_TSTRING)
		{
			error(context + "ignoring a field that isn't named");
			lua_pop(L, 1);
			continue;
		}

		std::string key = lua_tostring(L, -2);
		bool known = true;
		bool fieldValid = true;

		if (key == "particles")
		{
			//Names separated by spaces like the old game's scripts, or a table of names
			std::vector<std::string> names;
			if (lua_type(L, -1) == LUA_TSTRING)
			{
				std::istringstream words(lua_tostring(L, -1));
				std::string word;
				while (words >> word)
					names.push_back(word);
			}
			else if (lua_istable(L, -1))
			{
				lua_Integer length = (lua_Integer)lua_rawlen(L, -1);
				for (lua_Integer a = 1; a <= length && fieldValid; a++)
				{
					lua_rawgeti(L, -1, a);
					fieldValid = lua_type(L, -1) == LUA_TSTRING;
					if (fieldValid)
						names.push_back(lua_tostring(L, -1));
					lua_pop(L, 1);
				}
			}
			else
				fieldValid = false;

			for (const std::string& name : names)
			{
				int particleType = findParticleType(name);
				if (particleType == -1)
				{
					error(context + "there's no particle type named " + name);
					valid = false;
				}
				else
					data.particleTypes.push_back((uint16_t)particleType);
			}
		}
		else if (key == "uiName")
		{
			fieldValid = lua_type(L, -1) == LUA_TSTRING;
			if (fieldValid)
				data.uiName = lua_tostring(L, -1);
		}
		else
		{
			known = false;

			for (const auto& field : numberFields)
			{
				if (key == field.first)
				{
					known = true;
					fieldValid = readNumberField(L, *field.second);
				}
			}
		}

		if (!known)
		{
			error(context + "there's no emitter type field named " + key);
			valid = false;
		}
		else if (!fieldValid)
		{
			error(context + key + " has a value of the wrong kind, see LuaAPI.md");
			valid = false;
		}

		lua_pop(L, 1);
	}

	lua_settop(L, 0);

	if (!valid)
		return 0;

	if (data.name.length() < 1 || data.name.length() > 255 || data.uiName.length() > 255)
	{
		error("Emitter type names have to be 1-255 characters, uiName up to 255");
		return 0;
	}

	if (data.particleTypes.empty() || data.particleTypes.size() > EmitterTypeData::maxParticleTypes)
	{
		error(context + "needs 1-" + std::to_string(EmitterTypeData::maxParticleTypes) + " particle types in particles");
		return 0;
	}

	data.clampValues();

	int id = findEmitterType(data.name);
	if (id == -1)
	{
		if (LUA_pd->emitterTypes.size() >= 65535)
		{
			error("Too many emitter types, can't add " + data.name);
			return 0;
		}

		id = (int)LUA_pd->emitterTypes.size();
		LUA_pd->emitterTypes.push_back(data);
	}
	else
		LUA_pd->emitterTypes[id] = data;

	LUA_server->broadcast(makeEmitterTypePacket(id, data), JoinNegotiation);

	return 0;
}

static int LUA_getParticleTable(lua_State* L)
{
	scope("(LUA) getParticleTable");

	if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
	{
		error("Expected getParticleTable(name)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 1);
	lua_settop(L, 0);

	int id = findParticleType(name);
	if (id == -1)
	{
		error("There's no particle type named " + name);
		return 0;
	}

	const ParticleTypeData& data = LUA_pd->particleTypes[id];

	lua_newtable(L);

	lua_pushstring(L, data.texturePath.c_str());
	lua_setfield(L, -2, "texture");

	for (int a = 0; a < 4; a++)
	{
		std::string digit = std::to_string(a);

		pushNumbers(L, &data.colors[a][0], 4);
		lua_setfield(L, -2, ("color" + digit).c_str());

		lua_pushnumber(L, data.sizes[a]);
		lua_setfield(L, -2, ("size" + digit).c_str());

		lua_pushnumber(L, data.times[a]);
		lua_setfield(L, -2, ("time" + digit).c_str());
	}

	pushNumbers(L, &data.drag[0], 3);
	lua_setfield(L, -2, "drag");

	pushNumbers(L, &data.gravity[0], 3);
	lua_setfield(L, -2, "gravity");

	lua_pushnumber(L, data.inheritedVelFactor);
	lua_setfield(L, -2, "inheritedVelFactor");

	lua_pushnumber(L, data.lifetimeMS);
	lua_setfield(L, -2, "lifetimeMS");

	lua_pushnumber(L, data.lifetimeVarianceMS);
	lua_setfield(L, -2, "lifetimeVarianceMS");

	lua_pushnumber(L, data.spinSpeed);
	lua_setfield(L, -2, "spinSpeed");

	lua_pushboolean(L, data.useInvAlpha);
	lua_setfield(L, -2, "useInvAlpha");

	lua_pushboolean(L, data.needsSorting);
	lua_setfield(L, -2, "needsSorting");

	lua_pushboolean(L, data.lit);
	lua_setfield(L, -2, "lit");

	return 1;
}

static int LUA_getEmitterTable(lua_State* L)
{
	scope("(LUA) getEmitterTable");

	if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
	{
		error("Expected getEmitterTable(name)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 1);
	lua_settop(L, 0);

	int id = findEmitterType(name);
	if (id == -1)
	{
		error("There's no emitter type named " + name);
		return 0;
	}

	const EmitterTypeData& data = LUA_pd->emitterTypes[id];

	lua_newtable(L);

	std::string particles = "";
	for (uint16_t particleType : data.particleTypes)
	{
		if (particleType >= LUA_pd->particleTypes.size())
			continue;
		if (particles.length() > 0)
			particles += " ";
		particles += LUA_pd->particleTypes[particleType].name;
	}
	lua_pushstring(L, particles.c_str());
	lua_setfield(L, -2, "particles");

	lua_pushstring(L, data.uiName.c_str());
	lua_setfield(L, -2, "uiName");

	const std::pair<const char*, float> numberFields[] = {
		{ "ejectionPeriodMS", data.ejectionPeriodMS },
		{ "periodVarianceMS", data.periodVarianceMS },
		{ "ejectionVelocity", data.ejectionVelocity },
		{ "velocityVariance", data.velocityVariance },
		{ "ejectionOffset", data.ejectionOffset },
		{ "thetaMin", data.thetaMin },
		{ "thetaMax", data.thetaMax },
		{ "phiReferenceVel", data.phiReferenceVel },
		{ "phiVariance", data.phiVariance },
		{ "lifetimeMS", data.lifetimeMS } };

	for (const auto& field : numberFields)
	{
		lua_pushnumber(L, field.second);
		lua_setfield(L, -2, field.first);
	}

	return 1;
}

static int LUA_addEmitter(lua_State* L)
{
	scope("(LUA) addEmitter");

	const std::string usage = "addEmitter(typeName[, x, y, z])";

	int args = lua_gettop(L);
	if ((args != 1 && args != 4) || lua_type(L, 1) != LUA_TSTRING)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 1);
	float v[3] = { 0, 0, 0 };
	bool valid = args == 1 || readNumbers(L, 2, 3, v, usage);
	lua_settop(L, 0);
	if (!valid)
		return 0;

	if (!LUA_pd->emitters)
	{
		error("emitters ObjHolder is null");
		return 0;
	}

	int type = findEmitterType(name);
	if (type == -1)
	{
		error("There's no emitter type named " + name);
		return 0;
	}

	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->create((uint16_t)type, glm::vec3(v[0], v[1], v[2]));
	LUA_pd->emitters->pushLua(L, emitter);
	return 1;
}

/*
	For emitter: methods, expects the emitter plus exactly count numbers, which are read into out
	Returns nullptr after logging why if anything is wrong, and leaves the stack empty either way
*/
static std::shared_ptr<Emitter> popEmitterAndNumbers(lua_State* L, int count, float* out, const std::string& usage)
{
	if (lua_gettop(L) != count + 1)
	{
		error("Expected " + std::to_string(count + 1) + " arguments " + usage);
		lua_settop(L, 0);
		return nullptr;
	}

	if (!LUA_pd->emitters)
	{
		error("emitters ObjHolder is null");
		lua_settop(L, 0);
		return nullptr;
	}

	if (!readNumbers(L, 2, count, out, usage))
	{
		lua_settop(L, 0);
		return nullptr;
	}

	lua_settop(L, 1);
	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
	lua_settop(L, 0);

	if (!emitter)
		error("Invalid emitter passed, was it removed already?");

	return emitter;
}

static int LUA_emitterDestroy(lua_State* L)
{
	scope("(LUA) emitter:destroy");

	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 0, nullptr, "emitter:destroy()");
	if (emitter)
		LUA_pd->emitters->destroy(emitter);

	return 0;
}

static int LUA_emitterGetPosition(lua_State* L)
{
	scope("(LUA) emitter:getPosition");

	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 0, nullptr, "emitter:getPosition()");
	if (!emitter)
		return 0;

	glm::vec3 position = emitter->getPosition();
	lua_pushnumber(L, position.x);
	lua_pushnumber(L, position.y);
	lua_pushnumber(L, position.z);
	return 3;
}

static int LUA_emitterSetPosition(lua_State* L)
{
	scope("(LUA) emitter:setPosition");

	float v[3];
	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 3, v, "emitter:setPosition(x, y, z)");
	if (emitter)
		emitter->setPosition(glm::vec3(v[0], v[1], v[2]));

	return 0;
}

static int LUA_emitterGetTypeName(lua_State* L)
{
	scope("(LUA) emitter:getTypeName");

	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 0, nullptr, "emitter:getTypeName()");
	if (!emitter || emitter->getTypeID() >= LUA_pd->emitterTypes.size())
		return 0;

	lua_pushstring(L, LUA_pd->emitterTypes[emitter->getTypeID()].name.c_str());
	return 1;
}

static int LUA_emitterSetType(lua_State* L)
{
	scope("(LUA) emitter:setType");

	if (lua_gettop(L) != 2 || lua_type(L, 2) != LUA_TSTRING)
	{
		error("Expected emitter:setType(typeName)");
		lua_settop(L, 0);
		return 0;
	}

	std::string name = lua_tostring(L, 2);
	lua_settop(L, 1);

	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 0, nullptr, "emitter:setType(typeName)");
	if (!emitter)
		return 0;

	int type = findEmitterType(name);
	if (type == -1)
	{
		error("There's no emitter type named " + name);
		return 0;
	}

	emitter->setType((uint16_t)type);
	return 0;
}

static int LUA_emitterSetColor(lua_State* L)
{
	scope("(LUA) emitter:setColor");

	const std::string usage = "emitter:setColor(r, g, b[, a])";

	int args = lua_gettop(L);
	if (args != 4 && args != 5)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	float v[4] = { 1, 1, 1, 1 };
	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, args - 1, v, usage);
	if (!emitter)
		return 0;

	glm::vec4 color = glm::clamp(glm::vec4(v[0], v[1], v[2], v[3]), 0.0f, 1.0f);
	emitter->setColor(glm::u8vec4(color * 255.0f + 0.5f));
	return 0;
}

static int LUA_emitterGetColor(lua_State* L)
{
	scope("(LUA) emitter:getColor");

	std::shared_ptr<Emitter> emitter = popEmitterAndNumbers(L, 0, nullptr, "emitter:getColor()");
	if (!emitter)
		return 0;

	glm::vec4 color = emitter->getTint();
	for (int channel = 0; channel < 4; channel++)
		lua_pushnumber(L, color[channel]);
	return 4;
}

static int LUA_emitterAimWith(lua_State* L)
{
	scope("(LUA) emitter:aimWith");

	const std::string usage = "emitter:aimWith(dynamic, range) or emitter:aimWith(nil)";

	int args = lua_gettop(L);
	if (!LUA_pd->emitters || !LUA_pd->dynamics)
	{
		error("emitters or dynamics ObjHolder is null");
		lua_settop(L, 0);
		return 0;
	}

	if (args == 2 && lua_isnil(L, 2))
	{
		lua_settop(L, 1);
		std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
		lua_settop(L, 0);

		if (emitter)
			emitter->aimWith(nullptr, 0);
		else
			error("Invalid emitter passed to " + usage);
		return 0;
	}

	if (args != 3 || !lua_isnumber(L, 3) || !std::isfinite(lua_tonumber(L, 3)))
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	float range = std::clamp((float)lua_tonumber(L, 3), 0.0f, 1000.0f);

	lua_settop(L, 2);
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
	lua_settop(L, 0);

	if (!dynamic || !emitter)
	{
		error("Invalid emitter or dynamic passed to " + usage);
		return 0;
	}

	emitter->aimWith(dynamic, range);
	return 0;
}

static int LUA_emitterAttachToDynamic(lua_State* L)
{
	scope("(LUA) emitter:attachToDynamic");

	const std::string usage = "emitter:attachToDynamic(dynamic[, meshName][, offsetX, offsetY, offsetZ])";

	int args = lua_gettop(L);
	bool hasMesh = args == 3 || args == 6;
	bool hasOffset = args == 5 || args == 6;

	bool badArguments = args != 2 && args != 3 && args != 5 && args != 6;
	if (hasMesh && lua_type(L, 3) != LUA_TSTRING)
		badArguments = true;
	for (int a = args - 2; hasOffset && a <= args; a++)
	{
		if (lua_type(L, a) != LUA_TNUMBER)
			badArguments = true;
	}

	if (badArguments)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	if (!LUA_pd->emitters || !LUA_pd->dynamics)
	{
		error("emitters or dynamics ObjHolder is null");
		lua_settop(L, 0);
		return 0;
	}

	std::string meshName = hasMesh ? lua_tostring(L, 3) : "";

	glm::vec3 offset(0);
	if (hasOffset)
		offset = glm::vec3(lua_tonumber(L, args - 2), lua_tonumber(L, args - 1), lua_tonumber(L, args));

	lua_settop(L, 2);
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
	lua_settop(L, 0);

	if (!dynamic || !emitter)
	{
		error("Invalid emitter or dynamic passed to " + usage);
		return 0;
	}

	int meshIndex = -1;
	if (meshName.length() > 0)
	{
		meshIndex = dynamic->getType()->getModel()->getMeshIdx(meshName);

		//255 means no mesh in packets
		if (meshIndex < 0 || meshIndex > 254)
		{
			error("The dynamic's model has no mesh named " + meshName);
			return 0;
		}
	}

	emitter->attachToDynamic(dynamic, meshIndex, offset);
	return 0;
}

static int LUA_emitterAttachToBrick(lua_State* L)
{
	scope("(LUA) emitter:attachToBrick");

	if (lua_gettop(L) != 2)
	{
		error("Expected emitter:attachToBrick(brick or nil)");
		lua_settop(L, 0);
		return 0;
	}

	if (!LUA_pd->emitters || !LUA_pd->bricks)
	{
		error("emitters ObjHolder or BrickHolder is null");
		lua_settop(L, 0);
		return 0;
	}

	//nil leaves the emitter where it is, no longer on or following anything
	if (lua_isnil(L, 2))
	{
		lua_settop(L, 1);
		std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
		lua_settop(L, 0);

		if (emitter)
			emitter->setPosition(emitter->getPosition());
		return 0;
	}

	Brick* brick = LUA_pd->bricks->popLua(L);
	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
	lua_settop(L, 0);

	if (!brick || !emitter)
	{
		error("Invalid emitter or brick passed to emitter:attachToBrick(brick)");
		return 0;
	}

	emitter->attachToBrick(brick->netId, brick->getWorldCenter());
	return 0;
}

/*
	emitter:attachToVehicle(vehicle[, offsetX, offsetY, offsetZ])

	Goes along with the vehicle, at a spot in its body's own space so it turns with it, like the wingtips
	of a plane. It's removed along with the vehicle rather than after its type's lifetime, the way an
	emitter carried over from one of a sliced vehicle's bricks is
*/
static int LUA_emitterAttachToVehicle(lua_State* L)
{
	scope("(LUA) emitter:attachToVehicle");

	const std::string usage = "emitter:attachToVehicle(vehicle[, offsetX, offsetY, offsetZ])";

	int args = lua_gettop(L);
	bool hasOffset = args == 5;
	bool badArguments = args != 2 && args != 5;
	for (int a = 3; hasOffset && a <= 5; a++)
	{
		if (lua_type(L, a) != LUA_TNUMBER)
			badArguments = true;
	}

	if (badArguments)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	if (!LUA_pd->emitters || !LUA_pd->vehicles)
	{
		error("emitters or vehicles ObjHolder is null");
		lua_settop(L, 0);
		return 0;
	}

	glm::vec3 offset(0);
	if (hasOffset)
		offset = glm::vec3(lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5));

	lua_settop(L, 2);
	std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->popLua(L);
	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->popLua(L);
	lua_settop(L, 0);

	if (!vehicle || !emitter)
	{
		error("Invalid emitter or vehicle passed to " + usage);
		return 0;
	}

	emitter->attachToVehicle(vehicle, offset);
	return 0;
}

static int LUA_getEmitterId(lua_State* L)
{
	scope("(LUA) getEmitterId");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument getEmitterId(netId)");
		lua_settop(L, 0);
		return 0;
	}

	netIDType id = (netIDType)lua_tointeger(L, 1);
	lua_settop(L, 0);

	if (!LUA_pd->emitters)
	{
		error("emitters ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Emitter> emitter = LUA_pd->emitters->find(id);
	if (!emitter)
	{
		error("Invalid emitter id passed, does it exist?");
		return 0;
	}

	LUA_pd->emitters->pushLua(L, emitter);
	return 1;
}

static int LUA_getEmitterIdx(lua_State* L)
{
	scope("(LUA) getEmitterIdx");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument getEmitterIdx(index)");
		lua_settop(L, 0);
		return 0;
	}

	lua_Integer idx = lua_tointeger(L, 1);
	lua_settop(L, 0);

	if (!LUA_pd->emitters)
	{
		error("emitters ObjHolder is null");
		return 0;
	}

	if (idx < 0 || idx >= (lua_Integer)LUA_pd->emitters->size())
	{
		error("Invalid emitter index passed, size: " + std::to_string(LUA_pd->emitters->size()) + ", index: " + std::to_string(idx));
		return 0;
	}

	LUA_pd->emitters->pushLua(L, LUA_pd->emitters->get(idx));
	return 1;
}

static int LUA_getNumEmitters(lua_State* L)
{
	scope("(LUA) getNumEmitters");

	if (lua_gettop(L) != 0)
	{
		error("Expected 0 arguments getNumEmitters()");
		lua_settop(L, 0);
		return 0;
	}

	if (!LUA_pd->emitters)
	{
		error("emitters ObjHolder is null");
		return 0;
	}

	lua_pushinteger(L, LUA_pd->emitters->size());
	return 1;
}

luaL_Reg* getEmitterFunctions(lua_State* L)
{
	lua_register(L, "addParticleType", LUA_addParticleType);
	lua_register(L, "addEmitterType", LUA_addEmitterType);
	lua_register(L, "getParticleTable", LUA_getParticleTable);
	lua_register(L, "getEmitterTable", LUA_getEmitterTable);
	lua_register(L, "addEmitter", LUA_addEmitter);
	lua_register(L, "getEmitterId", LUA_getEmitterId);
	lua_register(L, "getEmitterIdx", LUA_getEmitterIdx);
	lua_register(L, "getNumEmitters", LUA_getNumEmitters);

	luaL_Reg* regs = new luaL_Reg[13];

	int iter = 0;
	regs[iter++] = { "destroy",			LUA_emitterDestroy };
	//The old game's name for destroy
	regs[iter++] = { "remove",			LUA_emitterDestroy };
	regs[iter++] = { "getPosition",		LUA_emitterGetPosition };
	regs[iter++] = { "setPosition",		LUA_emitterSetPosition };
	regs[iter++] = { "getTypeName",		LUA_emitterGetTypeName };
	regs[iter++] = { "setType",			LUA_emitterSetType };
	regs[iter++] = { "attachToDynamic",	LUA_emitterAttachToDynamic };
	regs[iter++] = { "attachToBrick",	LUA_emitterAttachToBrick };
	regs[iter++] = { "attachToVehicle",	LUA_emitterAttachToVehicle };
	regs[iter++] = { "setColor",		LUA_emitterSetColor };
	regs[iter++] = { "getColor",		LUA_emitterGetColor };
	regs[iter++] = { "aimWith",			LUA_emitterAimWith };
	regs[iter++] = { NULL, NULL };

	return regs;
}
