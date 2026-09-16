#pragma once

#include "../NetTypes/NetType.h"
#include "Server.h"
#include <glm/gtx/norm.hpp> //glm::distance2

extern "C" 
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

//Basically we wanna make sure packets are under the MTU if possible,
//but I'm not positive how much overhead ENet adds to packets
#define MTU ENET_HOST_DEFAULT_MTU - 20

/*
	How often each client hears about an object depends on how far away from them it is, and how much they've already
	been sent this tick. Set once at startup from the hosting/ settings, read in the LoopServer constructor

	The camera's far plane is 1000 studs, so the bands are sized around that: everything you can actually see is in the
	near or mid band, the far band keeps things roughly right just past the horizon so they're not stale when they come
	into view, and past that a client is sent nothing at all about it
*/
struct NetRelevanceSettings
{
	//Objects closer than this to a client get every update the server generates, which is one per 25ms
	static float nearDistance;
	//Out to here they get every other one, and out to farDistance every fourth. Past farDistance, nothing
	static float midDistance;
	static float farDistance;

	//Most bytes of object updates one client can be sent per tick, across every type of object
	static int bytesPerTick;

	//How much of that budget a client with a bad connection gets, rather than dropping them entirely as we used to
	static float highPingFactor;
	static float veryHighPingFactor;

	static float nearDistance2() { return nearDistance * nearDistance; }
	static float midDistance2() { return midDistance * midDistance; }
	static float farDistance2() { return farDistance * farDistance; }
};

//I wanted this to be a .tpp file since it's just templated code, but I guess MSVC doesn't like those

/*
	ObjHolder is a factory for SimObject instances
	It makes sure vector membership is synonymous with existance
	It can batch creation/update/removal packets on the server for more efficient networking
	It can manage Lua state
*/
template <typename T>
class ObjHolder
{
	/*
		Each time a SimObject is created, this will be assigned to it and increment by one
		Not static: the client and server each keep their own independent ID sequence per type,
		and in single player both may live in the same process at once.
	*/
	netIDType lastNetID = 0;

	/*
		What type of objects does this hold
		There should be one ObjHolder per type of object
	*/
	SimObjectType type = SimObjectType::InvalidSimTypeId;

	/*
		This should be every instance of this type of object that currently exists in the program
	*/
	std::vector<std::shared_ptr<T>> allObjects;

	/*
		When an object gets deleted, its ID is added here
		Every frame or few frames one big packet is sent out to clients with all recent deletions
	*/
	std::vector<netIDType> recentlyDeletedIDs;

	/*
		When an object gets created, its pointer is added here
		Every frame or few frames one big packet is sent out to clients with all recent creations
	*/
	std::vector<std::shared_ptr<T>> recentCreations;

	//Non-owning: LoopServer owns the one Server instance and deletes it itself.
	//Will be nullptr if this is the client
	Server* server = nullptr;

	/*
		Server: one tick's worth of object updates, serialized once each and then copied to whichever clients are
		close enough to want them. addToUpdatePacket consumes the state it writes - it clears gravityUpdated, moves
		lastSentTransform up, rolls the position keyframe - so it can only be called once an object per tick, which
		is why the bytes are staged here instead of written straight into a per-client packet
	*/
	struct PendingObjectUpdate
	{
		netIDType id = NO_ID;
		//Where in updateScratch this object's bytes start, and how many there are
		unsigned int offset = 0;
		unsigned int bytes = 0;
		//Where the object is, and whether it has a position worth throttling by at all
		glm::vec3 position = glm::vec3(0, 0, 0);
		bool cullable = false;
		//Whether a throttled client has to be sent this one anyway, see SimObject::lastUpdateAnchorsLaterOnes
		bool anchor = false;
		//Only valid for the one sendRecent call that staged it, to rewrite the playback interval per band
		T* object = nullptr;
	};

