#pragma once

#include "../LandOfDran.h"

#include "../Networking/ObjHolder.h"
#include "../NetTypes/DynamicType.h"
#include "../SimObjects/Dynamic.h"
#include "../SimObjects/StaticObject.h"
#include "../LuaFunctions/EventManager.h"
#include "../Physics/PhysicsWorld.h"
#include "ClientData.h"

/*
	Struct holds state that may be needed to process packets from the client
*/
struct ServerProgramData
{
	//Allow people to run Lua commands remotely with a password
	bool useEvalPassword = false;

	//The password, only works if useEvalPassword is true
	std::string evalPassword = "changeme";

	//True for the embedded server behind "Start Server" (single player): the only way in is the host's own
	//client connecting to itself over loopback, so that connection can safely skip the eval password entirely -
	//useEvalPassword is about remote access, which doesn't apply to it. A real (dedicated) server leaves this
	//false, since it's reachable over the network and always needs the password. See LoopServer's constructor
	bool autoAdminForLoopback = false;

	//Should this client be trusted as admin without going through the eval password?
	bool isTrustedLocalAdmin(JoinedClient* client) const
	{
		return autoAdminForLoopback && client->isLoopback();
	}

	std::shared_ptr<PhysicsWorld>	physicsWorld = nullptr;
	
	lua_State * luaState = nullptr;
	EventManager * eventManager = nullptr;

	//Includes all of the types from each of the vectors below, used to send them all quickly when someone joins and needs types
	std::vector<std::shared_ptr<NetType>> allNetTypes;
	//Various specific kinds of types
	std::vector<std::shared_ptr<DynamicType>> dynamicTypes;

	//ObjHolders created and destroyed with ServerLoop class
	//All dynamic objects:
	ObjHolder<Dynamic>* dynamics = nullptr;
	ObjHolder<StaticObject> * statics = nullptr;

	//All clients:
	std::vector<std::shared_ptr<ClientData>> clients;

	//Basically JoinedClient is lower level and used by the server for networking, ClientData is passed to server-side packet functions
	//ClientData contains references to a JoinedClient but also anything else that client 'owns' like a player, a camera, bricks, etc.
	//Called in Server::run
	void makeClient(std::shared_ptr<JoinedClient> src) const
	{
		auto client = std::make_shared<ClientData>();
		client->me = client;
		client->client = src;
		src->userData = client.get();
		std::vector<std::shared_ptr<ClientData>>* c = const_cast<std::vector<std::shared_ptr<ClientData>>*>( & clients);
		c->push_back(client);
	}

	//O(1) access to client data in packet functions, can return nullptr
	std::shared_ptr<ClientData> getClient(std::shared_ptr<JoinedClient> source) const
	{
		if (!source->userData)
			return nullptr;

		ClientData* ret = (ClientData*)source->userData;
		if (!ret)
			return nullptr;

		return ret->me;
	}

	//Called in Server::run when client leaves
	void removeClient(std::shared_ptr<JoinedClient> src) const
	{
		if (!src->userData)
			return;

		std::vector<std::shared_ptr<ClientData>>* c = const_cast<std::vector<std::shared_ptr<ClientData>>*>(&clients);
		for (unsigned int a = 0; a < c->size(); a++)
		{
			if (c->at(a).get() == src->userData)
			{
				//These objects don't have to dissapear if Lua modders don't want them to
				//But we need to clarify all of these objects are *only* owned by Lua now
				c->at(a)->controlledObjects.clear();

				//Get rid of ClientData and JoinedClient structures themselves
				src->userData = nullptr;
				c->at(a)->me.reset();
				c->at(a)->client.reset();
				c->erase(c->begin() + a);
				return;
			}
		}
	}
};
