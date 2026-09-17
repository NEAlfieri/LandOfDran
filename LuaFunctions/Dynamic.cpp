#include "Dynamic.h"
#include "ClientLua.h"
#include "SoundLua.h"

ServerProgramData* LUA_pd = nullptr;

static int LUA_createDynamic(lua_State* L)
{
	scope("(LUA) createDynamic");

	int args = lua_gettop(L);

	if(args != 4)
	{
		error("Expected 4 arguments createDynamic(typeID, x, y, z)");
		return 0;
	}

	int typeID = lua_tointeger(L, 1);
	float x = lua_tonumber(L, 2);
	float y = lua_tonumber(L, 3);
	float z = lua_tonumber(L, 4);

	lua_pop(L, 4);

	if(!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	if (typeID >= LUA_pd->dynamicTypes.size() || typeID < 0)
	{
		error("typeID out of range");
		return 0;
	}

	std::shared_ptr<DynamicType> type = LUA_pd->dynamicTypes[typeID];
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->create(type, btVector3(x, y, z),btQuaternion::getIdentity());

	LUA_pd->dynamics->pushLua(L, dynamic);
	return 1;
}

static int LUA_dynamicDestroy(lua_State* L)
{
	scope("(LUA) dynamic:destroy");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:destroy()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	for (int b = 0; b < LUA_pd->clients.size(); b++)
	{
		bool outerBreak = false;
		for (int c = 0; c < LUA_pd->clients[b]->controlledObjects.size(); c++)
		{
			if (LUA_pd->clients[b]->controlledObjects[c]->getID() == dynamic->getID())
			{
				LUA_pd->clients[b]->controlledObjects.erase(LUA_pd->clients[b]->controlledObjects.begin() + c);
				outerBreak = true;
				break;
			}
		}
		if (outerBreak)
			break;
	}

	//Out of the inventory it's in
	if (dynamic->getKind() == DynamicKind_Item)
	{
		std::shared_ptr<Item> item = std::static_pointer_cast<Item>(dynamic);
		if (std::shared_ptr<ClientData> carrier = item->owner.lock())
			carrier->forgetItem(*item);
	}

	LUA_pd->dynamics->destroy(dynamic);

	return 0;
}

static int LUA_dynamicSetVelocity(lua_State* L)
{
	scope("(LUA) dynamic:setVelocity");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments dynamic:setVelocity(x,y,z)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//Carried items stay with whoever carries them
	if (!dynamic->isInWorld())
		return 0;

	dynamic->setVelocity(btVector3(x, y, z));
	dynamic->forcePlayerUpdate = true;
	dynamic->activate();

	return 0;
}

static int LUA_dynamicSetAngularFactor(lua_State* L)
{
	scope("(LUA) dynamic:setAngularFactor");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments dynamic:setAngularFactor(x,y,z)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->body->setAngularFactor(btVector3(x, y, z));
	dynamic->activate();

	return 0;
}

static int LUA_dynamicGetVelocity(lua_State* L)
{
	scope("(LUA) dynamic:getVelocity");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getVelocity()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	btVector3 vel = dynamic->getVelocity();
	lua_pushnumber(L, vel.x());
	lua_pushnumber(L, vel.y());
	lua_pushnumber(L, vel.z());

	return 3;
}

static int LUA_dynamicSetPosition(lua_State* L)
{
	scope("(LUA) dynamic:setPosition");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments dynamic:setPosition(x,y,z)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//Carried items stay with whoever carries them
	if (!dynamic->isInWorld())
		return 0;

	dynamic->setPosition(btVector3(x, y, z));
	dynamic->forcePlayerUpdate = true;
	dynamic->activate();

	return 0;
}

static int LUA_dynamicActivate(lua_State* L)
{
	scope("(LUA) dynamic:activate");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:activate()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//A carried item's body is out of the world
	if (!dynamic->isInWorld())
		return 0;

	dynamic->activate();

	return 0;
}

static int LUA_dynamicGetPosition(lua_State* L)
{
	scope("(LUA) dynamic:getPosition");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getPosition()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	btVector3 vel = dynamic->getPosition();
	lua_pushnumber(L, vel.x());
	lua_pushnumber(L, vel.y());
	lua_pushnumber(L, vel.z());

	return 3;
}

static int LUA_getDynamicId(lua_State* L)
{
	scope("(LUA) getDynamicId");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getDynamicId(netId)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	int id = lua_tointeger(L, -1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->find(id);

	if (!dynamic)
	{
		error("Invalid dynamic object id passed, does it exist?");
		return 0;
	}

	LUA_pd->dynamics->pushLua(L, dynamic);

	return 1;
}

static int LUA_getDynamicIdx(lua_State* L)
{
	scope("(LUA) getDynamicIdx");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getDynamicIdx(index)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	int idx = lua_tointeger(L, -1);

	if(idx < 0 || idx >= LUA_pd->dynamics->size())
	{
		error("Invalid dynamic object index passed, size: " + std::to_string(LUA_pd->dynamics->size()) + ", index: " + std::to_string(idx));
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->get(idx);

	LUA_pd->dynamics->pushLua(L, dynamic);

	return 1;
}

static int LUA_getNumDynamics(lua_State* L)
{
	scope("(LUA) getNumDynamics");

	int args = lua_gettop(L);

	if (args != 0)
	{
		error("Expected 0 arguments getNumDynamics()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	lua_pushinteger(L, LUA_pd->dynamics->size());

	return 1;
}

static int LUA_dynamicSetAngularVelocity(lua_State* L)
{
	scope("(LUA) dynamic:setAngularVelocity");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments dynamic:setAngularVelocity(x,y,z)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//Carried items stay with whoever carries them
	if (!dynamic->isInWorld())
		return 0;

	dynamic->setAngularVelocity(btVector3(x, y, z));
	dynamic->forcePlayerUpdate = true;
	dynamic->activate();

	return 0;
}

static int LUA_dynamicGetAngularVelocity(lua_State * L)
{
	scope("(LUA) dynamic:getAngularVelocity");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getAngularVelocity()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	btVector3 vel = dynamic->getAngularVelocity();
	lua_pushnumber(L, vel.x());
	lua_pushnumber(L, vel.y());
	lua_pushnumber(L, vel.z());

	return 3;
}

static int LUA_dynamicIsActive(lua_State* L)
{
	scope("(LUA) dynamic:isActive");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:isActive()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushboolean(L, dynamic->body->isActive());

	return 1;
}

static int LUA_dynamicSetGravity(lua_State* L)
{
	scope("(LUA) dynamic:setGravity");

	int args = lua_gettop(L);

	if (args != 4)
	{
		error("Expected 4 arguments dynamic:setGravity(x,y,z)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->gravityUpdated = true;
	dynamic->body->setGravity(btVector3(x, y, z));
	dynamic->activate();

	return 0;
}

static int LUA_dynamicGetGravity(lua_State* L)
{
	scope("(LUA) dynamic:getGravity");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getGravity()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	btVector3 vel = dynamic->body->getGravity();
	lua_pushnumber(L, vel.x());
	lua_pushnumber(L, vel.y());
	lua_pushnumber(L, vel.z());

	return 3;
}

static int LUA_dynamicGetFriction(lua_State* L)
{
	scope("(LUA) dynamic:getFriction");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getFriction()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, dynamic->body->getFriction());

	return 1;
}

static int LUA_dynamicGetRestitution(lua_State* L)
{
	scope("(LUA) dynamic:getRestitution");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getRestitution()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, dynamic->body->getRestitution());

	return 1;
}

static int LUA_dynamicSetFriction(lua_State* L)
{
	scope("(LUA) dynamic:setFriction");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments dynamic:setFriction(friction)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float friction = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	friction = std::clamp(friction, 0.0f, 10.0f);

	dynamic->frictionUpdated = true;
	dynamic->body->setFriction(friction);

	return 0;
}

static int LUA_dynamicSetRestitution(lua_State* L)
{
	scope("(LUA) dynamic:setRestitution");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments dynamic:setRestitution(restitution)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float friction = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	friction = std::clamp(friction, 0.0f, 10.0f);

	dynamic->restitutionUpdated = true;
	dynamic->body->setRestitution(friction);

	return 0;
}

static int LUA_dynamicGetRotation(lua_State* L)
{
	scope("(LUA) dynamic:getRotation");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getRotation()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	btQuaternion rot = dynamic->body->getWorldTransform().getRotation();
	lua_pushnumber(L, rot.w());
	lua_pushnumber(L, rot.x());
	lua_pushnumber(L, rot.y());
	lua_pushnumber(L, rot.z());

	return 4;
}

static int LUA_dynamicSetRotation(lua_State* L)
{
	scope("(LUA) dynamic:setRotation");

	int args = lua_gettop(L);

	if (args != 5 && args != 4)
	{
		error("Expected 4 or 5 arguments dynamic:setRotation(w,x,y,z) or dynamic:setRotation(yaw,pitch,roll)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (args == 4)
	{
		std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

		if (!dynamic)
		{
			error("Invalid dynamic object passed, was it deleted already?");
			return 0;
		}

		//Carried items stay with whoever carries them
		if (!dynamic->isInWorld())
			return 0;

		btTransform t = dynamic->body->getWorldTransform();
		t.setRotation(btQuaternion(x,y,z));
		dynamic->body->setWorldTransform(t);
		dynamic->activate();

		return 0;
	}

	float w = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//Carried items stay with whoever carries them
	if (!dynamic->isInWorld())
		return 0;

	btTransform t = dynamic->body->getWorldTransform();
	t.setRotation(btQuaternion(x, y, z, w));
	dynamic->body->setWorldTransform(t);
	dynamic->forcePlayerUpdate = true;
	dynamic->activate();

	return 0;
}

static int LUA_dynamicGetMass(lua_State* L)
{
	scope("(LUA) dynamic:getMass");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments dynamic:getMass()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, dynamic->body->getMass());

	return 1;
}

static int LUA_dynamicSetMassProps(lua_State* L)
{
	scope("(LUA) dynamic:setMassProps");

	int args = lua_gettop(L);

	if (args != 5)
	{
		error("Expected 5 arguments dynamic:setMassProps(mass,centerX,centerY,centerZ)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float mass = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->body->setMassProps(mass, btVector3(x, y, z));

	return 0;
}

static int LUA_dynamicSetMeshColor(lua_State* L)
{
	scope("(LUA) dynamic:setMeshColor");

	int args = lua_gettop(L);

	if (args != 6)
	{
		error("Expected 6 arguments dynamic:setMeshcolor(meshName,r,g,b,a)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
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

	const char *name = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!name)
	{
		error("Invalid mesh name passed");
		return 0;
	}

	std::string meshName = std::string(name);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	ENetPacket *packet = dynamic->setMeshColor(meshName, glm::vec4(r, g, b, a));
	if(packet)
		LUA_server->broadcast(packet,OtherReliable);

	return 0;
}

static int LUA_dynamicGetMeshAt(lua_State* L)
{
	scope("(LUA) dynamic:getMeshAt");

	if (lua_gettop(L) != 4)
	{
		error("Expected 4 arguments dynamic:getMeshAt(x,y,z)");
		lua_settop(L, 0);
		return 0;
	}

	float z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//The point in the model's own space: undo where the dynamic is and which way it's turned, then its type's scale
	const btTransform& transform = dynamic->body->getWorldTransform();
	const btQuaternion& rotation = transform.getRotation();
	glm::vec3 offset = b2g3(transform.getOrigin());
	glm::quat turn(rotation.w(), rotation.x(), rotation.y(), rotation.z());

	std::shared_ptr<Model> model = dynamic->getType()->getModel();
	glm::vec3 point = (glm::inverse(turn) * (glm::vec3(x, y, z) - offset)) / model->baseScale;

	int meshIdx = model->getMeshAtPoint(point);
	if (meshIdx == -1)
		lua_pushnil(L);
	else
		lua_pushstring(L, model->getMeshName(meshIdx).c_str());
	return 1;
}

static int LUA_dynamicSetMeshDecal(lua_State* L)
{
	scope("(LUA) dynamic:setMeshDecal");

	if (lua_gettop(L) != 3)
	{
		error("Expected 3 arguments dynamic:setMeshDecal(meshName,faceName)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	const char* face = lua_tostring(L, -1);
	lua_pop(L, 1);
	const char* mesh = lua_tostring(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	if (!mesh || !face)
	{
		error("Mesh and decal names have to be strings");
		return 0;
	}

	ENetPacket* packet = dynamic->setMeshDecal(std::string(mesh), std::string(face));
	if (packet)
		LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_dynamicSetHighlight(lua_State* L)
{
	scope("(LUA) dynamic:setHighlight");

	int args = lua_gettop(L);

	if (args != 6)
	{
		error("Expected 6 arguments dynamic:setHighlight(r,g,b,a,thickness)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
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

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	glm::vec4 color(r, g, b, a);
	dynamic->setHighlight(color, thickness);

	ENetPacket* packet = dynamic->makeHighlightPacket(color, thickness);
	LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_dynamicClearHighlight(lua_State* L)
{
	scope("(LUA) dynamic:clearHighlight");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:clearHighlight()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	glm::vec4 color(0, 0, 0, 0);
	dynamic->setHighlight(color, 0.0f);

	ENetPacket* packet = dynamic->makeHighlightPacket(color, 0.0f);
	LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

static int LUA_dynamicSetNameTag(lua_State* L)
{
	scope("(LUA) dynamic:setNameTag");

	int args = lua_gettop(L);

	if (args != 5)
	{
		error("Expected 5 arguments dynamic:setNameTag(text,r,g,b)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float b = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float g = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float r = lua_tonumber(L, -1);
	lua_pop(L, 1);

	const char* text = lua_tostring(L, -1);
	std::string tag = text ? std::string(text) : "";
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->setNameTag(tag, glm::vec3(r, g, b));

	LUA_server->broadcast(dynamic->makeNameTagPacket(), OtherReliable);

	return 0;
}

static int LUA_newDynamicType(lua_State* L)
{
	scope("(LUA) newDynamicType");

	int args = lua_gettop(L);

	if (args != 5)
	{
		error("Expected 5 arguments newDynamicType(scriptName,modelFilePath,scaleX,scaleY,scaleZ)");
		return 0;
	}

	glm::vec3 scale;
	scale.z = lua_tonumber(L, -1);
	lua_pop(L, 1);
	scale.y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	scale.x = lua_tonumber(L, -1);
	lua_pop(L, 1);

	const char *modelFilePath = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!modelFilePath)
	{
		error("Invalid model file path passed");
		return 0;
	}

	const char *scriptName = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!scriptName)
	{
		error("Invalid script name passed");
		return 0;
	}

	auto testType = std::make_shared<DynamicType>();
	testType->serverSideLoad(modelFilePath, LUA_pd->dynamicTypes.size(), scale);
	testType->scriptName = scriptName;
	LUA_pd->dynamicTypes.push_back(testType);
	LUA_pd->allNetTypes.push_back(testType);

	info("Added new dynamic type: " + std::string(scriptName));

	lua_pushinteger(L, testType->getID());
	return 1;
}

static int LUA_getDynamicType(lua_State* L)
{
	scope("(LUA) getDynamicType");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getDynamicType(scriptName)");
		return 0;
	}

	const char *scriptName = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!scriptName)
	{
		error("Invalid script name passed");
		return 0;
	}

	for (int i = 0; i < LUA_pd->dynamicTypes.size(); i++)
	{
		if (LUA_pd->dynamicTypes[i]->scriptName == scriptName)
		{
			lua_pushinteger(L, i);
			return 1;
		}
	}

	error("Dynamic type not found: " + std::string(scriptName));
	lua_pushnil(L);
	return 1;
}

static int LUA_addAnimation(lua_State* L)
{
	scope("(LUA) addAnimation");

	int args = lua_gettop(L);

	if (args != 7)
	{
		error("Expected 7 arguments addAnimation(typeID,animationName,startFrame,endFrame,speed,fadeInMS,fadeOutMS)");
		return 0;
	}

	int fadeOutMS = lua_tointeger(L, -1);
	lua_pop(L, 1);

	int fadeInMS = lua_tointeger(L, -1);
	lua_pop(L, 1);

	float speed = lua_tonumber(L, -1);
	lua_pop(L, 1);

	int endFrame = lua_tointeger(L, -1);
	lua_pop(L, 1);

	int startFrame = lua_tointeger(L, -1);
	lua_pop(L, 1);

	const char *animationName = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!animationName)
	{
		error("Invalid animation name passed");
		return 0;
	}

	int typeID = lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (typeID >= LUA_pd->dynamicTypes.size() || typeID < 0)
	{
		error("typeID out of range");
		return 0;
	}

	std::shared_ptr<DynamicType> type = LUA_pd->dynamicTypes[typeID];

	if(!type)
	{
		error("Invalid typeID passed");
		return 0;
	}

	Animation anim;
	anim.defaultSpeed = speed;
	anim.startTime = startFrame;
	anim.endTime = endFrame;
	anim.name = animationName;
	anim.fadeInMS = fadeInMS;
	anim.fadeOutMS = fadeOutMS;

	type->getModel()->addAnimation(anim);

	return 0;
}

static int LUA_getTypeNodePosition(lua_State* L)
{
	scope("(LUA) getTypeNodePosition");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments getTypeNodePosition(typeID,nodeName)");
		return 0;
	}

	const char* nodeName = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!nodeName)
	{
		error("Invalid node name passed");
		return 0;
	}

	int typeID = lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (typeID >= (int)LUA_pd->dynamicTypes.size() || typeID < 0)
	{
		error("typeID out of range");
		return 0;
	}

	std::shared_ptr<DynamicType> type = LUA_pd->dynamicTypes[typeID];

	if (!type || !type->getModel())
	{
		error("Invalid typeID passed");
		return 0;
	}

	glm::vec3 position;
	if (!type->getModel()->getNodePosition(nodeName, position))
		return 0;

	lua_pushnumber(L, position.x);
	lua_pushnumber(L, position.y);
	lua_pushnumber(L, position.z);
	return 3;
}

//TODO: Probably should be in other functions file cause it can return statics as well
static int LUA_raycast(lua_State* L)
{
	scope("(LUA) raycast");

	int args = lua_gettop(L);

	if (args != 6 && args != 7)
	{
		error("Expected 6 or 7 arguments raycast(startX,startY,startZ,endX,endY,endZ[,dynamic to ignore])");
		return 0;
	}

	btRigidBody *ignore = nullptr;
	if (args == 7)
	{
		if (lua_isnil(L, -1))
			lua_pop(L, 1);
		else
		{
			std::shared_ptr<Dynamic> toIgnore = LUA_pd->dynamics->popLua(L);
			if (!toIgnore)
			{
				error("Invalid dynamic object passed, was it deleted already?");
				return 0;
			}
			else
				ignore = toIgnore->body;
		}
	}

	float endZ = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float endY = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float endX = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float startZ = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float startY = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float startX = lua_tonumber(L, -1);
	lua_pop(L, 1);
	 
	btVector3 start = btVector3(startX, startY, startZ);
	btVector3 end = btVector3(endX, endY, endZ);

	btVector3 hitPosition, hitNormal;
	btRigidBody * result = LUA_pd->physicsWorld->doRaycast(start, end, ignore, hitPosition, hitNormal);
	return pushRaycastHit(L, result, start, hitPosition, hitNormal);
}

int pushRaycastHit(lua_State* L, btRigidBody* result, const btVector3& start, const btVector3& hitPosition, const btVector3& hitNormal)
{
	if (!pushRaycastResult(L, result))
	{
		//The ground has no Lua object, but where it was hit still follows the nil
		if (!result || result->getUserIndex() != groundPlane)
			return 1;
	}

	lua_pushnumber(L, hitPosition.x());
	lua_pushnumber(L, hitPosition.y());
	lua_pushnumber(L, hitPosition.z());
	lua_pushnumber(L, hitNormal.x());
	lua_pushnumber(L, hitNormal.y());
	lua_pushnumber(L, hitNormal.z());
	lua_pushnumber(L, (hitPosition - start).length());
	return 8;
}

bool pushRaycastResult(lua_State* L, btRigidBody* result)
{
	if (!result)
	{
		lua_pushnil(L);
		return false;
	}

	if (result->getUserIndex() == dynamicBody)
	{
		std::shared_ptr<Dynamic> resultDynamic = dynamicFromBody(result);

		if (resultDynamic)
		{
			LUA_pd->dynamics->pushLua(L, resultDynamic);
			return true;
		}
	}
	else if (result->getUserIndex() == staticBody)
	{
		std::shared_ptr<StaticObject> resultStatic = staticFromBody(result);

		if (resultStatic)
		{
			LUA_pd->statics->pushLua(L, resultStatic);
			return true;
		}
	}
	else if (result->getUserIndex() == brickBody)
	{
		LUA_pd->bricks->pushLua(L, (Brick*)result->getUserPointer());
		return true;
	}
	else if (result->getUserIndex() == vehicleBody)
	{
		if (std::shared_ptr<Vehicle> vehicle = vehicleFromBody(result))
		{
			LUA_pd->vehicles->pushLua(L, vehicle);
			return true;
		}
	}

	lua_pushnil(L);
	return false;
}

static int LUA_dynamicGetNumControllers(lua_State* L)
{
	scope("(LUA) dynamic:getNumControllers");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:getNumControllers()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	int count = 0;
	for (int b = 0; b < LUA_pd->clients.size(); b++)
	{
		for (int c = 0; c < LUA_pd->clients[b]->controlledObjects.size(); c++)
		{
			if (LUA_pd->clients[b]->controlledObjects[c]->getID() == dynamic->getID())
			{
				count++;
				break;
			}
		}
	}

	lua_pushinteger(L, count);

	return 1;
}

static int LUA_dynamicGetControllerIdx(lua_State* L)
{
	scope("(LUA) dynamic:getControllerIdx");

	int args = lua_gettop(L);

	if (args != 2)
	{
		error("Expected 2 arguments dynamic:getControllerIdx(index)");
		return 0;
	}

	int index = lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	if (index < 0)
	{
		error("Index " + std::to_string(index) + " out of bounds for dynamic:getControllerIdx");
		return 0;
	}

	int count = 0;
	for (int b = 0; b < LUA_pd->clients.size(); b++)
	{
		bool found = false;
		for (int c = 0; c < LUA_pd->clients[b]->controlledObjects.size(); c++)
		{
			if (LUA_pd->clients[b]->controlledObjects[c]->getID() == dynamic->getID())
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			if (count == index)
			{
				pushClientLua(L, LUA_pd->clients[b]->client);
				return 1;
			}
			count++;
		}
	}

	error("Index " + std::to_string(index) + " out of bounds for dynamic:getControllerIdx");
	return 0;
}

static int LUA_dynamicSnapToCursor(lua_State* L)
{
	scope("(LUA) dynamic:snapToCursor");

	int args = lua_gettop(L);

	if (args != 5)
	{
		error("Expected 5 arguments dynamic:snapToCursor(client,xOffset,yOffset,zOffset)");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	float zOffset = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float yOffset = lua_tonumber(L, -1);
	lua_pop(L, 1);
	float xOffset = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<JoinedClient> client = popClientLua(L);

	if (!client)
	{
		error("Invalid client passed to dynamic:snapToCursor");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//Carried items stay with whoever carries them
	if (!dynamic->isInWorld())
		return 0;

	dynamic->snapToCursor(client, glm::vec3(xOffset, yOffset, zOffset));

	return 0;
}

static int LUA_dynamicUnsnap(lua_State* L)
{
	scope("(LUA) dynamic:unsnap");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:unsnap()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->unsnapFromCursor();

	return 0;
}

static int LUA_dynamicIsSnapped(lua_State* L)
{
	scope("(LUA) dynamic:isSnapped");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:isSnapped()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushboolean(L, dynamic->isSnappedToCursor());

	return 1;
}

static int LUA_dynamicGetSnapClient(lua_State* L)
{
	scope("(LUA) dynamic:getSnapClient");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 argument dynamic:getSnapClient()");
		return 0;
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = dynamic->snappedToClient.lock();

	if (!client)
	{
		lua_pushnil(L);
		return 1;
	}

	pushClientLua(L, client);
	return 1;
}

static int LUA_dynamicSetBuoyancy(lua_State* L)
{
	scope("(LUA) dynamic:setBuoyancy");

	if (lua_gettop(L) != 2 || !lua_isnumber(L, 2))
	{
		error("Expected 2 arguments dynamic:setBuoyancy(buoyancy)");
		lua_settop(L, 0);
		return 0;
	}

	float buoyancy = std::clamp((float)lua_tonumber(L, 2), 0.0f, 10.0f);
	lua_settop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	dynamic->buoyancy = buoyancy;

	//Clients simulate their own player in water, so they need it too
	LUA_server->broadcast(dynamic->makeBuoyancyPacket(), OtherReliable);
	return 0;
}

static int LUA_dynamicGetBuoyancy(lua_State* L)
{
	scope("(LUA) dynamic:getBuoyancy");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument dynamic:getBuoyancy()");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushnumber(L, dynamic->buoyancy);
	return 1;
}

static int LUA_addProjectile(lua_State* L)
{
	scope("(LUA) addProjectile");

	const std::string usage = "addProjectile(typeID, x, y, z, velX, velY, velZ[, tag[, shooter]])";

	int args = lua_gettop(L);
	if (args < 7 || args > 9)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return 0;
	}

	for (int a = 1; a <= 7; a++)
	{
		if (lua_type(L, a) != LUA_TNUMBER)
		{
			error("Expected numbers for the type ID, position, and velocity in " + usage);
			lua_settop(L, 0);
			return 0;
		}
	}

	std::string tag = "";
	if (args >= 8 && !lua_isnil(L, 8))
	{
		if (lua_type(L, 8) != LUA_TSTRING)
		{
			error("Expected a string or nil for the tag in " + usage);
			lua_settop(L, 0);
			return 0;
		}
		tag = lua_tostring(L, 8);
	}

	if (!LUA_pd->dynamics)
	{
		error("dynamics ObjHolder is null");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Dynamic> shooter = nullptr;
	if (args == 9 && !lua_isnil(L, 9))
	{
		shooter = LUA_pd->dynamics->popLua(L);
		if (!shooter)
		{
			error("Invalid shooter passed to " + usage + ", was it deleted already?");
			lua_settop(L, 0);
			return 0;
		}
	}

	int typeID = lua_tointeger(L, 1);
	btVector3 position(lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	btVector3 velocity(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	lua_settop(L, 0);

	if (typeID < 0 || typeID >= LUA_pd->dynamicTypes.size())
	{
		error("typeID out of range in " + usage);
		return 0;
	}

	std::shared_ptr<Dynamic> projectile = LUA_pd->dynamics->create(LUA_pd->dynamicTypes[typeID], position, btQuaternion::getIdentity());
	projectile->isProjectile = true;
	projectile->projectileTag = tag;
	projectile->setVelocity(velocity);
	projectile->faceVelocity();
	projectile->body->setActivationState(DISABLE_DEACTIVATION);

	//Small and fast, so each step sweeps it along the way it moved instead of letting it skip through thin bricks
	btVector3 low, high;
	projectile->body->getCollisionShape()->getAabb(btTransform::getIdentity(), low, high);
	btVector3 halfSize = (high - low) * 0.5;
	btScalar thinnest = std::min(halfSize.x(), std::min(halfSize.y(), halfSize.z()));
	projectile->body->setCcdMotionThreshold(thinnest);
	projectile->body->setCcdSweptSphereRadius(thinnest * 0.8);

	if (shooter && shooter->body)
	{
		projectile->body->setIgnoreCollisionCheck(shooter->body, true);
		projectile->projectileShooter = shooter;
		projectile->ignoredShooterBody = shooter->body;
	}

	LUA_pd->dynamics->pushLua(L, projectile);
	return 1;
}

static int LUA_dynamicIsProjectile(lua_State* L)
{
	scope("(LUA) dynamic:isProjectile");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument dynamic:isProjectile()");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushboolean(L, dynamic->isProjectile);
	return 1;
}

static int LUA_dynamicIsItem(lua_State* L)
{
	scope("(LUA) dynamic:isItem");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument dynamic:isItem()");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	lua_pushboolean(L, dynamic->getKind() == DynamicKind_Item);
	return 1;
}

static int LUA_dynamicPlayAnimation(lua_State* L)
{
	scope("(LUA) dynamic:playAnimation");

	int args = lua_gettop(L);
	if (args != 2 && args != 3)
	{
		error("Expected dynamic:playAnimation(name[,loop])");
		lua_settop(L, 0);
		return 0;
	}

	const char* name = lua_tostring(L, 2);
	bool loop = args == 3 && lua_toboolean(L, 3);
	lua_settop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	int id = name ? dynamic->getType()->getModel()->getAnimationID(name) : -1;
	if (id == -1)
	{
		error("Dynamic type " + dynamic->getType()->scriptName + " has no animation named " + std::string(name ? name : "nil") + ", see addAnimation");
		return 0;
	}

	if (loop)
		dynamic->startLoop(id);
	else
		dynamic->playOneShot(id);
	return 0;
}

static int LUA_dynamicStopAnimation(lua_State* L)
{
	scope("(LUA) dynamic:stopAnimation");

	int args = lua_gettop(L);
	if (args != 1 && args != 2)
	{
		error("Expected dynamic:stopAnimation([name])");
		lua_settop(L, 0);
		return 0;
	}

	const char* name = args == 2 ? lua_tostring(L, 2) : nullptr;
	lua_settop(L, 1);

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	//No name stops every loop Lua started
	int id = -1;
	if (name)
	{
		id = dynamic->getType()->getModel()->getAnimationID(name);
		if (id == -1)
		{
			error("Dynamic type " + dynamic->getType()->scriptName + " has no animation named " + std::string(name) + ", see addAnimation");
			return 0;
		}
	}

	dynamic->stopLoop(id);
	return 0;
}

luaL_Reg* getDynamicFunctions(lua_State *L)
{
	//Register dynamic global functions:
	lua_register(L, "createDynamic", LUA_createDynamic);
	lua_register(L, "getDynamicId", LUA_getDynamicId);
	lua_register(L, "getDynamicIdx", LUA_getDynamicIdx);
	lua_register(L, "getNumDynamics", LUA_getNumDynamics);
	lua_register(L, "newDynamicType", LUA_newDynamicType);
	lua_register(L, "getTypeNodePosition", LUA_getTypeNodePosition);
	lua_register(L, "getDynamicType", LUA_getDynamicType);
	lua_register(L, "addAnimation", LUA_addAnimation);
	lua_register(L, "raycast", LUA_raycast);
	lua_register(L, "addProjectile", LUA_addProjectile);

	//Create table of dynamic metatable functions:
	luaL_Reg* regs = new luaL_Reg[41];

	int iter = 0;
	regs[iter++] = { "destroy",     LUA_dynamicDestroy };
	regs[iter++] = { "getVelocity", LUA_dynamicGetVelocity };
	regs[iter++] = { "getPosition", LUA_dynamicGetPosition };
	regs[iter++] = { "setPosition", LUA_dynamicSetPosition };
	regs[iter++] = { "setVelocity", LUA_dynamicSetVelocity };
	regs[iter++] = { "setAngularVelocity", LUA_dynamicSetAngularVelocity };
	regs[iter++] = { "getAngularVelocity", LUA_dynamicGetAngularVelocity };
	regs[iter++] = { "setAngularFactor", LUA_dynamicSetAngularFactor };
	regs[iter++] = { "activate",    LUA_dynamicActivate };
	regs[iter++] = { "isActive",    LUA_dynamicIsActive };
	regs[iter++] = { "getGravity",    LUA_dynamicGetGravity};
	regs[iter++] = { "setGravity",    LUA_dynamicSetGravity };
	regs[iter++] = { "setRestitution",    LUA_dynamicSetRestitution };
	regs[iter++] = { "getRestitution",    LUA_dynamicGetRestitution };
	regs[iter++] = { "setFriction",    LUA_dynamicSetFriction };
	regs[iter++] = { "getFriction",    LUA_dynamicGetFriction };
	regs[iter++] = { "setRotation",    LUA_dynamicSetRotation };
	regs[iter++] = { "getRotation",    LUA_dynamicGetRotation };
	regs[iter++] = { "getMass",    LUA_dynamicGetMass };
	regs[iter++] = { "setMassProps",    LUA_dynamicSetMassProps };
	regs[iter++] = { "setMeshColor",    LUA_dynamicSetMeshColor };
	regs[iter++] = { "getMeshAt",    LUA_dynamicGetMeshAt };
	regs[iter++] = { "playAnimation",    LUA_dynamicPlayAnimation };
	regs[iter++] = { "stopAnimation",    LUA_dynamicStopAnimation };
	regs[iter++] = { "setMeshDecal",    LUA_dynamicSetMeshDecal };
	regs[iter++] = { "setHighlight",    LUA_dynamicSetHighlight };
	regs[iter++] = { "clearHighlight",    LUA_dynamicClearHighlight };
	regs[iter++] = { "setNameTag",    LUA_dynamicSetNameTag };
	regs[iter++] = { "getNumControllers", LUA_dynamicGetNumControllers };
	regs[iter++] = { "getControllerIdx", LUA_dynamicGetControllerIdx };
	regs[iter++] = { "snapToCursor", LUA_dynamicSnapToCursor };
	regs[iter++] = { "unsnap", LUA_dynamicUnsnap };
	regs[iter++] = { "isSnapped", LUA_dynamicIsSnapped };
	regs[iter++] = { "getSnapClient", LUA_dynamicGetSnapClient };
	regs[iter++] = { "playSound", LUA_dynamicPlaySound };
	regs[iter++] = { "startSoundLoop", LUA_dynamicStartSoundLoop };
	regs[iter++] = { "setBuoyancy", LUA_dynamicSetBuoyancy };
	regs[iter++] = { "getBuoyancy", LUA_dynamicGetBuoyancy };
	regs[iter++] = { "isItem", LUA_dynamicIsItem };
	regs[iter++] = { "isProjectile", LUA_dynamicIsProjectile };
	regs[iter++] = { NULL, NULL };

	return regs;
}