	std::vector<enet_uint8> updateScratch;
	std::vector<PendingObjectUpdate> pendingUpdates;

	//Indices into pendingUpdates, rebuilt per client, see sendObjectUpdates
	std::vector<unsigned int> nearBand;
	std::vector<unsigned int> midBand;
	std::vector<unsigned int> farBand;

	//Counts sendRecent calls, to stagger which ticks the throttled bands go out on and which objects win a tight budget
	unsigned int updateTick = 0;

	std::string metatableName = "";

	public:

	/*
		Creates a lua global variable with name provided that is a table with functions provided 
		Assigned to all Lua objects created by this ObjHolder's pushLua function
	*/
	void makeLuaMetatable(lua_State* const L,const std::string& name, luaL_Reg *functions)
	{
		metatableName = name;

		luaL_newmetatable(L, metatableName.c_str());
		luaL_setfuncs(L, functions, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -1, "__index");
		lua_setglobal(L, metatableName.c_str());

		delete[] functions;
	}

	/*
		Lua representations of SimObjects are tables with:
		- type: integer, the SimObjectType of the object
		- ptr: lightuserdata, a pointer to a weak_ptr to the object
		- id: integer, the netID of the object
		A metatable that handles function calls and comparison behavior
	*/
	std::shared_ptr<T> popLua(lua_State* const L) const
	{
		scope("popLua");

		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			error("No table for supposed SimObject");
			return nullptr;
		}

		lua_getfield(L, -1, "type");
		if(!lua_isinteger(L, -1))
		{
			lua_pop(L, 2);
			error("No type field for supposed SimObject");
			return nullptr;
		}

		SimObjectType poppedType = (SimObjectType)lua_tointeger(L, -1);
		lua_pop(L, 1);

		if (poppedType != type)
		{
			lua_pop(L, 1);
			error("SimObject type mismatch, invalid Lua cast");
			return nullptr;
		}

		lua_getfield(L, -1, "ptr");
		if(!lua_isuserdata(L, -1))
		{
			lua_pop(L, 2);
			error("No ptr field for supposed SimObject");
			return nullptr;
		}

		std::weak_ptr<T> *obj = (std::weak_ptr<T>*)lua_touserdata(L, -1);
		lua_pop(L, 1);

		if (!obj)
		{
			lua_pop(L, 1);
			error("Lua userdata for SimObject weak_ptr not set");
			return nullptr;
		}

		if(obj->expired())
		{
			lua_getfield(L, -1, "id");
			if (lua_isinteger(L, -1))
				error("SimObject with ID " + std::to_string(lua_tointeger(L, -1)) + " was deleted");
			else
				error("SimObject was deleted, could not get ID");

			lua_pop(L, 2);
			return nullptr;
		}

