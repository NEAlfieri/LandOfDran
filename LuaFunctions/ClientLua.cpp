#include "ClientLua.h"
#include "Dynamic.h" //pushRaycastResult

Server * LUA_server = nullptr;

void pushClientLua(lua_State* L, std::shared_ptr<JoinedClient> client)
{
	std::string metatableName = "metatable_client";

	//Set class metatable
	lua_newtable(L);
	lua_getglobal(L, metatableName.c_str());
	lua_setmetatable(L, -2);

	//Push class-unique server ID
	lua_pushinteger(L, client->getNetId());
	lua_setfield(L, -2, "id");

	//Allow unambigious type checking
	lua_pushinteger(L, ClientTypeId);
	lua_setfield(L, -2, "type");

	//Lua will automatically deallocate the space for the weak_ptr itself when the table is garbage collected
	std::weak_ptr<JoinedClient>* userdata = (std::weak_ptr<JoinedClient>*)lua_newuserdata(L, sizeof(std::weak_ptr<JoinedClient>));
	new(userdata) std::weak_ptr<JoinedClient>(client->me);
	lua_setfield(L, -2, "ptr");
}

std::shared_ptr<JoinedClient> popClientLua(lua_State* L)
{
	scope("popLua");

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		error("No table for supposed JoinedClient");
		return nullptr;
	}

	lua_getfield(L, -1, "type");
	if (!lua_isinteger(L, -1))
	{
		lua_pop(L, 2);
		error("No type field for supposed JoinedClient");
		return nullptr;
	}

	SimObjectType poppedType = (SimObjectType)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (poppedType != ClientTypeId)
	{
		lua_pop(L, 1);
		error("JoinedClient type mismatch: " + std::to_string(poppedType)+", invalid Lua cast");
		return nullptr;
	}

	lua_getfield(L, -1, "ptr");
	if (!lua_isuserdata(L, -1))
	{
		lua_pop(L, 2);
		error("No ptr field for supposed JoinedClient");
		return nullptr;
	}

	std::weak_ptr<JoinedClient>* obj = (std::weak_ptr<JoinedClient>*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (!obj)
	{
		lua_pop(L, 1);
		error("Lua userdata for JoinedClient weak_ptr not set");
		return nullptr;
	}

	if (obj->expired())
	{
		lua_getfield(L, -1, "id");
		if (lua_isinteger(L, -1))
			error("JoinedClient with ID " + std::to_string(lua_tointeger(L, -1)) + " was deleted");
		else
			error("JoinedClient was deleted, could not get ID");

		lua_pop(L, 2);
		return nullptr;
	}

	lua_pop(L, 1);
	return obj->lock();
}

static int LUA_clientKick(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:kick()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	client->kick(KickReason::LuaKick);

	return 0;
}

static int LUA_clientMessage(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:message(message)");
		return 0;
	}

	const char* msg = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!msg)
	{
		error("Expected string for message");
		return 0;
	}

	std::string message = std::string(msg);
	if (message.length() < 1)
		return 0;
	if (message.length() > 255)
		message = message.substr(0, 255);

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	client->sendChat(message);

	return 0;
}

static int LUA_messageAll(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument messageAll(message)");
		return 0;
	}

	const char* msg = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!msg)
	{
		error("Expected string for message");
		return 0;
	}

	std::string message = std::string(msg);
	if (message.length() < 1)
		return 0;
	if (message.length() > 255)
		message = message.substr(0, 255);

	if (!LUA_server)
	{
		error("Server not set");
		return 0;
	}

	LUA_server->broadcastChat(message);

	return 0;
}

static int LUA_getNumClients(lua_State* L)
{
	if (lua_gettop(L) != 0)
	{
		error("Expected 0 arguments getNumClients()");
		return 0;
	}

	if (!LUA_server)
	{
		error("Server not set");
		return 0;
	}

	lua_pushinteger(L, LUA_server->getNumClients());

	return 1;
}

