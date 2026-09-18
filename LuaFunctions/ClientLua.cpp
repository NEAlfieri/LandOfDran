#include "ClientLua.h"
#include "Dynamic.h" //pushRaycastResult
#include "SoundLua.h"
#include "BrickLua.h"
#include "VehicleLua.h"

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
	client->controllers.back().jetsAllowed = client->jetsEnabled;

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

	//Or the vehicle they're driving
	std::shared_ptr<Vehicle> driving = client->vehicle.lock();

	btVector3 hitPosition, hitNormal;
	btRigidBody* result = LUA_pd->physicsWorld->doRaycast(start, end, ignore, hitPosition, hitNormal, driving ? driving->body : nullptr);
	return pushRaycastHit(L, result, start, hitPosition, hitNormal);
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

/*
	client:setVignette(red, green, blue, alpha, strength, durationMS)
	A color drawn in from the edges of their screen, alpha being how opaque it is at the edges to start with, while the
	picture wobbles like it does under the water, strength being how hard: 0 for none, 1 about as much as being underwater,
	up to 10. Both fade to nothing as durationMS runs out, and a new one replaces the one showing. 0 ms clears it
*/
static int LUA_clientSetVignette(lua_State* L)
{
	scope("(LUA) client:setVignette");

	const std::string usage = "client:setVignette(red, green, blue, alpha, strength, durationMS)";
	if (lua_gettop(L) != 7)
	{
		error("Expected 6 arguments " + usage);
		lua_settop(L, 0);
		return 0;
	}

	float values[6];
	for (int a = 0; a < 6; a++)
	{
		if (!lua_isnumber(L, a + 2))
		{
			error("Argument " + std::to_string(a + 1) + " is not a number " + usage);
			lua_settop(L, 0);
			return 0;
		}
		values[a] = (float)lua_tonumber(L, a + 2);
	}
	lua_settop(L, 1);

	std::shared_ptr<JoinedClient> client = popClientLua(L);
	if (!client)
	{
		error("Invalid client object passed to client:setVignette");
		return 0;
	}

	float red = std::clamp(values[0], 0.0f, 1.0f);
	float green = std::clamp(values[1], 0.0f, 1.0f);
	float blue = std::clamp(values[2], 0.0f, 1.0f);
	float alpha = std::clamp(values[3], 0.0f, 10.0f);
	float strength = std::clamp(values[4], 0.0f, 10.0f);
	unsigned int durationMS = (unsigned int)std::clamp(values[5], 0.0f, 3600000.0f);

	client->sendVignette(red, green, blue, alpha, strength, durationMS);

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

//The ClientData of the client object at the bottom of the stack, emptying the stack. nullptr after logging an error if there isn't one
static std::shared_ptr<ClientData> popClientData(lua_State* L, int expectedArgs, const std::string& usage)
{
	if (lua_gettop(L) != expectedArgs)
	{
		error("Expected " + usage);
		lua_settop(L, 0);
		return nullptr;
	}

	lua_settop(L, 1);
	std::shared_ptr<JoinedClient> jc = popClientLua(L);
	lua_settop(L, 0);

	std::shared_ptr<ClientData> client = jc ? LUA_pd->getClient(jc) : nullptr;
	if (!client)
		error("Invalid client object passed to " + usage);

	return client;
}

static int LUA_clientSetVoiceMuted(lua_State* L)
{
	scope("(LUA) client:setVoiceMuted");

	bool muted = lua_toboolean(L, 2);
	std::shared_ptr<ClientData> client = popClientData(L, 2, "client:setVoiceMuted(muted)");
	if (!client)
		return 0;

	//The server drops their voice from now on. If they were talking, LoopServer::endQuietTalkers ends it once it stops arriving
	client->voiceMuted = muted;

	//So they stop sending and see that they're muted
	char data[3] = { (char)VoiceStatus, (char)VoiceStatusMuted, (char)(muted ? 1 : 0) };
	client->client->send(data, 3, OtherReliable);

	return 0;
}

static int LUA_clientIsVoiceMuted(lua_State* L)
{
	scope("(LUA) client:isVoiceMuted");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:isVoiceMuted()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->voiceMuted);
	return 1;
}

static int LUA_clientIsTalking(lua_State* L)
{
	scope("(LUA) client:isTalking");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:isTalking()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->talking);
	return 1;
}

