#include "NameTag.h"

bool NameTagPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	constexpr unsigned int headerBytes = 2 + sizeof(netIDType) + sizeof(float) * 3;
	if (packet->dataLength < headerBytes)
		return true;

	netIDType id;
	memcpy(&id, packet->data + 1, sizeof(netIDType));

	glm::vec3 color;
	memcpy(&color.r, packet->data + 1 + sizeof(netIDType), sizeof(float) * 3);

	unsigned int textLength = packet->data[headerBytes - 1];
	if (headerBytes + textLength > packet->dataLength)
		return true;

	//The dynamic might not have arrived yet, hold onto this until it does
	std::shared_ptr<Dynamic> dynamic = simulation.dynamics->find(id);
	if (!dynamic)
		return false;

	dynamic->setNameTag(std::string((char*)packet->data + headerBytes, textLength), color);

	return true;
}

NameTagPacket::NameTagPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

NameTagPacket::~NameTagPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
