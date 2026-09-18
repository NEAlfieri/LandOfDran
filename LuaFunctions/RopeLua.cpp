#include "RopeLua.h"

#include <cmath>

//The old game's attachByRope made ropes of 15 links unless told otherwise
static constexpr int defaultLinks = 15;

//Reads a finite number at index into out
static bool readNumber(lua_State* L, int index, float& out)
{
	if (!lua_isnumber(L, index))
		return false;

	out = (float)lua_tonumber(L, index);
	return std::isfinite(out);
}

//Reads x, y, z starting at index
static bool readVector(lua_State* L, int index, glm::vec3& out)
{
	return readNumber(L, index, out.x) && readNumber(L, index + 1, out.y) && readNumber(L, index + 2, out.z);
}

//Reads a table of three numbers at index
static bool readVectorTable(lua_State* L, int index, glm::vec3& out)
{
	if (!lua_istable(L, index))
		return false;

	for (int a = 0; a < 3; a++)
	{
		lua_rawgeti(L, index, a + 1);
		bool good = readNumber(L, -1, out[a]);
		lua_pop(L, 1);
		if (!good)
			return false;
	}

	return true;
}

/*
	Ties one end of a rope to whatever is at index: a dynamic or vehicle, offset along its own axes from its middle,
	a brick, at position in the world or its middle if there's none, or a table of x, y, z, a spot in the world
	Leaves the stack as it was, and returns false after logging why if that isn't something a rope can be tied to
*/
static bool tieEnd(lua_State* L, int index, const std::shared_ptr<Rope>& rope, int end, bool hasVector, const glm::vec3& vector)
{
	if (!lua_istable(L, index))
	{
		error("A rope's end has to be a dynamic, vehicle, brick, or a table of x, y, z");
		return false;
	}

	lua_getfield(L, index, "type");
	bool isObject = lua_isinteger(L, -1);
	SimObjectType type = isObject ? (SimObjectType)lua_tointeger(L, -1) : InvalidSimTypeId;
	lua_pop(L, 1);

	if (!isObject)
	{
		glm::vec3 position;
		if (!readVectorTable(L, index, position))
		{
			error("A rope's end has to be a dynamic, vehicle, brick, or a table of x, y, z");
			return false;
		}

		rope->tieToPoint(end, position);
		return true;
	}

	//popLua takes it off the top
	lua_pushvalue(L, index);

	switch (type)
	{
		case DynamicTypeId:
		{
			std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
			if (!dynamic)
				return false;

			rope->tieToDynamic(end, dynamic, hasVector ? vector : glm::vec3(0));
			return true;
		}
		case VehicleTypeId:
		{
			std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->popLua(L);
			if (!vehicle)
				return false;

			rope->tieToVehicle(end, vehicle, hasVector ? vector : glm::vec3(0));
			return true;
		}
		case BrickTypeId:
		{
			Brick* brick = LUA_pd->bricks->popLua(L);
			if (!brick)
				return false;

			rope->tieToBrick(end, brick->netId, hasVector ? vector : brick->getWorldCenter());
			return true;
		}
		default:
			lua_pop(L, 1);
			error("A rope can only be tied to a dynamic, vehicle, brick, or a spot in the world");
			return false;
	}
}

//The rope at the bottom of the stack, which is left alone, nullptr after logging if it isn't one
static std::shared_ptr<Rope> getRope(lua_State* L)
{
	if (!LUA_pd->ropes || lua_gettop(L) < 1)
	{
		error("Expected a rope");
		return nullptr;
	}

	lua_pushvalue(L, 1);
	std::shared_ptr<Rope> rope = LUA_pd->ropes->popLua(L);
	if (!rope)
		error("Invalid rope passed, was it removed already?");
	return rope;
}

//1 or 2 at index into 0 or 1
static bool readEnd(lua_State* L, int index, int& end)
{
	if (!lua_isinteger(L, index) || lua_tointeger(L, index) < 1 || lua_tointeger(L, index) > 2)
	{
		error("Which end has to be 1 or 2");
		return false;
	}

	end = (int)lua_tointeger(L, index) - 1;
	return true;
}

