#include "AddSoundType.h"

bool AddSoundTypePacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte  - packet type
		2 bytes - sound ID
		1 byte  - flags, 1 = music
		1 byte  - name length, then the name
		1 byte  - file path length, then the path
		4 bytes - full volume distance, left off by servers from before sounds had one
	*/

	//Sounds are needed from the moment objects start arriving, so this doesn't wait for InGame
	if (packet->dataLength < 5)
		return true;

	uint16_t id;
	memcpy(&id, packet->data + 1, sizeof(uint16_t));
	bool isMusic = packet->data[3] & 1;

	unsigned int byteIterator = 4;
	unsigned int nameLength = packet->data[byteIterator];
	byteIterator++;
	if (packet->dataLength < byteIterator + nameLength + 1)
		return true;

	std::string name((char*)packet->data + byteIterator, nameLength);
	byteIterator += nameLength;

	unsigned int pathLength = packet->data[byteIterator];
	byteIterator++;
	if (packet->dataLength < byteIterator + pathLength)
		return true;

	std::string filePath((char*)packet->data + byteIterator, pathLength);
	byteIterator += pathLength;

	float fullVolumeDistance = AudioSystem::defaultFullVolumeDistance;
	if (packet->dataLength >= byteIterator + sizeof(float))
	{
		memcpy(&fullVolumeDistance, packet->data + byteIterator, sizeof(float));
		if (!std::isfinite(fullVolumeDistance))
			fullVolumeDistance = AudioSystem::defaultFullVolumeDistance;
		fullVolumeDistance = std::clamp(fullVolumeDistance, 1.0f, 500.0f);
	}

	pd.audio->addSoundType(id, name, filePath, isMusic, fullVolumeDistance);

	return true;
}

AddSoundTypePacket::AddSoundTypePacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

AddSoundTypePacket::~AddSoundTypePacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
