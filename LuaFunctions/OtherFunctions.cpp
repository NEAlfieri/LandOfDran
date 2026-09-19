#include "OtherFunctions.h"
#include "../GameLoop/ServerProgramData.h"
#include "../Networking/ServerFiles.h"

extern ServerProgramData* LUA_pd;

ExecutableArguments* LUA_args = nullptr;

void stackDump(lua_State* L) 
{
	int i;
	int top = lua_gettop(L);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {

		case LUA_TSTRING:  /* strings */
			printf("`%s'", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			printf(lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:  /* numbers */
			printf("%g", lua_tonumber(L, i));
			break;

		default:  /* other values */
			printf("%s", lua_typename(L, t));
			break;

		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */
}

static int LUA_shutdown(lua_State* L)
{
	if (LUA_args)
	{
		info("shutdown() called");
		LUA_args->mainLoopRun = false;
	}
	else
		error("LUA_args not set");

	return 0;
}

//Common part of LUA_info/error/debug
std::string makeStringFromArgs(lua_State* L)
{
	int args = lua_gettop(L);

	if(args == 0)
	{
		return "[Empty line]";
	}

	std::string ret = "";

	for (unsigned int a = 1; a < args+1; a++)
	{
		if (lua_isstring(L, a))
		{
			const char *str = lua_tostring(L, a);
			if (str)
				ret += std::string(str);
			else
				ret += "[invalid string]";
			continue;
		}

		if (lua_isboolean(L, a))
		{
			ret += lua_toboolean(L, a) ? "true" : "false";
			continue;
		}

		if (lua_isnil(L, a))
		{
			ret += "[nil]";
			continue;
		}

		if (lua_isnumber(L, a))
		{
			ret += std::to_string(lua_tonumber(L, a));
			continue;
		}

		if (lua_istable(L, a))
		{
			//Bricks have no ptr field, so check for them before the SimObject handling below
			lua_getfield(L, a, "type");
			bool isBrick = lua_isinteger(L, -1) && lua_tointeger(L, -1) == BrickTypeId;
			lua_pop(L, 1);

			if (isBrick)
			{
				lua_getfield(L, a, "id");
				ret += "[Brick " + std::to_string(lua_tointeger(L, -1)) + "]";
				lua_pop(L, 1);
				continue;
			}

			lua_getfield(L, a, "ptr");
			if (lua_isuserdata(L, -1))
			{
				lua_pop(L, 1); //ptr

				lua_getfield(L, a, "type");
				if(lua_isinteger(L,-1))
				{
					SimObjectType type = (SimObjectType)lua_tointeger(L, -1);
					lua_pop(L, 1); //type
					switch (type)
					{
						case DynamicTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Dynamic " + std::to_string(id)+"]";
							break;
						}

						case ClientTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Client " + std::to_string(id) + "]";
							break;
						}

						case StaticTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Static " + std::to_string(id) + "]";
							break;
						}

						case LightTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Light " + std::to_string(id) + "]";
							break;
						}

						case EmitterTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Emitter " + std::to_string(id) + "]";
							break;
						}

						case RopeTypeId:
						{
							lua_getfield(L, a, "id");
							int id = lua_tointeger(L, -1);
							lua_pop(L, 1); //id

							ret += "[Rope " + std::to_string(id) + "]";
							break;
						}

						case InvalidSimTypeId:
						default:
							ret += "[Unknown Simobject]";
					}
					continue;
				}
				else
				{
					ret += "[Unknown Userdata]";
					lua_pop(L, 1); //type
					continue;
				}
			}
			else
				lua_pop(L, 1); //ptr


			ret += "[table]";
			continue;
		}

		if (lua_isfunction(L, a))
		{
			ret += "[function]";
			continue;
		}

		ret += "[unknown]";
	}

	lua_pop(L, args);

	return ret;
}

static int LUA_info(lua_State* L)
{
	scope("LUA_info");

	std::string line = makeStringFromArgs(L);
	info(line);

	return 0;
}

static int LUA_error(lua_State* L)
{
	scope("LUA_error");

	std::string line = makeStringFromArgs(L);
	error(line);

	return 0;
}

static int LUA_debug(lua_State* L)
{
	scope("LUA_debug");

	std::string line = makeStringFromArgs(L);
	debug(line);

	return 0;
}

//For setters that take exactly one number, logs an error and returns false for anything else
static bool getOnlyNumberArgument(lua_State* L, double& result)
{
	int args = lua_gettop(L);
	bool valid = args == 1 && lua_isnumber(L, 1);

	if (valid)
		result = lua_tonumber(L, 1);
	else
		error("Expected 1 number argument");

	lua_pop(L, args);
	return valid;
}

