#include "Static.h"

static int LUA_staticSetMeshColor(lua_State* L)
{
	scope("(LUA) static:setMeshColor");

	int args = lua_gettop(L);

	if (args != 6)
	{
		error("Expected 6 arguments static:setMeshcolor(meshName,r,g,b,a)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	float a = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float b = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float g = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float r = lua_tonumber(L, -1);
	lua_pop(L, 1);

	const char* name = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!name)
	{
		error("Invalid mesh name passed");
		return 0;
	}

	std::string meshName = std::string(name);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	ENetPacket* packet = staticObject->setMeshColor(meshName, glm::vec4(r, g, b, a));
	if (packet)
		LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_staticSetHighlight(lua_State* L)
{
	scope("(LUA) static:setHighlight");

	int args = lua_gettop(L);

	if (args != 6)
	{
		error("Expected 6 arguments static:setHighlight(r,g,b,a,thickness)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	float thickness = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float a = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float b = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float g = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float r = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	glm::vec4 color(r, g, b, a);
	staticObject->setHighlight(color, thickness);

	ENetPacket* packet = staticObject->makeHighlightPacket(color, thickness);
	LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_staticClearHighlight(lua_State* L)
{
	scope("(LUA) static:clearHighlight");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument static:clearHighlight()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	glm::vec4 color(0, 0, 0, 0);
	staticObject->setHighlight(color, 0.0f);

	ENetPacket* packet = staticObject->makeHighlightPacket(color, 0.0f);
	LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_staticGetFriction(lua_State* L)
{
	scope("(LUA) static:getFriction");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments static:getFriction()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, staticObject->body->getFriction());

	return 1;
}

static int LUA_staticGetRestitution(lua_State* L)
{
	scope("(LUA) static:getRestitution");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments static:getRestitution()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, staticObject->body->getRestitution());

	return 1;
}

static int LUA_staticSetFriction(lua_State* L)
{
	scope("(LUA) static:setFriction");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments static:setFriction(friction)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	float friction = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	friction = std::clamp(friction, 0.0f, 10.0f);

	staticObject->frictionUpdated = true;
	staticObject->body->setFriction(friction);

	return 0;
}

static int LUA_staticSetRestitution(lua_State* L)
{
	scope("(LUA) static:setRestitution");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments static:setRestitution(restitution)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	float friction = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	friction = std::clamp(friction, 0.0f, 10.0f);

	staticObject->restitutionUpdated = true;
	staticObject->body->setRestitution(friction);

	return 0;
}

static int LUA_staticGetPosition(lua_State* L)
{
	scope("(LUA) static:getPosition");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments static:getPosition()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	btVector3 vel = staticObject->getPosition();
	lua_pushnumber(L, vel.x());
	lua_pushnumber(L, vel.y());
	lua_pushnumber(L, vel.z());

	return 3;
}

static int LUA_staticGetRotation(lua_State* L)
{
	scope("(LUA) static:getRotation");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments static:getRotation()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	btQuaternion rot = staticObject->body->getWorldTransform().getRotation();
	lua_pushnumber(L, rot.w());
	lua_pushnumber(L, rot.x());
	lua_pushnumber(L, rot.y());
	lua_pushnumber(L, rot.z());

	return 4;
}

static int LUA_createStatic(lua_State* L)
{
	scope("(LUA) createStatic");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments createStatic(typeID, x, y, z)");
		return 0;
	}

	int typeID = lua_tointeger(L, 1);
	float x = lua_tonumber(L, 2);
	float y = lua_tonumber(L, 3);
	float z = lua_tonumber(L, 4);

	lua_pop(L, 4);

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	if (typeID >= LUA_pd->dynamicTypes.size() || typeID < 0)
	{
		error("typeID out of range");
		return 0;
	}

	std::shared_ptr<DynamicType> type = LUA_pd->dynamicTypes[typeID];
	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->create(type, btVector3(x, y, z), btQuaternion::getIdentity());

	LUA_pd->statics->pushLua(L, staticObject);
	return 1;
}



static int LUA_getStaticId(lua_State* L)
{
	scope("(LUA) getStaticId");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getStaticId(netId)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	int id = lua_tointeger(L, -1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->find(id);

	if (!staticObject)
	{
		error("Invalid static object id passed, does it exist?");
		return 0;
	}

	LUA_pd->statics->pushLua(L, staticObject);

	return 1;
}

static int LUA_getStaticIdx(lua_State* L)
{
	scope("(LUA) getStaticIdx");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getStaticIdx(index)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	int idx = lua_tointeger(L, -1);

	if (idx < 0 || idx >= LUA_pd->statics->size())
	{
		error("Invalid static object index passed, size: " + std::to_string(LUA_pd->statics->size()) + ", index: " + std::to_string(idx));
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->get(idx);

	LUA_pd->statics->pushLua(L, staticObject);

	return 1;
}

static int getNumStatics(lua_State* L)
{
	scope("(LUA) getNumStatics");

	int args = lua_gettop(L);

	if (args != 0)
	{
		error("Expected 0 arguments getNumStatics()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	lua_pushinteger(L, LUA_pd->statics->size());

	return 1;
}

static int LUA_staticDestroy(lua_State* L)
{
	scope("(LUA) static:destroy");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument static:destroy()");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("static ObjHolder is null");
		return 0;
	}

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	LUA_pd->statics->destroy(staticObject);

	return 0;
}

static int LUA_staticSetColliding(lua_State* L)
{
	scope("(LUA) static:setColliding");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments static:setColliding(bool)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	bool collides = lua_toboolean(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	staticObject->setColliding(collides);

	return 0;
}

static int LUA_staticSetHidden(lua_State* L)
{
	scope("(LUA) static:setHidden");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments static:setHidden(bool)");
		return 0;
	}

	if (!LUA_pd->statics)
	{
		error("statics ObjHolder is null");
		return 0;
	}

	bool collides = lua_toboolean(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<StaticObject> staticObject = LUA_pd->statics->popLua(L);

	if (!staticObject)
	{
		error("Invalid static object passed, was it deleted already?");
		return 0;
	}

	staticObject->setHiddenServer(collides);

	return 0;
}

luaL_Reg* getStaticFunctions(lua_State* L)
{
	//Register static global functions:
	lua_register(L, "createStatic", LUA_createStatic);
	lua_register(L, "getStaticId", LUA_getStaticId);
	lua_register(L, "getStaticIdx", LUA_getStaticIdx);
	lua_register(L, "getNumStatics", getNumStatics);

	//Create table of static metatable functions:
	luaL_Reg* regs = new luaL_Reg[13];

	int iter = 0;
	regs[iter++] = { "destroy",     LUA_staticDestroy };
	regs[iter++] = { "getPosition",     LUA_staticGetPosition };
	regs[iter++] = { "getRotation",     LUA_staticGetRotation };
	regs[iter++] = { "getFriction",     LUA_staticGetFriction };
	regs[iter++] = { "getRestitution",     LUA_staticGetRestitution };
	regs[iter++] = { "setFriction",     LUA_staticSetFriction };
	regs[iter++] = { "setRestitution",     LUA_staticSetRestitution };
	regs[iter++] = { "setMeshColor",     LUA_staticSetMeshColor };
	regs[iter++] = { "setHighlight",     LUA_staticSetHighlight };
	regs[iter++] = { "clearHighlight",     LUA_staticClearHighlight };
	regs[iter++] = { "setColliding",     LUA_staticSetColliding };
	regs[iter++] = { "setHidden",     LUA_staticSetHidden };
	regs[iter++] = { NULL, NULL };

	return regs;
}