static int LUA_clientSetJetsEnabled(lua_State* L)
{
	scope("(LUA) client:setJetsEnabled");

	bool enabled = lua_toboolean(L, 2);
	std::shared_ptr<ClientData> client = popClientData(L, 2, "client:setJetsEnabled(enabled)");
	if (!client)
		return 0;

	client->jetsEnabled = enabled;
	//Stops anyone jetting right away, LoopServer::updatePlayerAbilities takes their flames away
	for (PlayerController& controller : client->controllers)
		controller.jetsAllowed = enabled;
	client->sendAbilities();

	return 0;
}

static int LUA_clientGetJetsEnabled(lua_State* L)
{
	scope("(LUA) client:getJetsEnabled");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:getJetsEnabled()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->jetsEnabled);
	return 1;
}

static int LUA_clientSetFlashlightEnabled(lua_State* L)
{
	scope("(LUA) client:setFlashlightEnabled");

	bool enabled = lua_toboolean(L, 2);
	std::shared_ptr<ClientData> client = popClientData(L, 2, "client:setFlashlightEnabled(enabled)");
	if (!client)
		return 0;

	client->flashlightEnabled = enabled;
	if (!enabled)
		client->setFlashlight(LUA_pd, false, glm::vec3(1));
	client->sendAbilities();

	return 0;
}

static int LUA_clientGetFlashlightEnabled(lua_State* L)
{
	scope("(LUA) client:getFlashlightEnabled");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:getFlashlightEnabled()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->flashlightEnabled);
	return 1;
}

static int LUA_clientSetFreeCameraEnabled(lua_State* L)
{
	scope("(LUA) client:setFreeCameraEnabled");

	bool enabled = lua_toboolean(L, 2);
	std::shared_ptr<ClientData> client = popClientData(L, 2, "client:setFreeCameraEnabled(enabled)");
	if (!client)
		return 0;

	client->freeCameraEnabled = enabled;
	//Puts a camera that's already off flying back on their player, their game does the same when it hears the abilities
	if (!enabled)
		client->setFreeCamera(LUA_pd, false);
	client->sendAbilities();

	return 0;
}

static int LUA_clientGetFreeCameraEnabled(lua_State* L)
{
	scope("(LUA) client:getFreeCameraEnabled");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:getFreeCameraEnabled()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->freeCameraEnabled);
	return 1;
}

static int LUA_clientGetFreeCamera(lua_State* L)
{
	scope("(LUA) client:getFreeCamera");

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:getFreeCamera()");
	if (!client)
		return 0;

	lua_pushboolean(L, client->freeCamera);
	return 1;
}

void applyAppearance(Server const* server, ClientData& client, std::shared_ptr<Dynamic> dynamic, const PlayerAppearance* previous)
{
	client.appearanceTarget = dynamic;

	const Model* model = dynamic->getType()->getModel().get();
	const PlayerAppearance& appearance = client.appearance;

	auto paint = [&](const std::string& meshName, const glm::vec4& color)
	{
		int meshIdx = model->getMeshIdxIgnoringCase(meshName);
		if (meshIdx == -1)
			return;

		ENetPacket* packet = dynamic->setMeshColor(model->getMeshName(meshIdx), color);
		if (packet)
			server->broadcast(packet, OtherReliable);
	};

	//A color with no alpha shows the model's own look
	if (previous)
	{
		for (const auto& [meshName, color] : previous->colors)
		{
			bool stillPainted = std::any_of(appearance.colors.begin(), appearance.colors.end(),
				[&](const std::pair<std::string, glm::vec3>& part) { return lowercase(part.first) == lowercase(meshName); });
			if (!stillPainted)
				paint(meshName, glm::vec4(0));
		}
	}

	for (const auto& [meshName, color] : appearance.colors)
		paint(meshName, glm::vec4(color, 1.0f));

	//Nothing is sent for a part that has no decal and isn't getting one
	auto putDecal = [&](int meshIdx, const std::string& decalName)
	{
		if (meshIdx == -1 || (decalName.empty() && !dynamic->meshDecals.count(meshIdx)))
			return;

		ENetPacket* packet = dynamic->setMeshDecal(model->getMeshName(meshIdx), decalName);
		if (packet)
			server->broadcast(packet, OtherReliable);
	};

	putDecal(model->getFaceMeshIdx(), appearance.face);
	putDecal(model->getShirtMeshIdx(), appearance.shirt);
}

