#include "DynamicPart.h"
#include <cmath>

bool DynamicPartPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	//See Dynamic::setPart for the layout
	size_t byteIterator = 1;
	if (packet->dataLength < byteIterator + sizeof(netIDType) + 1)
		return true;

	netIDType id;
	memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
	byteIterator += sizeof(netIDType);

	unsigned int slotLength = packet->data[byteIterator];
	byteIterator++;
	if (packet->dataLength < byteIterator + slotLength + 1)
		return true;

	std::string slot((char*)packet->data + byteIterator, slotLength);
	byteIterator += slotLength;

	unsigned int nameLength = packet->data[byteIterator];
	byteIterator++;
	if (packet->dataLength < byteIterator + nameLength + sizeof(float) * 5)
		return true;

	std::string partName((char*)packet->data + byteIterator, nameLength);
	byteIterator += nameLength;

	glm::vec4 color;
	memcpy(&color.r, packet->data + byteIterator, sizeof(float) * 4);
	byteIterator += sizeof(float) * 4;

	float scale;
	memcpy(&scale, packet->data + byteIterator, sizeof(float));
	if (!std::isfinite(scale) || scale <= 0)
		scale = 1.0f;

	std::shared_ptr<Dynamic> dynamic = simulation.dynamics->find(id);
	if (!dynamic)
		return false;

	//A part this game doesn't have is left off, like a face it doesn't have
	dynamic->setPart(slot, partName.empty() ? nullptr : pd.getPartModel(partName), color, scale);

	return true;
}

DynamicPartPacket::DynamicPartPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

DynamicPartPacket::~DynamicPartPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