/*
	Makes a rope between what's at indices a and b. A length of 0 or less means as long as it is from one to the
	other right now. Pushes nothing, returns nullptr after logging if either end was no good
*/
static std::shared_ptr<Rope> makeRope(lua_State* L, int a, int b, float length, int links)
{
	std::shared_ptr<Rope> rope = LUA_pd->ropes->create();

	if (!tieEnd(L, a, rope, 0, false, glm::vec3(0)) || !tieEnd(L, b, rope, 1, false, glm::vec3(0)))
	{
		LUA_pd->ropes->destroy(rope);
		return nullptr;
	}

	rope->setLinks(links);
	rope->setLength(length > 0 ? length : rope->getPathLength());
	return rope;
}

static int LUA_createRope(lua_State* L)
{
	scope("(LUA) createRope");

	int args = lua_gettop(L);
	if (args < 2 || args > 4 || !LUA_pd->ropes)
	{
		error("Expected createRope(endA, endB[, length[, links]])");
		lua_settop(L, 0);
		return 0;
	}

	float length = 0;
	if (args >= 3 && !lua_isnil(L, 3) && !readNumber(L, 3, length))
	{
		error("length has to be a number");
		lua_settop(L, 0);
		return 0;
	}

	int links = defaultLinks;
	if (args >= 4)
	{
		if (!lua_isinteger(L, 4) || lua_tointeger(L, 4) < Rope::minLinks || lua_tointeger(L, 4) > Rope::maxLinks)
		{
			error("Number of links should be between " + std::to_string(Rope::minLinks) + " and " + std::to_string(Rope::maxLinks));
			lua_settop(L, 0);
			return 0;
		}
		links = (int)lua_tointeger(L, 4);
	}

	std::shared_ptr<Rope> rope = makeRope(L, 1, 2, length, links);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	LUA_pd->ropes->pushLua(L, rope);
	return 1;
}

//dynamic:attachByRope(other[, links]) and dynamic:attachByRopeBrick(brick[, links]), the old game's, which now give back the rope
static int attachByRope(lua_State* L, const std::string& usage)
{
	int args = lua_gettop(L);
	if (args < 2 || args > 3 || !LUA_pd->ropes)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	int links = defaultLinks;
	if (args == 3)
	{
		if (!lua_isinteger(L, 3) || lua_tointeger(L, 3) < Rope::minLinks || lua_tointeger(L, 3) > Rope::maxLinks)
		{
			error("Number of links should be between " + std::to_string(Rope::minLinks) + " and " + std::to_string(Rope::maxLinks));
			lua_settop(L, 0);
			return 0;
		}
		links = (int)lua_tointeger(L, 3);
	}

	//As long as they are apart right now, like the old ones
	std::shared_ptr<Rope> rope = makeRope(L, 1, 2, 0, links);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	LUA_pd->ropes->pushLua(L, rope);
	return 1;
}

static int LUA_dynamicAttachByRope(lua_State* L)
{
	scope("(LUA) dynamic:attachByRope");
	return attachByRope(L, "dynamic:attachByRope(otherDynamic[, links])");
}

static int LUA_dynamicAttachByRopeBrick(lua_State* L)
{
	scope("(LUA) dynamic:attachByRopeBrick");
	return attachByRope(L, "dynamic:attachByRopeBrick(brick[, links])");
}

