#include "CenterPrint.h"

bool CenterPrintPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	unsigned int minSize = 1 + sizeof(unsigned int) + sizeof(float) * 3 + 1;
	if (packet->dataLength < minSize)
		return true;

	int byteIterator = 1;

	unsigned int durationMS;
	memcpy(&durationMS, packet->data + byteIterator, sizeof(unsigned int));
	byteIterator += sizeof(unsigned int);

	float red, green, blue;
	memcpy(&red, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(&green, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(&blue, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);

	unsigned int length = packet->data[byteIterator];
	byteIterator += 1;

	if (packet->dataLength < byteIterator + length)
		return true;

	std::string text((char*)packet->data + byteIterator, length);

	pd.gui->addCenterPrint(text, durationMS, red, green, blue);

	return true;
}

CenterPrintPacket::CenterPrintPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

CenterPrintPacket::~CenterPrintPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
