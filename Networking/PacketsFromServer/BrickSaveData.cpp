#include "BrickSaveData.h"
#include "../../Bricks/BrickSaves.h"

#include <sstream>

/*
	1 byte		-	packet type
	4 bytes		-	the request ID we sent
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	The rest	-	file bytes

	The server wrote who saved it and when, we put in the picture we drew as we asked for it, see LoopClient::renderSavePreview
*/
bool BrickSaveDataPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	static constexpr unsigned int headerBytes = 1 + sizeof(uint32_t) * 3;

	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < headerBytes)
		return true;

	uint32_t requestID, total, offset;
	memcpy(&requestID, packet->data + 1, sizeof(uint32_t));
	memcpy(&total, packet->data + 1 + sizeof(uint32_t), sizeof(uint32_t));
	memcpy(&offset, packet->data + 1 + sizeof(uint32_t) * 2, sizeof(uint32_t));

	//Not one we asked for
	auto found = simulation.brickSaves.find(requestID);
	if (found == simulation.brickSaves.end())
		return true;

	Simulation::PendingBrickSave& save = found->second;
	if (offset == 0)
		save.bytes.clear();
	else if (offset != save.bytes.size())
		return true;

	save.bytes.append((const char*)packet->data + headerBytes, packet->dataLength - headerBytes);
	if (save.bytes.size() < total)
		return true;

	if (!save.thumbnail.empty() && !setLodSaveThumbnail(save.bytes, save.thumbnail))
		error("Couldn't put the picture into " + save.path);

	std::error_code errorCode;
	std::filesystem::create_directories("Saves", errorCode);

	std::ofstream file(save.path, std::ios::binary);
	bool written = file.is_open() && file.write(save.bytes.data(), save.bytes.size());
	file.close();

	if (written)
	{
		std::istringstream stream(save.bytes);
		LodSaveInfo saveInfo;
		std::string count = readLodSaveInfo(stream, saveInfo) ? std::to_string(saveInfo.brickCount) + " bricks" : "the bricks";
		info("Saved " + count + " to " + save.path);
		pd.gui->addCenterPrint("Saved " + count + " to " + save.path, 4000, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		error("Couldn't write " + save.path);
		pd.gui->addCenterPrint("Couldn't write " + save.path, 4000, 1.0f, 0.4f, 0.4f);
	}

	simulation.brickSaves.erase(found);
	pd.brickSaveMenu->refresh();
	return true;
}

BrickSaveDataPacket::BrickSaveDataPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

BrickSaveDataPacket::~BrickSaveDataPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