static int LUA_clientApplyAppearance(lua_State* L)
{
	scope("(LUA) client:applyAppearance");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:applyAppearance(dynamic)");
		return 0;
	}

	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);

	if (!dynamic)
	{
		error("Invalid dynamic passed to client:applyAppearance");
		return 0;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);

	if (!jc)
	{
		error("Invalid client object A passed to client:applyAppearance");
		return 0;
	}

	std::shared_ptr<ClientData> client = LUA_pd->getClient(jc);

	if (!client)
	{
		error("Invalid client object B passed to client:applyAppearance");
		return 0;
	}

	applyAppearance(LUA_server, *client, dynamic);

	return 0;
}

static int LUA_clientOpenWrenchDialog(lua_State* L)
{
	scope("(LUA) client:openWrenchDialog");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:openWrenchDialog(brick) or client:openWrenchDialog(vehicle)");
		lua_settop(L, 0);
		return 0;
	}

	//A vehicle's dialog just has its music
	lua_getfield(L, 2, "type");
	bool isVehicle = lua_isinteger(L, -1) && lua_tointeger(L, -1) == VehicleTypeId;
	lua_pop(L, 1);
	if (isVehicle)
	{
		std::shared_ptr<Vehicle> vehicle = LUA_pd->vehicles->popLua(L);
		if (!vehicle)
		{
			lua_settop(L, 0);
			return 0;
		}

		std::shared_ptr<ClientData> client = popClientData(L, 1, "client:openWrenchDialog(vehicle)");
		if (client)
			openVehicleWrenchDialog(*client, *vehicle);
		return 0;
	}

	//Pops the brick off the top, leaving the client
	Brick* brick = LUA_pd->bricks->popLua(L);
	if (!brick)
	{
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:openWrenchDialog(brick)");
	if (!client)
		return 0;

	openWrenchDialog(*client, brick);
	return 0;
}

static int LUA_clientOpenPrintMenu(lua_State* L)
{
	scope("(LUA) client:openPrintMenu");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:openPrintMenu(brick)");
		lua_settop(L, 0);
		return 0;
	}

	//Pops the brick off the top, leaving the client
	Brick* brick = LUA_pd->bricks->popLua(L);
	if (!brick)
	{
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<ClientData> client = popClientData(L, 1, "client:openPrintMenu(brick)");
	if (!client)
		return 0;

	openPrintMenu(*client, brick);
	return 0;
}

//Shared by getPaintColor and getPaintMaterial, logs an error with usage for anything that isn't a client
static std::shared_ptr<ClientData> popPaintingClient(lua_State* L, const std::string& usage)
{
	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument " + usage);
		lua_settop(L, 0);
		return nullptr;
	}

	std::shared_ptr<JoinedClient> jc = popClientLua(L);
	std::shared_ptr<ClientData> client = jc ? LUA_pd->getClient(jc) : nullptr;
	if (!client)
		error("Invalid client passed to " + usage);
	return client;
}

static int LUA_clientGetPaintColor(lua_State* L)
{
	scope("(LUA) client:getPaintColor");

	std::shared_ptr<ClientData> client = popPaintingClient(L, "client:getPaintColor()");
	if (!client)
		return 0;

	for (int channel = 0; channel < 4; channel++)
		lua_pushnumber(L, client->paintColor[channel] / 255.0);
	return 4;
}

static int LUA_clientGetPaintMaterial(lua_State* L)
{
	scope("(LUA) client:getPaintMaterial");

	std::shared_ptr<ClientData> client = popPaintingClient(L, "client:getPaintMaterial()");
	if (!client)
		return 0;

	lua_pushstring(L, brickMaterialNames[client->paintMaterial]);
	return 1;
}

/*
	1 byte - packet type
	1 byte - command length, then the command, lowercase without the slash
	1 byte - suggestion text length, then the text
*/
static ENetPacket* makeChatSuggestionPacket(const ServerProgramData::ChatSuggestion& suggestion)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(ChatSuggestion);
	bytes.push_back((unsigned char)suggestion.command.length());
	bytes.insert(bytes.end(), suggestion.command.begin(), suggestion.command.end());
	bytes.push_back((unsigned char)suggestion.text.length());
	bytes.insert(bytes.end(), suggestion.text.begin(), suggestion.text.end());
	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation));
}