static int LUA_getClientIdx(lua_State* L)
{
	scope("(LUA) getClientIdx");

	int args = lua_gettop(L);

	if (args != 1)
	{
		error("Expected 1 arguments getClientIdx(index)");
		return 0;
	}

	if (!LUA_server)
	{
		error("Server not set");
		return 0;
	}

	int idx = lua_tointeger(L, -1);

	if (idx < 0 || idx >= LUA_server->getNumClients())
	{
		error("Invalid client index passed, size: " + std::to_string(LUA_server->getNumClients()) + ", index: " + std::to_string(idx));
		return 0;
	}

	pushClientLua(L, LUA_server->getClientByIndex(idx));

	return 1;
}

static int LUA_clientGetName(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getName()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushstring(L, client->name.c_str());

	return 1;
}

static int LUA_clientGetPacketLoss(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getPacketLoss()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushnumber(L, client->getPacketLoss());

	return 1;
}

static int LUA_clientGetIP(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getIP()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushstring(L, client->getIP().c_str());

	return 1;
}

static int LUA_clientGetPing(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getPing()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushnumber(L,client->getPing());

	return 1;
}

static int LUA_clientGetID(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getID()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushinteger(L, client->getNetId());

	return 1;
}

static int LUA_clientIsAdmin(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:isAdmin()");
		return 0;
	}

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	lua_pushboolean(L, client->isAdmin);

	return 1;
}

static int LUA_clientGiveControl(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:giveControl(dynamic)");
		return 0;
	}

	std::shared_ptr<Dynamic> player = LUA_pd->dynamics->popLua(L);

	if (!player)
	{
		error("Invalid dynamic passed to client:giveControl");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:giveControl");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:giveControl");
		return 0;
	}

	//Give physics simulation control for this object to client
	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)TakeOverPhysics;
	ret->data[1] = (unsigned char)1; //Add
	netIDType netID = player->getID();
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	client->client->send(ret, OtherReliable);

	client->controlledObjects.push_back(player);

	return 0;
}

static int LUA_clientRemoveControl(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:removeControl(dynamic)");
		return 0;
	}

	std::shared_ptr<Dynamic> player = LUA_pd->dynamics->popLua(L);

	if (!player)
	{
		error("Invalid dynamic passed to client:removeControl");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:removeControl");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:removeControl");
		return 0;
	}

	//Turn old player back into a normal server-controlled object, if it existed
	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)TakeOverPhysics;
	ret->data[1] = (unsigned char)0; //Remove
	netIDType netID = player->getID();
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	client->client->send(ret, OtherReliable);

  	client->controlledObjects.erase(std::remove(client->controlledObjects.begin(), client->controlledObjects.end(), player), client->controlledObjects.end());

	return 0;
}

static int LUA_clientGetNumControlled(lua_State* L)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getNumControlled()");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:getNumControlled");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:getNumControlled");
		return 0;
	}

	lua_pushinteger(L, client->controlledObjects.size());

	return 1;
}

static int LUA_clientGetControlledIdx(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 1 argument client:getControlledIdx(index)");
		return 0;
	}

	int index = lua_tointeger(L,-1);
	lua_pop(L,1);

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:getControlledIdx");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:getControlledIdx");
		return 0;
	}

	if(index < 0 || index >= client->controlledObjects.size())
	{
		error("Index " + std::to_string(index) + " out of bounds for client:getControlledIdx");
		return 0;
	}
	
	LUA_pd->dynamics->pushLua(L, client->controlledObjects[index]);

	return 1;
}

static int LUA_clientBindCamera(lua_State* L)
{
	if(lua_gettop(L) != 4)
	{
		error("Expected 4 arguments client:bindCamera(dynamic,fixUpVector,maxFollowDistance)");
		return 0;
	}

	float maxFollowDistnace = lua_tonumber(L, -1);
	lua_pop(L, 1);

	bool fixUpVector = lua_toboolean(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<Dynamic> object = LUA_pd->dynamics->popLua(L);
	if (!object)
	{
		error("Invalid dynamic passed to client:bindCamera");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);
	if (!jc)
	{
		error("Invalid client object A passed to client:bindCamera");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);
	if (!client)
	{
		error("Invalid client object B passed to client:bindCamera");
		return 0;
	}

	//Send camera settings to client

	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(netIDType) + sizeof(float), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)CameraSettings;
	ret->data[1] = (unsigned char)CameraFlag_BoundToObject | (fixUpVector ? CameraFlag_LockUpVector : 0);
	netIDType netID = object->getID();
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	memcpy(ret->data + 2 + sizeof(netIDType), &maxFollowDistnace, sizeof(float));
	client->client->send(ret, OtherReliable);

	return 0;
}