//Removes every rope tied to the dynamic and returns how many there were
static int LUA_dynamicClearRopes(lua_State* L)
{
	scope("(LUA) dynamic:clearRopes");

	if (lua_gettop(L) != 1 || !LUA_pd->ropes || !LUA_pd->dynamics)
	{
		error("Expected dynamic:clearRopes()");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	lua_settop(L, 0);
	if (!dynamic)
		return 0;

	int removed = 0;
	for (int a = (int)LUA_pd->ropes->size() - 1; a >= 0; a--)
	{
		std::shared_ptr<Rope> rope = LUA_pd->ropes->get(a);
		bool tied = false;
		for (int end = 0; end < 2; end++)
			tied = tied || (rope->getAnchor(end).kind == RopeAnchorDynamic && rope->getAnchor(end).id == dynamic->getID());

		if (tied)
		{
			LUA_pd->ropes->destroy(rope);
			removed++;
		}
	}

	lua_pushinteger(L, removed);
	return 1;
}

static int LUA_ropeDestroy(lua_State* L)
{
	scope("(LUA) rope:destroy");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (rope)
		LUA_pd->ropes->destroy(rope);

	return 0;
}

static int LUA_ropeGetLength(lua_State* L)
{
	scope("(LUA) rope:getLength");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	lua_pushnumber(L, rope->getLength());
	return 1;
}

static int LUA_ropeSetLength(lua_State* L)
{
	scope("(LUA) rope:setLength");

	std::shared_ptr<Rope> rope = getRope(L);
	float length = 0;
	if (rope && (lua_gettop(L) != 2 || !readNumber(L, 2, length) || length < 0))
	{
		error("Expected rope:setLength(studs), 0 or more");
		rope = nullptr;
	}
	lua_settop(L, 0);

	if (rope)
		rope->setLength(length);
	return 0;
}

//How far apart its ends are along its bends right now, which is less than its length while it hangs slack
static int LUA_ropeGetSpan(lua_State* L)
{
	scope("(LUA) rope:getSpan");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	lua_pushnumber(L, rope->getPathLength());
	return 1;
}

static int LUA_ropeGetLinks(lua_State* L)
{
	scope("(LUA) rope:getLinks");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	lua_pushinteger(L, rope->getLinks());
	return 1;
}

static int LUA_ropeSetLinks(lua_State* L)
{
	scope("(LUA) rope:setLinks");

	std::shared_ptr<Rope> rope = getRope(L);
	if (rope && (lua_gettop(L) != 2 || !lua_isinteger(L, 2) || lua_tointeger(L, 2) < Rope::minLinks || lua_tointeger(L, 2) > Rope::maxLinks))
	{
		error("Expected rope:setLinks(links), between " + std::to_string(Rope::minLinks) + " and " + std::to_string(Rope::maxLinks));
		rope = nullptr;
	}

	if (rope)
		rope->setLinks((int)lua_tointeger(L, 2));
	lua_settop(L, 0);
	return 0;
}

static int LUA_ropeGetWidth(lua_State* L)
{
	scope("(LUA) rope:getWidth");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	lua_pushnumber(L, rope->getWidth());
	return 1;
}

static int LUA_ropeSetWidth(lua_State* L)
{
	scope("(LUA) rope:setWidth");

	std::shared_ptr<Rope> rope = getRope(L);
	float width = 0;
	if (rope && (lua_gettop(L) != 2 || !readNumber(L, 2, width) || width <= 0))
	{
		error("Expected rope:setWidth(studs), more than 0");
		rope = nullptr;
	}
	lua_settop(L, 0);

	if (rope)
		rope->setWidth(width);
	return 0;
}

static int LUA_ropeGetColor(lua_State* L)
{
	scope("(LUA) rope:getColor");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	for (int a = 0; a < 3; a++)
		lua_pushnumber(L, rope->getColor()[a] / 255.0);
	return 3;
}

static int LUA_ropeSetColor(lua_State* L)
{
	scope("(LUA) rope:setColor");

	std::shared_ptr<Rope> rope = getRope(L);
	int args = lua_gettop(L);
	glm::vec3 rgb(1);
	if (rope && (args != 4 || !readVector(L, 2, rgb)))
	{
		error("Expected rope:setColor(r, g, b), each 0 to 1");
		rope = nullptr;
	}
	lua_settop(L, 0);

	if (rope)
		rope->setColor(glm::u8vec4(glm::round(glm::clamp(glm::vec4(rgb, 1.0f), 0.0f, 1.0f) * 255.0f)));
	return 0;
}

//x, y, z of where an end is tied right now
static int LUA_ropeGetEnd(lua_State* L)
{
	scope("(LUA) rope:getEnd");

	std::shared_ptr<Rope> rope = getRope(L);
	int end = 0;
	if (rope && (lua_gettop(L) != 2 || !readEnd(L, 2, end)))
	{
		error("Expected rope:getEnd(1 or 2)");
		rope = nullptr;
	}
	lua_settop(L, 0);
	if (!rope)
		return 0;

	glm::vec3 position = rope->getEnd(end);
	lua_pushnumber(L, position.x);
	lua_pushnumber(L, position.y);
	lua_pushnumber(L, position.z);
	return 3;
}

static int LUA_ropeSetEnd(lua_State* L)
{
	scope("(LUA) rope:setEnd");

	std::shared_ptr<Rope> rope = getRope(L);
	int args = lua_gettop(L);
	int end = 0;
	glm::vec3 vector(0);
	if (rope && ((args != 3 && args != 6) || !readEnd(L, 2, end) || (args == 6 && !readVector(L, 4, vector))))
	{
		error("Expected rope:setEnd(1 or 2, target[, x, y, z])");
		rope = nullptr;
	}

	if (rope)
		tieEnd(L, 3, rope, end, args == 6, vector);
	lua_settop(L, 0);
	return 0;
}

static int LUA_ropeDrawOn(lua_State* L)
{
	scope("(LUA) rope:drawOn");

	std::shared_ptr<Rope> rope = getRope(L);
	int args = lua_gettop(L);
	int end = 0;
	glm::vec3 offset(0);
	if (rope && ((args != 3 && args != 6) || !readEnd(L, 2, end) || (args == 6 && !readVector(L, 4, offset))))
	{
		error("Expected rope:drawOn(1 or 2, dynamic or nil[, x, y, z])");
		rope = nullptr;
	}

	if (rope)
	{
		if (lua_isnil(L, 3))
			rope->drawOn(end, nullptr, glm::vec3(0));
		else
		{
			lua_pushvalue(L, 3);
			std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
			if (dynamic)
				rope->drawOn(end, dynamic, offset);
		}
	}

	lua_settop(L, 0);
	return 0;
}

static int LUA_ropeGetBends(lua_State* L)
{
	scope("(LUA) rope:getBends");

	std::shared_ptr<Rope> rope = getRope(L);
	lua_settop(L, 0);
	if (!rope)
		return 0;

	lua_newtable(L);
	for (size_t a = 0; a < rope->getBends().size(); a++)
	{
		lua_newtable(L);
		for (int b = 0; b < 3; b++)
		{
			lua_pushnumber(L, rope->getBends()[a][b]);
			lua_rawseti(L, -2, b + 1);
		}
		lua_rawseti(L, -2, (lua_Integer)a + 1);
	}
	return 1;
}

static int LUA_ropeSetBends(lua_State* L)
{
	scope("(LUA) rope:setBends");

	std::shared_ptr<Rope> rope = getRope(L);
	if (rope && (lua_gettop(L) != 2 || !lua_istable(L, 2) || lua_rawlen(L, 2) > (size_t)Rope::maxBends))
	{
		error("Expected rope:setBends({{x, y, z}, ...}), at most " + std::to_string(Rope::maxBends) + " of them");
		rope = nullptr;
	}

	std::vector<glm::vec3> bends;
	if (rope)
	{
		size_t count = lua_rawlen(L, 2);
		for (size_t a = 0; a < count; a++)
		{
			glm::vec3 bend;
			lua_rawgeti(L, 2, (lua_Integer)a + 1);
			bool good = readVectorTable(L, lua_gettop(L), bend);
			lua_pop(L, 1);

			if (!good)
			{
				error("Bend " + std::to_string(a + 1) + " isn't a table of x, y, z");
				rope = nullptr;
				break;
			}
			bends.push_back(bend);
		}
	}
	lua_settop(L, 0);

	if (rope)
		rope->setBends(bends);
	return 0;
}

static int LUA_getRopeId(lua_State* L)
{
	scope("(LUA) getRopeId");

	if (lua_gettop(L) != 1 || !LUA_pd->ropes)
	{
		error("Expected 1 argument getRopeId(netId)");
		lua_settop(L, 0);
		return 0;
	}

	netIDType id = (netIDType)lua_tointeger(L, 1);
	lua_settop(L, 0);

	//Asking after a rope that went away with what it was tied to is how a script finds that out
	std::shared_ptr<Rope> rope = LUA_pd->ropes->find(id);
	if (!rope)
		return 0;

	LUA_pd->ropes->pushLua(L, rope);
	return 1;
}

static int LUA_getRopeIdx(lua_State* L)
{
	scope("(LUA) getRopeIdx");

	if (lua_gettop(L) != 1 || !LUA_pd->ropes)
	{
		error("Expected 1 argument getRopeIdx(index)");
		lua_settop(L, 0);
		return 0;
	}

	lua_Integer idx = lua_tointeger(L, 1);
	lua_settop(L, 0);

	if (idx < 0 || idx >= (lua_Integer)LUA_pd->ropes->size())
	{
		error("Invalid rope index passed, size: " + std::to_string(LUA_pd->ropes->size()) + ", index: " + std::to_string(idx));
		return 0;
	}

	LUA_pd->ropes->pushLua(L, LUA_pd->ropes->get(idx));
	return 1;
}

static int LUA_getNumRopes(lua_State* L)
{
	scope("(LUA) getNumRopes");

	lua_settop(L, 0);
	lua_pushinteger(L, LUA_pd->ropes ? (lua_Integer)LUA_pd->ropes->size() : 0);
	return 1;
}

luaL_Reg* getRopeFunctions(lua_State* L)
{
	lua_register(L, "createRope", LUA_createRope);
	lua_register(L, "getRopeId", LUA_getRopeId);
	lua_register(L, "getRopeIdx", LUA_getRopeIdx);
	lua_register(L, "getNumRopes", LUA_getNumRopes);

	//The old game's ropes were made from the dynamic they were tied to, and items have their own copy of every dynamic function
	for (const char* metatable : { "metatable_dynamic", "metatable_item" })
	{
		int top = lua_gettop(L);
		lua_getglobal(L, metatable);
		if (!lua_istable(L, -1))
			luaL_getmetatable(L, metatable);

		if (lua_istable(L, -1))
		{
			lua_pushcfunction(L, LUA_dynamicAttachByRope);
			lua_setfield(L, -2, "attachByRope");
			lua_pushcfunction(L, LUA_dynamicAttachByRopeBrick);
			lua_setfield(L, -2, "attachByRopeBrick");
			lua_pushcfunction(L, LUA_dynamicClearRopes);
			lua_setfield(L, -2, "clearRopes");
		}
		lua_settop(L, top);
	}

	luaL_Reg* regs = new luaL_Reg[16];

	int iter = 0;
	regs[iter++] = { "destroy",		LUA_ropeDestroy };
	regs[iter++] = { "getLength",	LUA_ropeGetLength };
	regs[iter++] = { "setLength",	LUA_ropeSetLength };
	regs[iter++] = { "getSpan",		LUA_ropeGetSpan };
	regs[iter++] = { "getLinks",	LUA_ropeGetLinks };
	regs[iter++] = { "setLinks",	LUA_ropeSetLinks };
	regs[iter++] = { "getWidth",	LUA_ropeGetWidth };
	regs[iter++] = { "setWidth",	LUA_ropeSetWidth };
	regs[iter++] = { "getColor",	LUA_ropeGetColor };
	regs[iter++] = { "setColor",	LUA_ropeSetColor };
	regs[iter++] = { "getEnd",		LUA_ropeGetEnd };
	regs[iter++] = { "setEnd",		LUA_ropeSetEnd };
	regs[iter++] = { "drawOn",		LUA_ropeDrawOn };
	regs[iter++] = { "getBends",	LUA_ropeGetBends };
	regs[iter++] = { "setBends",	LUA_ropeSetBends };
	regs[iter++] = { NULL, NULL };

	return regs;
}