static int LUA_setTimeOfDay(lua_State* L)
{
	scope("LUA_setTimeOfDay");

	double fraction;
	if (!getOnlyNumberArgument(L, fraction))
		return 0;

	fraction -= floor(fraction);
	LUA_pd->worldTimeSeconds = fraction * DAY_LENGTH_SECONDS;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getTimeOfDay(lua_State* L)
{
	double fraction = fmod(LUA_pd->worldTimeSeconds, DAY_LENGTH_SECONDS) / DAY_LENGTH_SECONDS;
	if (fraction < 0)
		fraction += 1.0;

	lua_pushnumber(L, fraction);
	return 1;
}

static int LUA_setTimeScale(lua_State* L)
{
	scope("LUA_setTimeScale");

	double scale;
	if (!getOnlyNumberArgument(L, scale))
		return 0;

	LUA_pd->timeScale = (float)scale;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getTimeScale(lua_State* L)
{
	lua_pushnumber(L, LUA_pd->timeScale);
	return 1;
}

static int LUA_setWaterLevel(lua_State* L)
{
	scope("LUA_setWaterLevel");

	int args = lua_gettop(L);

	if (args == 0 || (args == 1 && lua_isnil(L, 1)))
		LUA_pd->waterEnabled = false;
	else if (args == 1 && lua_isnumber(L, 1))
	{
		LUA_pd->waterEnabled = true;
		LUA_pd->waterLevel = (float)lua_tonumber(L, 1);
	}
	else
	{
		error("Expected no arguments or 1 number argument");
		lua_pop(L, args);
		return 0;
	}

	lua_pop(L, args);
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getWaterLevel(lua_State* L)
{
	if (LUA_pd->waterEnabled)
		lua_pushnumber(L, LUA_pd->waterLevel);
	else
		lua_pushnil(L);

	return 1;
}

static int LUA_setRain(lua_State* L)
{
	scope("LUA_setRain");

	int args = lua_gettop(L);
	if (args != 1 || !lua_isnumber(L, 1))
	{
		error("Expected 1 number argument");
		lua_pop(L, args);
		return 0;
	}

	LUA_pd->rainIntensity = std::clamp((float)lua_tonumber(L, 1), 0.0f, 1.0f);
	lua_pop(L, args);
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getRain(lua_State* L)
{
	lua_pushnumber(L, LUA_pd->rainIntensity);
	return 1;
}

//A phase name then r, g, b, and when allowBrightness is set optionally a 5th number, for the day cycle color setters
//Logs an error and returns -1 for anything else, otherwise returns the DayPhase
static int getPhaseColorArguments(lua_State* L, bool allowBrightness, glm::vec3& color, float& brightness, bool& hasBrightness)
{
	int args = lua_gettop(L);
	bool valid = (args == 4 || (allowBrightness && args == 5)) && lua_type(L, 1) == LUA_TSTRING;
	for (int a = 2; valid && a <= args; a++)
		valid = lua_isnumber(L, a);

	int phase = -1;
	if (!valid)
		error(allowBrightness ? "Expected a phase name, r, g, b, and optionally brightness" : "Expected a phase name, r, g, b");
	else
	{
		phase = dayPhaseFromName(lua_tostring(L, 1));
		if (phase == -1)
			error("Unknown phase " + std::string(lua_tostring(L, 1)) + ", expected night, dawn, day, or dusk");

		color = glm::max(glm::vec3(lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4)), glm::vec3(0.0f));
		hasBrightness = args == 5;
		if (hasBrightness)
			brightness = std::max(0.0f, (float)lua_tonumber(L, 5));
	}

	lua_pop(L, args);
	return phase;
}

//Just a phase name, for the day cycle color getters, logs an error and returns -1 for anything else
static int getPhaseArgument(lua_State* L)
{
	int args = lua_gettop(L);

	int phase = -1;
	if (args != 1 || lua_type(L, 1) != LUA_TSTRING)
		error("Expected a phase name");
	else
	{
		phase = dayPhaseFromName(lua_tostring(L, 1));
		if (phase == -1)
			error("Unknown phase " + std::string(lua_tostring(L, 1)) + ", expected night, dawn, day, or dusk");
	}

	lua_pop(L, args);
	return phase;
}

static void pushColor(lua_State* L, const glm::vec3& color)
{
	lua_pushnumber(L, color.r);
	lua_pushnumber(L, color.g);
	lua_pushnumber(L, color.b);
}

static int LUA_setSkyColor(lua_State* L)
{
	scope("LUA_setSkyColor");

	glm::vec3 color;
	float unused;
	bool unusedFlag;
	int phase = getPhaseColorArguments(L, false, color, unused, unusedFlag);
	if (phase == -1)
		return 0;

	LUA_pd->dayCycle.phases[phase].skyColor = color;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getSkyColor(lua_State* L)
{
	scope("LUA_getSkyColor");

	int phase = getPhaseArgument(L);
	if (phase == -1)
		return 0;

	pushColor(L, LUA_pd->dayCycle.phases[phase].skyColor);
	return 3;
}

static int LUA_setFogColor(lua_State* L)
{
	scope("LUA_setFogColor");

	glm::vec3 color;
	float unused;
	bool unusedFlag;
	int phase = getPhaseColorArguments(L, false, color, unused, unusedFlag);
	if (phase == -1)
		return 0;

	LUA_pd->dayCycle.phases[phase].fogColor = color;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getFogColor(lua_State* L)
{
	scope("LUA_getFogColor");

	int phase = getPhaseArgument(L);
	if (phase == -1)
		return 0;

	pushColor(L, LUA_pd->dayCycle.phases[phase].fogColor);
	return 3;
}

static int LUA_setSunColor(lua_State* L)
{
	scope("LUA_setSunColor");

	glm::vec3 color;
	float brightness = 0;
	bool hasBrightness = false;
	int phase = getPhaseColorArguments(L, true, color, brightness, hasBrightness);
	if (phase == -1)
		return 0;

	//Light colors go past 1, so brightness scales the color. Leaving it out keeps the phase's current brightness
	glm::vec3& light = LUA_pd->dayCycle.phases[phase].lightColor;
	if (!hasBrightness)
		brightness = std::max(light.r, std::max(light.g, light.b));

	light = color * brightness;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getSunColor(lua_State* L)
{
	scope("LUA_getSunColor");

	int phase = getPhaseArgument(L);
	if (phase == -1)
		return 0;

	//Split back into a color whose brightest channel is 1, and how bright that channel is
	const glm::vec3& light = LUA_pd->dayCycle.phases[phase].lightColor;
	float brightness = std::max(light.r, std::max(light.g, light.b));
	pushColor(L, brightness > 0 ? light / brightness : glm::vec3(0.0f));
	lua_pushnumber(L, brightness);
	return 4;
}

static int LUA_setAmbientColor(lua_State* L)
{
	scope("LUA_setAmbientColor");

	glm::vec3 color;
	float unused;
	bool unusedFlag;
	int phase = getPhaseColorArguments(L, false, color, unused, unusedFlag);
	if (phase == -1)
		return 0;

	LUA_pd->dayCycle.phases[phase].ambientColor = color;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getAmbientColor(lua_State* L)
{
	scope("LUA_getAmbientColor");

	int phase = getPhaseArgument(L);
	if (phase == -1)
		return 0;

	pushColor(L, LUA_pd->dayCycle.phases[phase].ambientColor);
	return 3;
}

static int LUA_setFogDistance(lua_State* L)
{
	scope("LUA_setFogDistance");

	int args = lua_gettop(L);
	if (args != 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2))
	{
		error("Expected 2 number arguments");
		lua_pop(L, args);
		return 0;
	}

	float start = (float)lua_tonumber(L, 1);
	float end = (float)lua_tonumber(L, 2);
	lua_pop(L, args);

	if (start < 0 || end <= start || end > DayCycle::maxFogEnd)
	{
		error("Fog distances need 0 <= start < end <= " + std::to_string((int)DayCycle::maxFogEnd));
		return 0;
	}

	LUA_pd->dayCycle.fogStart = start;
	LUA_pd->dayCycle.fogEnd = end;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getFogDistance(lua_State* L)
{
	lua_pushnumber(L, LUA_pd->dayCycle.fogStart);
	lua_pushnumber(L, LUA_pd->dayCycle.fogEnd);
	return 2;
}

static int LUA_setFogHeight(lua_State* L)
{
	scope("LUA_setFogHeight");

	int args = lua_gettop(L);
	if (args != 1 || !lua_isnumber(L, 1))
	{
		error("Expected 1 number argument");
		lua_pop(L, args);
		return 0;
	}

	float height = (float)lua_tonumber(L, 1);
	lua_pop(L, args);

	if (height < 0 || height > DayCycle::maxFogHeight)
	{
		error("Fog height needs 0 <= height <= " + std::to_string((int)DayCycle::maxFogHeight));
		return 0;
	}

	LUA_pd->dayCycle.fogHeight = height;
	LUA_pd->worldStateChanged = true;

	return 0;
}

static int LUA_getFogHeight(lua_State* L)
{
	lua_pushnumber(L, LUA_pd->dayCycle.fogHeight);
	return 1;
}

static int LUA_resetDayCycle(lua_State* L)
{
	lua_pop(L, lua_gettop(L));

	LUA_pd->dayCycle = DayCycle();
	LUA_pd->worldStateChanged = true;

	return 0;
}

/*
	addServerFile("Add-ons/Weapon_Gun/gun.dts")

	Offers a file to clients that join: one that doesn't have a copy of its own is asked whether it wants
	the server to send it, see Networking/ServerFiles.h. Returns true if it's on offer

	Only sounds, models, textures, and the .txt descriptors that go with a model can be offered, so an
	add-on's scripts stay on the server where they are
*/
static int LUA_addServerFile(lua_State* L)
{
	scope("LUA_addServerFile");

	int args = lua_gettop(L);

	if (args != 1 || !lua_isstring(L, 1))
	{
		error("Expected 1 string argument, a file path");
		lua_pop(L, args);
		lua_pushboolean(L, false);
		return 1;
	}

	std::string path = std::string(lua_tostring(L, 1));
	lua_pop(L, args);

	std::string problem = "";
	if (!addServerFile(LUA_pd, path, problem))
	{
		error("Can't offer " + path + " to clients: " + problem);
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

/*
	addServerFolder("Add-ons/Weapon_Gun")

	The same for every file in a folder that clients can be sent, which is what an add-on that wants to
	send all of its own content uses instead of a line per file. Returns how many are on offer from it
	Files of any other kind, its scripts above all, are passed over rather than complained about
*/
static int LUA_addServerFolder(lua_State* L)
{
	scope("LUA_addServerFolder");

	int args = lua_gettop(L);

	if (args != 1 || !lua_isstring(L, 1))
	{
		error("Expected 1 string argument, a folder");
		lua_pop(L, args);
		lua_pushinteger(L, 0);
		return 1;
	}

	std::string folder = std::string(lua_tostring(L, 1));
	lua_pop(L, args);

	if (!isPathInsideGameFolder(folder))
	{
		error("Can't offer " + folder + " to clients: folder has to be inside the game's folder");
		lua_pushinteger(L, 0);
		return 1;
	}

	std::error_code trouble;
	if (!std::filesystem::is_directory(folder, trouble))
	{
		error("Can't offer " + folder + " to clients: no such folder");
		lua_pushinteger(L, 0);
		return 1;
	}

	int offered = 0;
	for (const std::filesystem::directory_entry& file : std::filesystem::recursive_directory_iterator(folder, trouble))
	{
		if (!file.is_regular_file())
			continue;

		std::string path = file.path().generic_string();
		if (contentFileKind(path) == ContentFileUnknown)
			continue;

		std::string problem = "";
		if (addServerFile(LUA_pd, path, problem))
			offered++;
		else
			error("Can't offer " + path + " to clients: " + problem);
	}

	lua_pushinteger(L, offered);
	return 1;
}

void registerOtherFunctions(lua_State* L)
{
	lua_register(L, "info", LUA_info);
	lua_register(L, "error", LUA_error);
	lua_register(L, "debug", LUA_debug);
	lua_register(L, "shutdown", LUA_shutdown);
	lua_register(L, "setTimeOfDay", LUA_setTimeOfDay);
	lua_register(L, "getTimeOfDay", LUA_getTimeOfDay);
	lua_register(L, "setTimeScale", LUA_setTimeScale);
	lua_register(L, "getTimeScale", LUA_getTimeScale);
	lua_register(L, "setWaterLevel", LUA_setWaterLevel);
	lua_register(L, "getWaterLevel", LUA_getWaterLevel);
	lua_register(L, "setRain", LUA_setRain);
	lua_register(L, "getRain", LUA_getRain);
	lua_register(L, "setSkyColor", LUA_setSkyColor);
	lua_register(L, "getSkyColor", LUA_getSkyColor);
	lua_register(L, "setFogColor", LUA_setFogColor);
	lua_register(L, "getFogColor", LUA_getFogColor);
	lua_register(L, "setSunColor", LUA_setSunColor);
	lua_register(L, "getSunColor", LUA_getSunColor);
	lua_register(L, "setAmbientColor", LUA_setAmbientColor);
	lua_register(L, "getAmbientColor", LUA_getAmbientColor);
	lua_register(L, "setFogDistance", LUA_setFogDistance);
	lua_register(L, "getFogDistance", LUA_getFogDistance);
	lua_register(L, "setFogHeight", LUA_setFogHeight);
	lua_register(L, "getFogHeight", LUA_getFogHeight);
	lua_register(L, "resetDayCycle", LUA_resetDayCycle);
	lua_register(L, "addServerFile", LUA_addServerFile);
	lua_register(L, "addServerFolder", LUA_addServerFolder);
}
