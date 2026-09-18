#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/VehicleLua.h"
#include "../../LuaFunctions/SoundLua.h"

#include <cmath>

//Biggest vehicle save a client can upload, a vehicle with every brick it can have with long names comes to a few hundred kilobytes
static constexpr size_t maxUploadBytes = 8 * 1024 * 1024;

//Save file bytes per VehicleSaveData packet, under the MTU with the header
static constexpr size_t saveChunkBytes = 1100;

//The old game made people wait 5 seconds between planting cars
static constexpr unsigned int loadCooldownMS = 5000;
static constexpr unsigned int saveCooldownMS = 1000;

//How far from their camera a client can place a vehicle's ghost, VehicleGhost::reach plus room for moving while it uploads
static constexpr float placeReach = 140.0f;

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID

	Sends the client a save of the vehicle in VehicleSaveData packets, which their game writes to a file
*/
void vehicleSaveRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(netIDType))
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	netIDType vehicleID;
	memcpy(&vehicleID, packet->data + 1, sizeof(netIDType));

	std::shared_ptr<Vehicle> vehicle = pd->vehicles->find(vehicleID);
	if (!vehicle)
	{
		source->sendCenterPrint("That vehicle is gone.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	//A vehicle save is a save of bricks, and a model vehicle has none, so there would be nothing in the file
	if (vehicle->isModelVehicle())
	{
		source->sendCenterPrint("That vehicle is a model, not bricks, so it can't be saved.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	unsigned int now = SDL_GetTicks();
	if (now - client->lastVehicleSaveMS < saveCooldownMS)
		return;
	client->lastVehicleSaveMS = now;

	/*
		1 byte		-	packet type
		4 bytes		-	vehicle net ID
		4 bytes		-	size of the whole file
		4 bytes		-	where in the file this packet's bytes go
		The rest	-	file bytes
	*/
	std::string file = makeVehicleSaveFile(*vehicle);
	uint32_t total = (uint32_t)file.size();
	static constexpr unsigned int headerBytes = 1 + sizeof(netIDType) + sizeof(uint32_t) * 2;

	for (size_t offset = 0; offset < file.size(); offset += saveChunkBytes)
	{
		size_t length = std::min(saveChunkBytes, file.size() - offset);
		uint32_t at = (uint32_t)offset;

		ENetPacket* chunk = enet_packet_create(NULL, headerBytes + length, getFlagsFromChannel(OtherReliable));
		chunk->data[0] = VehicleSaveData;
		memcpy(chunk->data + 1, &vehicleID, sizeof(netIDType));
		memcpy(chunk->data + 1 + sizeof(netIDType), &total, sizeof(uint32_t));
		memcpy(chunk->data + 1 + sizeof(netIDType) + sizeof(uint32_t), &at, sizeof(uint32_t));
		memcpy(chunk->data + headerBytes, file.data() + offset, length);
		source->send(chunk, OtherReliable);
	}
}

/*
	1 byte		-	packet type
	4 bytes		-	upload ID, a new one for each file
	1 byte		-	1 to load it as a vehicle, 0 as bricks
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	12 bytes	-	grid voxel the middle of its bottom goes at
	The rest	-	file bytes

	A vehicle save from the client's computer, placed where they put its ghost once all of it has arrived
*/
void vehicleUpload(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	static constexpr unsigned int headerBytes = 1 + sizeof(uint32_t) + 1 + sizeof(uint32_t) * 2 + sizeof(int32_t) * 3;
	if (packet->dataLength < headerBytes)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	uint32_t uploadID, total, offset;
	memcpy(&uploadID, packet->data + 1, sizeof(uint32_t));
	bool asVehicle = packet->data[1 + sizeof(uint32_t)] & 1;
	memcpy(&total, packet->data + 2 + sizeof(uint32_t), sizeof(uint32_t));
	memcpy(&offset, packet->data + 2 + sizeof(uint32_t) * 2, sizeof(uint32_t));
	int32_t spotValues[3];
	memcpy(spotValues, packet->data + 2 + sizeof(uint32_t) * 3, sizeof(spotValues));

	if (total == 0 || total > maxUploadBytes)
		return;

	//The first packet of a new file starts over
	if (offset == 0)
	{
		client->vehicleUpload.clear();
		client->vehicleUploadID = uploadID;
		playSoundFor(source, "UploadStart", 1.0f, 1.0f);
	}
	else if (uploadID != client->vehicleUploadID || offset != client->vehicleUpload.size())
		return;

	size_t length = packet->dataLength - headerBytes;
	if (client->vehicleUpload.size() + length > total)
	{
		client->vehicleUpload.clear();
		return;
	}

	client->vehicleUpload.append((const char*)packet->data + headerBytes, length);
	if (client->vehicleUpload.size() < total)
		return;

	std::string data;
	data.swap(client->vehicleUpload);

	unsigned int now = SDL_GetTicks();
	if (client->lastVehicleLoadMS != 0 && now - client->lastVehicleLoadMS < loadCooldownMS)
	{
		source->sendCenterPrint("Wait a moment before loading another vehicle.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	if (client->controllers.empty())
	{
		source->sendCenterPrint("You need a player to load a vehicle.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	//Where they put the ghost, which has to be about as far as their crosshair reaches
	glm::ivec3 spot(spotValues[0], spotValues[1], spotValues[2]);
	glm::vec3 camera = client->controllers[0].lastCameraPosition;
	glm::vec3 place = glm::vec3(spot) * glm::vec3(STUD_SIZE, PLATE_SIZE, STUD_SIZE);
	if (spot.y < 0 || !std::isfinite(camera.x) || !std::isfinite(camera.y) || !std::isfinite(camera.z) || glm::distance(camera, place) > placeReach)
	{
		source->sendCenterPrint("That's too far away to place a vehicle.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	client->lastVehicleLoadMS = now;

	std::string message;
	bool placed = loadVehicleSave(client.get(), data, spot, asVehicle, message);
	if (!message.empty())
		source->sendCenterPrint(message, 4000, 1.0f, placed ? 1.0f : 0.4f, placed ? 1.0f : 0.4f);
	if (placed)
		playSoundFor(source, "ProcessComplete", 1.0f, 1.0f);
}
