#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/BrickLua.h"
#include "../../LuaFunctions/ClientLua.h"
#include "../../LuaFunctions/SoundLua.h"
#include "../../Bricks/BrickSaves.h"

#include <sstream>

/*
	The saved bricks window: an admin asks for a save of every brick to keep on their own computer, and uploads one
	from their computer to load, which is how a build gets from one server to another
	Anyone else saves what their own game can see of the bricks without asking the server, see LoopClient::saveBricksLocally
	The server never lists or draws anything, the client's window is its own Saves folder
*/

//Save file bytes per BrickSaveData packet, under the MTU with the header
static constexpr size_t saveChunkBytes = 1100;

//Biggest save a client can upload, tens of thousands of bricks with long names come to a couple of megabytes, a big Blockland .bls to a few
static constexpr size_t maxUploadBytes = 16 * 1024 * 1024;

//How often one client can be sent a save or have one loaded, both go through every brick
static constexpr unsigned int saveCooldownMS = 5000;
static constexpr unsigned int loadCooldownMS = 5000;

/*
	1 byte		-	packet type
	4 bytes		-	request ID, which the save is sent back with

	Sends the client a save of every brick in BrickSaveData packets, which their game writes to a file, with their name and the time in it
*/
void brickSaveRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(uint32_t))
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	uint32_t requestID;
	memcpy(&requestID, packet->data + 1, sizeof(uint32_t));

	//Names, who built what, and everything on a brick are only for admins, everyone else's game saves what it can see
	if (!source->isAdmin)
	{
		source->sendCenterPrint("Only admins get the full save from the server.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	unsigned int now = SDL_GetTicks();
	if (client->lastBrickSaveMS != 0 && now - client->lastBrickSaveMS < saveCooldownMS)
	{
		source->sendCenterPrint("Wait a moment before saving again.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	if (pd->bricks->size() == 0)
	{
		source->sendCenterPrint("There are no bricks to save.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	//Lua can keep the build from being copied, saying nothing to the client
	if (pd->eventManager)
	{
		lua_State* L = pd->luaState;
		pushClientLua(L, source->me);
		pd->eventManager->callEvent(L, "ClientSaveBricks", 1);
		bool vetoed = lua_gettop(L) == 1 && lua_isnil(L, 1);
		lua_settop(L, 0);
		if (vetoed)
			return;
	}

	client->lastBrickSaveMS = now;

	std::vector<const Brick*> all;
	all.reserve(pd->bricks->size());
	for (size_t a = 0; a < pd->bricks->size(); a++)
		all.push_back(pd->bricks->get(a));

	//Their picture of it goes in on their end, see BrickSaveDataPacket
	LodSaveInfo saveInfo;
	saveInfo.savedBy = source->name;
	saveInfo.savedAt = (int64_t)time(nullptr);

	std::ostringstream stream(std::ios::binary);
	if (!writeLodBricks(stream, all, pd->bricks->getTypes(), &pd->prints, false, &saveInfo))
	{
		source->sendCenterPrint("Couldn't make the save.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	/*
		1 byte		-	packet type
		4 bytes		-	request ID
		4 bytes		-	size of the whole file
		4 bytes		-	where in the file this packet's bytes go
		The rest	-	file bytes
	*/
	std::string file = stream.str();
	uint32_t total = (uint32_t)file.size();
	static constexpr unsigned int headerBytes = 1 + sizeof(uint32_t) * 3;

	for (size_t offset = 0; offset < file.size(); offset += saveChunkBytes)
	{
		size_t length = std::min(saveChunkBytes, file.size() - offset);
		uint32_t at = (uint32_t)offset;

		ENetPacket* chunk = enet_packet_create(NULL, headerBytes + length, getFlagsFromChannel(OtherReliable));
		chunk->data[0] = BrickSaveData;
		memcpy(chunk->data + 1, &requestID, sizeof(uint32_t));
		memcpy(chunk->data + 1 + sizeof(uint32_t), &total, sizeof(uint32_t));
		memcpy(chunk->data + 1 + sizeof(uint32_t) * 2, &at, sizeof(uint32_t));
		memcpy(chunk->data + headerBytes, file.data() + offset, length);
		source->send(chunk, OtherReliable);
	}

	info(source->name + " was sent a save of " + std::to_string(all.size()) + " bricks, " + std::to_string(file.size()) + " bytes");
}

/*
	1 byte		-	packet type
	4 bytes		-	upload ID, a new one for each file
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	1 byte		-	flags, see BrickLoadFlag_Clear
	1 byte		-	1 for a Blockland .bls save, 0 for one of ours
	12 bytes	-	how far from where it was saved it goes, in studs, plates, and studs
	1 byte		-	length of the file name, then the name, just for messages
	The rest	-	file bytes

	A save from the client's computer, loaded on top of the bricks or in place of them once all of it has arrived, admins only
*/
void brickUpload(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	static constexpr size_t fixedBytes = 1 + sizeof(uint32_t) * 3 + 2 + sizeof(int32_t) * 3 + 1;
	if (packet->dataLength < fixedBytes)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	uint32_t uploadID, total, offset;
	memcpy(&uploadID, packet->data + 1, sizeof(uint32_t));
	memcpy(&total, packet->data + 1 + sizeof(uint32_t), sizeof(uint32_t));
	memcpy(&offset, packet->data + 1 + sizeof(uint32_t) * 2, sizeof(uint32_t));
	unsigned char flags = packet->data[1 + sizeof(uint32_t) * 3];
	bool blockland = packet->data[2 + sizeof(uint32_t) * 3] != 0;
	int32_t place[3];
	memcpy(place, packet->data + 3 + sizeof(uint32_t) * 3, sizeof(place));
	unsigned char nameLength = packet->data[fixedBytes - 1];

	size_t headerBytes = fixedBytes + nameLength;
	if (packet->dataLength < headerBytes)
		return;

	std::string fileName((const char*)packet->data + fixedBytes, nameLength);
	bool clearFirst = flags & BrickLoadFlag_Clear;

	//No point holding onto a file that can't be loaded, and this stops a whole upload from anyone else
	if (!source->isAdmin)
	{
		if (offset == 0)
			source->sendCenterPrint("Only admins can load saves.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	if (total == 0 || total > maxUploadBytes)
	{
		if (offset == 0)
			source->sendCenterPrint("That save is too big to upload.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	//The first packet of a new file starts over
	if (offset == 0)
	{
		client->brickUpload.clear();
		client->brickUploadID = uploadID;
		playSoundFor(source, "UploadStart", 1.0f, 1.0f);
	}
	else if (uploadID != client->brickUploadID || offset != client->brickUpload.size())
		return;

	size_t length = packet->dataLength - headerBytes;
	if (client->brickUpload.size() + length > total)
	{
		client->brickUpload.clear();
		return;
	}

	client->brickUpload.append((const char*)packet->data + headerBytes, length);
	if (client->brickUpload.size() < total)
		return;

	std::string data;
	data.swap(client->brickUpload);

	unsigned int now = SDL_GetTicks();
	if (client->lastBrickLoadMS != 0 && now - client->lastBrickLoadMS < loadCooldownMS)
	{
		source->sendCenterPrint("Wait a moment before loading again.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	//Bricks further out than this from the origin are thrown away by the loader anyway
	for (int axis = 0; axis < 3; axis++)
		place[axis] = std::clamp(place[axis], -100000, 100000);

	//Lua can stop the load, saying nothing to the client
	if (pd->eventManager)
	{
		lua_State* L = pd->luaState;
		pushClientLua(L, source->me);
		lua_pushstring(L, fileName.c_str());
		lua_pushboolean(L, clearFirst);
		lua_pushinteger(L, place[0]);
		lua_pushinteger(L, place[1]);
		lua_pushinteger(L, place[2]);
		pd->eventManager->callEvent(L, "ClientLoadBricks", 6);
		bool vetoed = lua_gettop(L) == 6 && lua_isnil(L, 1);
		lua_settop(L, 0);
		if (vetoed)
			return;
	}

	client->lastBrickLoadMS = now;

	if (clearFirst)
		pd->bricks->clear();

	std::string label = (fileName.empty() ? "a save" : fileName) + " (uploaded by " + source->name + ")";
	std::istringstream stream(data);
	int loaded = blockland ? loadBlocklandBricks(*pd->bricks, pd->brickTypes, &pd->prints, stream, label, makeBlocklandLookup(), place[0], place[1], place[2])
		: loadLodBricks(*pd->bricks, &pd->prints, stream, label, place[0], place[1], place[2]);

	if (loaded < 0)
	{
		source->sendCenterPrint("That file isn't a save this server can read.", 4000, 1.0f, 0.4f, 0.4f);
		return;
	}

	playSoundFor(source, "ProcessComplete", 1.0f, 1.0f);

	std::string count = std::to_string(loaded);
	std::string where = place[0] == 0 && place[1] == 0 && place[2] == 0 ? "" : " at " + std::to_string(place[0]) + ", " + std::to_string(place[1]) + ", " + std::to_string(place[2]);
	source->sendCenterPrint("Loaded " + count + " bricks from " + (fileName.empty() ? "your save" : fileName) + where, 4000, 1.0f, 1.0f, 1.0f);
	server->broadcastChat(source->name + " loaded " + (fileName.empty() ? "a save" : fileName) + " (" + count + " bricks)" + (clearFirst ? ", replacing the bricks" : ""));
	info(source->name + " loaded " + count + " bricks from " + label + where);
}
