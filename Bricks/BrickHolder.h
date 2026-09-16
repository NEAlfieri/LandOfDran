#pragma once

#include "Brick.h"
#include "BrickTypes.h"
#include "../Physics/PhysicsWorld.h"
#include "../Networking/Server.h"
#include "../External/RTree.h"

#include <unordered_set>

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

class InstancedBrickRenderer;

/*
	Every brick in the world, with overlap checks, lookups, physics bodies, and (server side) networking
	Not an ObjHolder: bricks are far too numerous for a full SimObject each
*/
class BrickHolder
{
	std::vector<Brick*> bricks;
	std::unordered_map<netIDType, Brick*> byId;

	//Every brick that has a name, by that name exactly, for getNamed. Bricks with no name are left out entirely
	std::unordered_map<std::string, std::vector<Brick*>> byName;

	//Inclusive min/max voxel bounds of every brick
	RTree<Brick*, int, 3, float> tree;

	//Box shapes shared by every brick of the same footprint and height
	std::unordered_map<unsigned int, btBoxShape*> shapes;

	std::shared_ptr<PhysicsWorld> world = nullptr;

	//Non-owning, for special bricks' sizes and collision shapes
	const BrickTypes* types = nullptr;

	//Non-owning, nullptr on the client
	Server* server = nullptr;

	//Non-owning, client only
	InstancedBrickRenderer* renderer = nullptr;

	//Server only: bricks added or changed since the last sendRecent, and IDs removed since then
	std::unordered_set<netIDType> pendingSends;
	std::vector<netIDType> pendingRemovals;

	//Server only: removed since the last sendRecent, and clients should show them popping loose
	std::vector<netIDType> pendingEffectRemovals;

	//Sends RemoveBricks packets for ids, each under the MTU
	void sendRemovals(const std::vector<netIDType>& ids, bool showEffect) const;

	netIDType lastNetId = 0;

	std::string metatableName = "";

	void insert(Brick* brick);
	void createBody(Brick* brick);
	void destroyBody(Brick* brick);

	//Puts a brick into byName under the name it has now, or takes it back out, both nothing for a brick with no name
	void addName(Brick* brick);
	void removeName(Brick* brick);

	//AddBricks packets for these bricks, each under the MTU
	std::vector<ENetPacket*> makeAddPackets(const std::vector<const Brick*>& toSend) const;

	public:

	//Bytes per brick in AddBricks packets
	static constexpr unsigned int recordBytes = 23;

	//typeID and printID are written as is, so the caller maps them between the client's and server's special types and prints
	static void writeRecord(const Brick* brick, enet_uint8* data);
	static Brick readRecord(const enet_uint8* data);

	/*
		Returns nullptr if the brick is zero sized, out of bounds, would overlap another brick, or is an unknown special type
		Special bricks take their size from their type, whatever desc says
	*/
	Brick* add(const Brick& desc);

	/*
		Client: applies a record from an AddBricks packet without validating it, the server already did
		A record for a brick we already have is an update to its color or collision
	*/
	Brick* addFromServer(const Brick& desc);

	/*
		Removes and deletes the brick
		showEffect has clients show it popping loose and flying off, for a brick removed on purpose one at a time
	*/
	void remove(Brick* brick, bool showEffect = false);

	void clear();

	void setColliding(Brick* brick, bool collides);
	void setColor(Brick* brick, const glm::u8vec4& color);

	//An unknown material becomes BrickMaterial_None
	void setMaterial(Brick* brick, unsigned char material);

	//0 for no print, otherwise 1 more than an index into PrintTypes, see Brick::printID
	void setPrint(Brick* brick, uint16_t printID);

	//Names are server side only, clients never learn them. Keeps byName up to date, so always rename through this
	void setName(Brick* brick, const std::string& name);

	//Would a brick with this min corner and rotated size overlap an existing one
	bool overlaps(int x, int y, int z, int footprintWidth, int height, int footprintLength) const;

	//Brick occupying this voxel, or nullptr
	Brick* getAt(int x, int y, int z) const;

	//Calls visit for every brick with any part inside the voxels from low to high, both inclusive
	void forEachInBox(const glm::ivec3& low, const glm::ivec3& high, const std::function<void(Brick*)>& visit) const;

	//nullptr if no brick has that ID
	Brick* find(netIDType netId) const;

	//How many bricks are named this, matching case exactly. An empty name is always 0, however many bricks have no name
	size_t numNamed(const std::string& name) const;

	//The index'th brick with that name, nullptr past the end. Removing one changes the order of the rest, like get
	Brick* getNamed(const std::string& name, size_t index) const;

	size_t size() const { return bricks.size(); }
	Brick* get(size_t index) const { return bricks[index]; }

	//Server: broadcast everything added, changed, or removed since the last call, once per tick
	void sendRecent();

	//Server: every brick, to a client that just finished loading
	void sendAll(JoinedClient const* client) const;

	//Server: the ID and name of every special type, to a client that just connected, see SpecialBrickTypesPacket
	void sendSpecialTypes(JoinedClient const* client) const;

	/*
		Lua bricks are tables with integer id and type (BrickTypeId) fields, looked up by id on each use so a
		table for a removed brick safely resolves to nullptr
		makeLuaMetatable deletes functions, like ObjHolder::makeLuaMetatable
	*/
	void makeLuaMetatable(lua_State* L, const std::string& name, luaL_Reg* functions);
	void pushLua(lua_State* L, const Brick* brick) const;
	//Pops the table on top of the stack, nullptr (with an error logged) if it isn't an existing brick
	Brick* popLua(lua_State* L) const;

	/*
		Server only, set by LoopServer: makes the music loop, light, and emitter of a brick added with attachments (from a save),
		and gets rid of them just before a brick with attachments is removed
	*/
	std::function<void(Brick*)> spawnAttachments = nullptr;
	std::function<void(Brick*)> removeAttachments = nullptr;

	//Client: every brick added, changed, or removed from here on is passed along to this renderer
	void setRenderer(InstancedBrickRenderer* _renderer) { renderer = _renderer; }

	const BrickTypes* getTypes() const { return types; }

	//Pass nullptr as _server on the client
	BrickHolder(std::shared_ptr<PhysicsWorld> _world, const BrickTypes* _types, Server* _server = nullptr);
	~BrickHolder();
};
