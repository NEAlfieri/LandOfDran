#pragma once

#include "../NetTypes/NetType.h"
#include "Server.h"

extern "C" 
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

//Basically we wanna make sure packets are under the MTU if possible,
//but I'm not positive how much overhead ENet adds to packets
#define MTU ENET_HOST_DEFAULT_MTU - 20

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

		//Set class metatable
		lua_newtable(L);
		lua_getglobal(L, metatableName.c_str());
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
		tmp->me = tmp;
		allObjects.push_back(std::move(tmp));
		allObjects.back()->netID = lastNetID++;
		allObjects.back()->creationTime = SDL_GetTicks();
		allObjects.back()->onCreation();
		if(server)
			recentCreations.push_back(allObjects.back());
		return allObjects.back();
	}

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
	void sendAll(JoinedClient const *client) const
	{
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

		//If clients have over 400ms ping, only send every other iteration, if they have over 1500ms ping don't send at all
		static bool highPingIteration = false;
		highPingIteration = !highPingIteration;
			

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

		//Send any pending object updates to all connected clients:

		toSend = allObjects.size();
		sent = 0;

		//Send as many packets as needed to send all objects to the client
		//without any one packet exceeding the MTU
		while (toSend > 0)
		{
			//Didn't need updates, didn't move or get changed since last time
			int skippedThisPacket = 0;
			//Did need updates
			int sentThisPacket = 0;
			int bytesThisPacket = 0; 
			netIDType lastId = NO_ID; //Highest possible value, indicates no previous ID

			//I guess if one object was somehow larger than like 1300 bytes this would stall lol
			for (unsigned int a = sent; a < allObjects.size(); a++)
			{
				//Only using one byte to encode amount of objects in the packet
				if (sentThisPacket >= 255)
					break;

				if (!allObjects[a]->requiresNetUpdate())
				{
					skippedThisPacket++;
					continue;
				}

				sentThisPacket++;
				bytesThisPacket += allObjects[a]->getUpdatePacketBytes() + bytesForIdDelta(lastId,allObjects[a]->getID());

				if (bytesThisPacket > MTU)
				{
					sentThisPacket--;
					bytesThisPacket -= allObjects[a]->getUpdatePacketBytes() + bytesForIdDelta(lastId, allObjects[a]->getID());
					break;
				}

				lastId = allObjects[a]->getID();
			}

			//Not a break: we may have just skipped every object in this packet
			if (sentThisPacket == 0)
			{	
				toSend -= skippedThisPacket;
				sent += skippedThisPacket;
				continue;
			}

			//Three extra bytes, packet type, simobject type, amount of objects
			ENetPacket* packet = enet_packet_create(NULL, bytesThisPacket + 3, getFlagsFromChannel(ObjectUpdates));
			packet->data[0] = FromServerPacketType::UpdateSimObjects;
			packet->data[1] = type;
			packet->data[2] = sentThisPacket;
			int byteIterator = 3;

			lastId = NO_ID; //Reset for the actual packet

			//std::cout << "Sending " << sentThisPacket << " updates for type " << (unsigned int)type << " of " << allObjects.size() << " total\n";

			for (int a = sent; a < sent + sentThisPacket + skippedThisPacket; a++)
			{
				if (!allObjects[a]->flaggedForUpdate)
					continue;

				allObjects[a]->flaggedForUpdate = false;

				byteIterator += addIdDelta(lastId, allObjects[a]->getID(), packet->data + byteIterator);
				lastId = allObjects[a]->getID();

				//getUpdatePacketBytes might be const, but addToUpdatePacket will affect its value
				int amount = allObjects[a]->getUpdatePacketBytes();
				allObjects[a]->addToUpdatePacket(packet->data + byteIterator);
				byteIterator += amount;
			}

			//std::cout << packet->dataLength << " bytes for "<<sentThisPacket<<" objects\n";

			//server->broadcast(packet, Unreliable);
			for (unsigned int a = 0; a < server->getNumClients(); a++)
			{
				if (server->getClientByIndex(a)->getPing() > 1500)
					continue;

				if (server->getClientByIndex(a)->getPing() > 400 && !highPingIteration)
					continue;

				//TODO: This can crash because ENet handles cleanup of packets when you pass it to send
				//Sending it to no, or multiple clients, may cause issues
				server->getClientByIndex(a)->send(packet, ObjectUpdates);
			}

			//No clients with pings worth sending to
			if (packet->referenceCount == 0)
				enet_packet_destroy(packet);

			toSend -= (sentThisPacket + skippedThisPacket);
			sent += (sentThisPacket + skippedThisPacket); 
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