static int LUA_clientStaticCamera(lua_State* L)
{
	if (lua_gettop(L) != 4 && lua_gettop(L) != 7)
	{
		error("Expected 4 or 7 arguments client:staticCamera(posX,posY,posZ) client:staticCamera(posX,posY,posZ,dirX,dirY,dirZ)");
		return 0;
	}

	int packetSize = 2 + sizeof(float) * 3;
	float pX=0,pY=0,pZ=0,dX=0,dY=0,dZ=0;

	if (lua_gettop(L) == 7)
	{
		packetSize += sizeof(float) * 3;

		dZ = lua_tonumber(L, -1);
		lua_pop(L, 1);
		dY = lua_tonumber(L, -1);
		lua_pop(L, 1);
		dX = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	pZ = lua_tonumber(L, -1);
	lua_pop(L, 1);
	pY = lua_tonumber(L, -1);
	lua_pop(L, 1);
	pX = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<JoinedClient> jc = popClientLua(L);
	if (!jc)
	{
		error("Invalid client object A passed to client:staticCamera");
		return 0;
	}

	ENetPacket* ret = enet_packet_create(NULL, packetSize, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)CameraSettings;
	
	if(packetSize == sizeof(float) * 3 + 2)
		ret->data[1] = (unsigned char)CameraFlag_LockPosition;
	else
		ret->data[1] = (unsigned char)(CameraFlag_LockPosition | CameraFlag_LockDirection);
	
	memcpy(ret->data + 2 + sizeof(float) * 0, &pX, sizeof(float));
	memcpy(ret->data + 2 + sizeof(float) * 1, &pY, sizeof(float));
	memcpy(ret->data + 2 + sizeof(float) * 2, &pZ, sizeof(float));

	if (packetSize == sizeof(float) * 6 + 2)
	{
		memcpy(ret->data + 2 + sizeof(float) * 3, &dX, sizeof(float));
		memcpy(ret->data + 2 + sizeof(float) * 4, &dY, sizeof(float));
		memcpy(ret->data + 2 + sizeof(float) * 5, &dZ, sizeof(float));
	}

	jc->send(ret, OtherReliable);

	return 0;
}

static int LUA_clientSetDefaultController(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:setDefaultController(dynamic)");
		return 0;
	}

	std::shared_ptr<Dynamic> player = LUA_pd->dynamics->popLua(L);

	if (!player)
	{
		error("Invalid dynamic passed to client:setDefaultController");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:setDefaultController");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:setDefaultController");
		return 0;
	} 

	client->controllers.emplace_back();
	client->controllers.back().target = player;

	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)MovementSettings;
	netIDType id = player->getID();
	memcpy(ret->data + 1, &id, sizeof(netIDType));
	jc->send(ret, OtherReliable);

	return 0;
}

static int LUA_clientGetCursorItem(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:getCursorItem(maxDistance)");
		return 0;
	}

	float maxDistance = lua_tonumber(L, -1);
	lua_pop(L, 1);

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object passed to client:getCursorItem");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object passed to client:getCursorItem");
		return 0;
	}

	if (client->controllers.size() == 0)
	{
		error("Client has no default controller set, call client:setDefaultController first");
		return 0;
	}

	//Only the first/default controller's camera state is tracked, matching how bindCamera/setDefaultController are used elsewhere
	glm::vec3 cameraPosition = client->controllers[0].lastCameraPosition;
	glm::vec3 cameraDirection = client->controllers[0].lastCameraDirection;

	btRigidBody* ignore = nullptr;
	if (client->controlledObjects.size() > 0)
		ignore = client->controlledObjects[0]->body;

	btVector3 start = btVector3(cameraPosition.x, cameraPosition.y, cameraPosition.z);
	btVector3 end = start + btVector3(cameraDirection.x, cameraDirection.y, cameraDirection.z) * maxDistance;

	btRigidBody* result = LUA_pd->physicsWorld->doRaycast(start, end, ignore);
	pushRaycastResult(L, result);

	return 1;
}