void sendChatSuggestions(const ServerProgramData* pd, JoinedClient* client)
{
	for (const ServerProgramData::ChatSuggestion& suggestion : pd->chatSuggestions)
		client->send(makeChatSuggestionPacket(suggestion), JoinNegotiation);
}

//The command is what a client types after the slash, the text is what their chat window lists for it, the full name with its arguments
//Registering a command again replaces its text
static int LUA_registerChatSuggestion(lua_State* L)
{
	scope("(LUA) registerChatSuggestion");

	int args = lua_gettop(L);
	if (args != 2 || !lua_isstring(L, 1) || !lua_isstring(L, 2))
	{
		error("Expected registerChatSuggestion(commandName, suggestionText)");
		lua_settop(L, 0);
		return 0;
	}

	ServerProgramData::ChatSuggestion suggestion;
	suggestion.command = lua_tostring(L, 1);
	suggestion.text = lua_tostring(L, 2);
	lua_settop(L, 0);

	//Written with or without the slash, matched without it
	if (!suggestion.command.empty() && suggestion.command[0] == '/')
		suggestion.command = suggestion.command.substr(1);

	//Commands are case-insensitive, see chatMessageSent
	for (char& c : suggestion.command)
		c = (char)tolower((unsigned char)c);

	if (suggestion.command.empty())
	{
		error("Command name can't be empty");
		return 0;
	}

	if (suggestion.command.find_first_of(" \t\r\n") != std::string::npos)
	{
		error("Command name " + suggestion.command + " can't contain spaces, a command ends at the first space");
		return 0;
	}

	//Leaves room for the slash and a space in the client's 256 byte message bar
	if (suggestion.command.length() > 64)
	{
		error("Command name " + suggestion.command + " is over 64 characters");
		return 0;
	}

	if (suggestion.text.empty())
		suggestion.text = "/" + suggestion.command;

	if (suggestion.text.length() > 255)
	{
		error("Suggestion text for " + suggestion.command + " is over 255 characters, cutting it short");
		suggestion.text = suggestion.text.substr(0, 255);
	}

	bool replaced = false;
	for (ServerProgramData::ChatSuggestion& existing : LUA_pd->chatSuggestions)
	{
		if (existing.command == suggestion.command)
		{
			existing.text = suggestion.text;
			replaced = true;
			break;
		}
	}

	if (!replaced)
		LUA_pd->chatSuggestions.push_back(suggestion);

	//Anyone already here, anyone who joins later gets it in sendChatSuggestions
	LUA_server->broadcast(makeChatSuggestionPacket(suggestion), JoinNegotiation);

	return 0;
}

void registerClientFunctions(lua_State* L)
{
	//Register client global functions:
	lua_register(L, "getNumClients", LUA_getNumClients);
	lua_register(L, "getClientIdx", LUA_getClientIdx);
	lua_register(L, "centerPrintAll", LUA_centerPrintAll);
	lua_register(L, "messageAll", LUA_messageAll);
	lua_register(L, "registerChatSuggestion", LUA_registerChatSuggestion);

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
		{ "playSound", LUA_clientPlaySound },
		{ "setVignette", LUA_clientSetVignette },
		{ "setAudioEffect", LUA_clientSetAudioEffect },
		{ "setVoiceMuted", LUA_clientSetVoiceMuted },
		{ "isVoiceMuted", LUA_clientIsVoiceMuted },
		{ "isTalking", LUA_clientIsTalking },
		{ "setJetsEnabled", LUA_clientSetJetsEnabled },
		{ "getJetsEnabled", LUA_clientGetJetsEnabled },
		{ "setFlashlightEnabled", LUA_clientSetFlashlightEnabled },
		{ "getFlashlightEnabled", LUA_clientGetFlashlightEnabled },
		{ "setFreeCameraEnabled", LUA_clientSetFreeCameraEnabled },
		{ "getFreeCameraEnabled", LUA_clientGetFreeCameraEnabled },
		{ "getFreeCamera", LUA_clientGetFreeCamera },
		{ "applyAppearance", LUA_clientApplyAppearance },
		{ "openWrenchDialog", LUA_clientOpenWrenchDialog },
		{ "openPrintMenu", LUA_clientOpenPrintMenu },
		{ "getPaintColor", LUA_clientGetPaintColor },
		{ "getPaintMaterial", LUA_clientGetPaintMaterial },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "metatable_client");
	luaL_setfuncs(L, regs, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "metatable_client");
}