		lua_pop(L, 1);
		return obj->lock();
	}

	//See note for its companion function: popLua
	void pushLua(lua_State* const L,std::shared_ptr<T> obj)
	{
		if (metatableName == "")
		{
			error("No metatable set for ObjHolder");
			return;
		}

		//Set class metatable, or the object's own, like an item's, which has every dynamic function and more
		const char* objectMetatable = obj->getLuaMetatable();
		lua_newtable(L);
		lua_getglobal(L, objectMetatable ? objectMetatable : metatableName.c_str());
		lua_setmetatable(L, -2);

		//Push class-unique server ID
		lua_pushinteger(L, obj->netID);
		lua_setfield(L, -2, "id");

		//Allow unambigious type checking
		lua_pushinteger(L, type);
		lua_setfield(L, -2, "type");

		//Lua will automatically deallocate the space for the weak_ptr itself when the table is garbage collected
		std::weak_ptr<T> *userdata = (std::weak_ptr<T>*)lua_newuserdata(L, sizeof(std::weak_ptr<T>));
		new(userdata) std::weak_ptr<T>(obj);
		lua_setfield(L, -2, "ptr");
	}

	/*
		Templated factory method for creating an object
		Sets ID and creation time, queues networking updates
	*/
	template <typename... Args>
	std::shared_ptr<T> create(Args... args)
	{
		std::shared_ptr<T> tmp(new T(args...));
		adopt(tmp);
		return tmp;
	}

	//Same as create, for a class derived from the one this holds, like an Item in the dynamics holder
	template <typename U, typename... Args>
	std::shared_ptr<U> createDerived(Args... args)
	{
		std::shared_ptr<U> tmp(new U(args...));
		adopt(tmp);
		return tmp;
	}

	private:

	//The rest of create, for an object that was just constructed
	void adopt(const std::shared_ptr<T>& object)
	{
		object->me = object;
		allObjects.push_back(object);
		object->netID = lastNetID++;
		object->creationTime = SDL_GetTicks();
		object->onCreation();
		if(server)
			recentCreations.push_back(object);
	}

	public:

	//Force the ID of the next created object to be a certain ID
	//This is used because the client gets object ids from the server
	inline void clientSetNextId(const netIDType& netID)
	{
		if (server)
		{
			error("clientSetNextId should only be called from the client!");
			return;
		}

		lastNetID = netID;
	}

	//Returns nullptr if object not found by that ID
	const inline std::shared_ptr<T> find(const netIDType& netID) const
	{
		if (allObjects.size() == 0)
			return nullptr;

		auto hasNetID = [&netID](const std::shared_ptr<T>& query) { return query->netID == netID; };
		auto it = std::find_if(allObjects.begin(), allObjects.end(), hasNetID);
		return it != allObjects.end() ? *it : nullptr;
	}

	//How many objects of this type exist in the entire program
	const inline size_t size() const { return allObjects.size(); }

	//Used to delete an object you found as the result of ex. a collision callback where you just have the raw pointer
	inline void destroyByPointer(T* target)
	{
		auto hasPointer = [&target](const std::shared_ptr<T>& query) { return query.get() == target; };
		auto it = std::find_if(allObjects.begin(), allObjects.end(), hasPointer);
		if (it != allObjects.end())
		{
			recentlyDeletedIDs.push_back((*it)->netID);
			(*it)->requestDestruction();
			(*it)->me.reset();
			(*it).reset();
			allObjects.erase(it);
		}
		else
		{
			scope("objHolder::destroy");
			error("Pointer not found in objHolder");
		}
	}

	//Used to delete an object using ID given to us by the server, principally from DeleteSimObjects packet
	inline void destroyByID(netIDType id)
	{
		auto hasID = [&id](const std::shared_ptr<T>& query) { return query->getID() == id; };
		auto it = std::find_if(allObjects.begin(), allObjects.end(), hasID);
		if (it != allObjects.end())
		{
			recentlyDeletedIDs.push_back((*it)->netID);
			(*it)->requestDestruction();
			(*it)->me.reset();
			(*it).reset();
			allObjects.erase(it);
		}
		else
		{
			scope("objHolder::destroy");
			error("Pointer not found in objHolder");
		}
	}

	//Exchange netID (i.e. from incoming client packet, lua callback) for vector index
	//Honestly no clue when this would be needed
	const inline size_t findIdx(const netIDType& netID) const
	{
		auto hasNetID = [&netID](const std::shared_ptr<T>& query) { return query->netID == netID; };
		auto it = std::find_if(allObjects.begin(), allObjects.end(), hasNetID);
		return it - allObjects.begin();
	}

	/*
		Attempt to canonically destroy the object, could still exist in memory with shared_ptrs
		Lua objects hold weak_ptrs and will not prohibit deallocation
	*/
	inline void destroy(std::shared_ptr<T>& idx)
	{
		auto it = std::find(allObjects.begin(), allObjects.end(), idx);
		if (it != allObjects.end())
		{
			recentlyDeletedIDs.push_back((*it)->netID);
			(*it)->requestDestruction();
			(*it)->me.reset();
			(*it).reset();
			allObjects.erase(it);
			idx.reset();
		}
		else
		{
			scope("objHolder::destroy");
			error("Pointer not found in objHolder");
		}
	}

	/*
		Canonically destroy every object this holder has, e.g. when the client leaves a server
		Each object's physics body holds a shared_ptr to the object itself, so simply dropping allObjects
		(which is all the empty destructor does) leaks every object along with its ModelInstance
		Must be called while SimObject::world still refers to the world these objects' bodies are in
	*/
	inline void destroyAll()
	{
		for (auto& obj : allObjects)
		{
			obj->requestDestruction();
			obj->me.reset();
			obj.reset();
		}
		allObjects.clear();
		recentCreations.clear();
		recentlyDeletedIDs.clear();
	}

	//Principle way of getting objects, outside of lua scripts and client packets that use netIDs
	inline std::shared_ptr<T> operator[](const std::size_t& idx) const
	{
		//This will mostly be used in loops so I'm not too worried about out of bounds indexing 
		/*scope("objHolder[]");
		if (idx >= allObjects.size())
		{
			error("SimObject index out of range.");
			return noObj;
		}
		else*/
			return allObjects[idx];
	}

	//Array operator sometimes doesn't compile
	inline std::shared_ptr<T> get(const std::size_t& idx) const
	{
		return allObjects[idx];
	}

	//Call on client when they confirm phase 1 loading is complete
	void sendAll(JoinedClient const *client)
	{
		/*
			Whatever this client is about to be told about these objects is all it will have, and it has no keyframes
			to measure position deltas against, so the next update for each of them goes out whole. It costs everyone
			one full update, which is cheaper than working out per client what each of them is missing
		*/
		for (unsigned int a = 0; a < allObjects.size(); a++)
			allObjects[a]->requireFullNetUpdate();

		int toSend = allObjects.size();
		int sent = 0;

		//Send as many packets as needed to send all objects to the client
		//without any one packet exceeding the MTU
		while (toSend > 0)
		{
			int sentThisPacket = 0;
			int bytesThisPacket = 0;

			//I guess if one object was somehow larger than like 1300 bytes this would stall lol
			for (unsigned int a = sent; a < allObjects.size(); a++)
			{
				//Only using one byte to encode amount of objects in the packet
				if (sentThisPacket >= 255)
					break;

				sentThisPacket++;
				bytesThisPacket += allObjects[a]->getCreationPacketBytes();

				if (bytesThisPacket > MTU)
				{
					sentThisPacket--;
					bytesThisPacket -= allObjects[a]->getCreationPacketBytes();
					break;
				}
			}

			//Three extra bytes, packet type, simobject type, amount of objects
			ENetPacket* packet = enet_packet_create(NULL, bytesThisPacket + 3, getFlagsFromChannel(OtherReliable));
			packet->data[0] = FromServerPacketType::AddSimObjects;
			packet->data[1] = type;
			packet->data[2] = sentThisPacket;
			int byteIterator = 3;

			for (int a = sent; a < sent + sentThisPacket; a++)
			{
				allObjects[a]->addToCreationPacket(packet->data + byteIterator);
				byteIterator += allObjects[a]->getCreationPacketBytes();
			}

			client->send(packet, OtherReliable);
			toSend -= sentThisPacket;
			sent += sentThisPacket;
		}
	}

	//How many bytes we'd need to represent an ID, given the ID before it in a sequence
	unsigned int bytesForIdDelta(netIDType last, netIDType next)
	{
		if (last == NO_ID)
			return 4;
		if (next - last < 128)
			return 1;
		else if (next - last < 16384)
			return 2;
		else
			return 4;
	}

	//Returns how many bytes it added to the packet, same as bytesForIdDelta
	unsigned int addIdDelta(netIDType last, netIDType next,enet_uint8 *data)
	{
		unsigned int delta = next - last;
		switch (bytesForIdDelta(last, next))
		{
			case 1:					//First bit zero 
				data[0] = delta;
				return 1;
			case 2:					//First bit set, next bit zero
				data[0] = 128 + ((delta) >> 8);
				data[1] = (delta) & 255;
				return 2; 
			case 4:					//First two bits set
				data[0] = 192 + (next >> 24);
				data[1] = (next >> 16) & 255;
				data[2] = (next >> 8) & 255;
				data[3] = next & 255;
				return 4;
		}
		error("addIdDelta failed");
		return 1;
	}

	//Increments byteIterator by bytesForIdDelta and returns ID
	netIDType getIdFromDelta(enet_uint8* data,netIDType lastId, unsigned int& byteIterator)
	{
		if (data[0] & 128)
		{
			if (data[0] & 64) //First two bits set, full 30 bit ID
			{
				byteIterator += 4;
				return ((data[0] & 63) << 24) + (data[1] << 16) + (data[2] << 8) + data[3];
			}
			else //First bit set, next bit zero, 14 bit delta
			{
				if (lastId == NO_ID)
					error("Expected full ID got ID delta instead.");
				byteIterator += 2;
				return ((data[0] & 63) << 8) + data[1] + lastId;
			}
		}
		else //First bit zero, 7 bit delta
		{
			if (lastId == NO_ID)
				error("Expected full ID got ID delta instead.");
			byteIterator++;
			return (data[0] & 127) + lastId;
		}
	}

	//Sends all recent creations, deletions, and updates to objects it manages to all connected clients
	void sendRecent()
	{
		if (server->getNumClients() == 0)
		{
			recentCreations.clear();
			recentlyDeletedIDs.clear();
			return;
		}

		static unsigned int lastFunctionCallMS = getTicksMS();

		if (getTicksMS() - lastFunctionCallMS < 25)
			return;
		lastFunctionCallMS = getTicksMS();


		//Send recently created objects to all connected clients:

		int toSend = recentCreations.size();
		int sent = 0;

		//Send as many packets as needed to send all objects to the client
		//without any one packet exceeding the MTU
		while (toSend > 0)
		{
			int sentThisPacket = 0;
			int bytesThisPacket = 0;

			//I guess if one object was somehow larger than like 1300 bytes this would stall lol
			for (unsigned int a = sent; a < recentCreations.size(); a++)
			{
				//Only using one byte to encode amount of objects in the packet
				if (sentThisPacket >= 255)
					break;

				sentThisPacket++;
				bytesThisPacket += recentCreations[a]->getCreationPacketBytes();

				if (bytesThisPacket > MTU)
				{
					sentThisPacket--;
					bytesThisPacket -= recentCreations[a]->getCreationPacketBytes();
					break;
				}
			}

			if (sentThisPacket == 0)
				break;

			//Three extra bytes, packet type, simobject type, amount of objects
			ENetPacket* packet = enet_packet_create(NULL, bytesThisPacket + 3, getFlagsFromChannel(OtherReliable));
			packet->data[0] = FromServerPacketType::AddSimObjects;
			packet->data[1] = type;
			packet->data[2] = sentThisPacket;
			int byteIterator = 3;

			for (int a = sent; a < sent + sentThisPacket; a++)
			{
				recentCreations[a]->addToCreationPacket(packet->data + byteIterator);
				byteIterator += recentCreations[a]->getCreationPacketBytes();
			}

			server->broadcast(packet, OtherReliable);
			toSend -= sentThisPacket;
			sent += sentThisPacket;
		}

		recentCreations.clear();

		//Send recently deleted objects(' IDs) to all connected clients:

		toSend = recentlyDeletedIDs.size();
		sent = 0;

		//Send as many packets as needed to send all objects to the client
		//without any one packet exceeding the MTU
		while (toSend > 0)
		{
			int sentThisPacket = 0;
			int bytesThisPacket = 0;

			for (unsigned int a = sent; a < recentlyDeletedIDs.size(); a++)
			{
				//Only using one byte to encode amount of objects in the packet
				if (sentThisPacket >= 255)
					break;

				sentThisPacket++;
				bytesThisPacket += sizeof(netIDType);

				if (bytesThisPacket > MTU)
				{
					sentThisPacket--;
					bytesThisPacket -= sizeof(netIDType);
					break;
				}
			} 

			if (sentThisPacket == 0)
				break;

			//Three extra bytes, packet type, simobject type, amount of objects
			ENetPacket* packet = enet_packet_create(NULL, bytesThisPacket + 3, getFlagsFromChannel(OtherReliable));
			packet->data[0] = FromServerPacketType::DeleteSimObjects;
			packet->data[1] = type;
			packet->data[2] = sentThisPacket;
			int byteIterator = 3;

			for (int a = sent; a < sent + sentThisPacket; a++)
			{
				memcpy(packet->data + byteIterator, &recentlyDeletedIDs[a], sizeof(netIDType));
				byteIterator += sizeof(netIDType);
			}

			server->broadcast(packet, OtherReliable);
			toSend -= sentThisPacket;
			sent += sentThisPacket;
		}

		recentlyDeletedIDs.clear();

		//Send any pending object updates out, see sendObjectUpdates
		serializePendingUpdates();

		for (unsigned int a = 0; a < server->getNumClients(); a++)
			sendObjectUpdates(server->getClientByIndex(a).get());

		updateTick++;
	}

	/*
		Asks every object whether it needs an update and, for the ones that do, writes its update once into
		updateScratch. Nothing is sent here: who gets which of these is sendObjectUpdates' job
	*/
	void serializePendingUpdates()
	{
		updateScratch.clear();
		pendingUpdates.clear();

		for (unsigned int a = 0; a < allObjects.size(); a++)
		{
			if (!allObjects[a]->requiresNetUpdate())
				continue;

			allObjects[a]->flaggedForUpdate = false;

			PendingObjectUpdate pending;
			pending.id = allObjects[a]->getID();
			pending.bytes = allObjects[a]->getUpdatePacketBytes();
			pending.offset = (unsigned int)updateScratch.size();
			pending.cullable = allObjects[a]->getNetRelevancePosition(pending.position);

			//One object bigger than a whole packet would never fit and would stall the send loop below
			if (pending.bytes + 3 + sizeof(netIDType) > MTU)
			{
				error("Object update too large to send, skipping");
				continue;
			}

			updateScratch.resize(pending.offset + pending.bytes);
			allObjects[a]->addToUpdatePacket(updateScratch.data() + pending.offset);
			pending.anchor = allObjects[a]->lastUpdateAnchorsLaterOnes();
			pending.object = allObjects[a].get();

			pendingUpdates.push_back(pending);
		}
	}

	/*
		Sorts this tick's updates into distance bands for one client and sends them the ones they're due, nearest
		band first, stopping when they run out of update budget for the tick
	*/
	void sendObjectUpdates(JoinedClient* client)
	{
		if (!client || pendingUpdates.empty())
			return;

		int budget = client->updateByteBudget;
		if (budget <= 0)
			return;

		nearBand.clear();
		midBand.clear();
		farBand.clear();

		for (unsigned int a = 0; a < pendingUpdates.size(); a++)
		{
			const PendingObjectUpdate& pending = pendingUpdates[a];

			//Anything without a position of its own, and everything at all for a client we can't place, goes out in full
			if (!pending.cullable || !client->hasRelevancePosition)
			{
				nearBand.push_back(a);
				continue;
			}

			const float distance2 = glm::distance2(pending.position, client->relevancePosition);

			if (distance2 <= NetRelevanceSettings::nearDistance2())
			{
				nearBand.push_back(a);
			}
			else if (distance2 <= NetRelevanceSettings::midDistance2())
			{
				//Half rate and quarter rate, spread across ticks by object ID so they don't all land on the same one
				if (pending.anchor || ((pending.id + updateTick) & 1) == 0)
					midBand.push_back(a);
			}
			else if (distance2 <= NetRelevanceSettings::farDistance2())
			{
				if (pending.anchor || ((pending.id + updateTick) & 3) == 0)
					farBand.push_back(a);
			}
			//Past farDistance this client hears nothing about it at all
		}

		//The multiplier is how much slower than the server writes them this client actually receives them
		sendUpdateBand(client, nearBand, budget, 1);
		sendUpdateBand(client, midBand, budget, 2);
		sendUpdateBand(client, farBand, budget, 4);

		client->updateByteBudget = budget;
	}

	/*
		Packs one band's updates into MTU sized packets for one client, spending from budget as it goes
		Starts at a rotating offset into the band so that a budget too small for all of it doesn't starve the same
		objects every tick
	*/
	void sendUpdateBand(JoinedClient* client, const std::vector<unsigned int>& band, int& budget, unsigned int multiplier)
	{
		if (band.empty() || budget <= 0)
			return;

		const unsigned int startAt = updateTick % band.size();
		unsigned int sent = 0;

		while (sent < band.size() && budget > 0)
		{
			//Work out what fits in this packet before creating it, since ENet wants the size up front
			unsigned int inThisPacket = 0;
			unsigned int bytesThisPacket = 0;
			netIDType lastId = NO_ID;

			for (unsigned int a = sent; a < band.size(); a++)
			{
				const PendingObjectUpdate& pending = pendingUpdates[band[(startAt + a) % band.size()]];
				const unsigned int cost = pending.bytes + bytesForIdDelta(lastId, pending.id);

				if (bytesThisPacket + cost > MTU || (int)(bytesThisPacket + cost) > budget)
					break;

				//Only using one byte to encode amount of objects in the packet
				if (inThisPacket >= 255)
					break;

				bytesThisPacket += cost;
				inThisPacket++;
				lastId = pending.id;
			}

			if (inThisPacket == 0)
				break;

			//Three extra bytes, packet type, simobject type, amount of objects
			ENetPacket* packet = enet_packet_create(NULL, bytesThisPacket + 3, getFlagsFromChannel(ObjectUpdates));
			packet->data[0] = FromServerPacketType::UpdateSimObjects;
			packet->data[1] = type;
			packet->data[2] = inThisPacket;

			unsigned int byteIterator = 3;
			lastId = NO_ID; //Reset for the actual packet

			for (unsigned int a = sent; a < sent + inThisPacket; a++)
			{
				const PendingObjectUpdate& pending = pendingUpdates[band[(startAt + a) % band.size()]];

				byteIterator += addIdDelta(lastId, pending.id, packet->data + byteIterator);
				lastId = pending.id;

				memcpy(packet->data + byteIterator, updateScratch.data() + pending.offset, pending.bytes);

				//Staged before we knew this client would be sent only every second or fourth of them
				if (multiplier > 1 && pending.object)
					pending.object->scaleUpdateInterval(packet->data + byteIterator, multiplier);

				byteIterator += pending.bytes;
			}

			client->send(packet, ObjectUpdates);

			//send hands the packet to ENet, which cleans it up, unless it never got that far
			if (packet->referenceCount == 0)
				enet_packet_destroy(packet);

			budget -= (int)bytesThisPacket;
			sent += inThisPacket;
		}
	}

	//Pass nullptr as server if this is being called from the client
	ObjHolder(SimObjectType _type,Server *_server = nullptr) : type(_type), server(_server)
	{
		ObjHolder<T>::lastNetID = 0;

		if (type == InvalidSimTypeId)
			error("ObjHolder::ObjHolder object type invalid");
	}

	~ObjHolder()
	{

	}
};