//durationMS is clamped to 60000 (60s) in JoinedClient::makeCenterPrintPacket
static int LUA_clientCenterPrint(lua_State* L)
{
	int args = lua_gettop(L);

	if (args != 2 && args != 3 && args != 6)
	{
		error("Expected client:centerPrint(text[,durationMS[,red,green,blue]])");
		return 0;
	}

	float red = 1.0f, green = 1.0f, blue = 1.0f;
	unsigned int durationMS = 3000;

	if (args == 6)
	{
		blue = lua_tonumber(L, -1);
		lua_pop(L, 1);
		green = lua_tonumber(L, -1);
		lua_pop(L, 1);
		red = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	if (args >= 3)
	{
		durationMS = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}

	const char* text = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!text)
	{
		error("Invalid text passed to client:centerPrint");
		return 0;
	}

	std::string message = std::string(text);

	std::shared_ptr<JoinedClient> client = popClientLua(L);

	if (!client)
	{
		error("Invalid client object passed to client:centerPrint");
		return 0;
	}

	client->sendCenterPrint(message, durationMS, red, green, blue);

	return 0;
}

//durationMS is clamped to 60000 (60s) in JoinedClient::makeCenterPrintPacket
static int LUA_centerPrintAll(lua_State* L)
{
	int args = lua_gettop(L);

	if (args != 1 && args != 2 && args != 5)
	{
		error("Expected centerPrintAll(text[,durationMS[,red,green,blue]])");
		return 0;
	}

	float red = 1.0f, green = 1.0f, blue = 1.0f;
	unsigned int durationMS = 3000;

	if (args == 5)
	{
		blue = lua_tonumber(L, -1);
		lua_pop(L, 1);
		green = lua_tonumber(L, -1);
		lua_pop(L, 1);
		red = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	if (args >= 2)
	{
		durationMS = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}

	const char* text = lua_tostring(L, -1);
	lua_pop(L, 1);

	if (!text)
	{
		error("Invalid text passed to centerPrintAll");
		return 0;
	}

	if (!LUA_server)
	{
		error("Server not set");
		return 0;
	}

	ENetPacket* packet = JoinedClient::makeCenterPrintPacket(std::string(text), durationMS, red, green, blue);
	LUA_server->broadcast(packet, OtherReliable);

	return 0;
}

void registerClientFunctions(lua_State* L)
{
	//Register client global functions:
	lua_register(L, "getNumClients", LUA_getNumClients);
	lua_register(L, "getClientIdx", LUA_getClientIdx);
	lua_register(L, "centerPrintAll", LUA_centerPrintAll);
	lua_register(L, "messageAll", LUA_messageAll);

	luaL_Reg regs[] = {
		{ "message", LUA_clientMessage},
		{ "kick", LUA_clientKick },
		{ "getName", LUA_clientGetName },
		{ "getIP", LUA_clientGetIP },
		{ "getID", LUA_clientGetID },
		{ "getPing", LUA_clientGetPing },
		{ "isAdmin", LUA_clientIsAdmin },
		{ "giveControl", LUA_clientGiveControl },
		{ "removeControl", LUA_clientRemoveControl },
		{ "getControlledIdx", LUA_clientGetControlledIdx },
		{ "getNumControlled", LUA_clientGetNumControlled },
		{ "bindCamera", LUA_clientBindCamera },
		{ "staticCamera", LUA_clientStaticCamera },
		{ "setDefaultController", LUA_clientSetDefaultController },
		{ "getPacketLoss", LUA_clientGetPacketLoss },
		{ "getCursorItem", LUA_clientGetCursorItem },
		{ "centerPrint", LUA_clientCenterPrint },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "metatable_client");
	luaL_setfuncs(L, regs, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "metatable_client");
}
