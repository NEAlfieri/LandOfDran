#include "ServerFileData.h"

/*
	1 byte		-	packet type
	1 byte		-	flags, see ServerFileFlag_BatchEnd
	2 bytes		-	file ID
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	The rest	-	file bytes

	These arrive while we're still connecting, before the types that use them, so like the list itself
	this doesn't wait for InGame
*/
bool ServerFileDataPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	static constexpr unsigned int headerBytes = 1 + 1 + sizeof(uint16_t) + sizeof(uint32_t) * 2;

	if (packet->dataLength < headerBytes)
		return true;

	unsigned char flags = packet->data[1];

	uint16_t id;
	uint32_t total, offset;
	memcpy(&id, packet->data + 2, sizeof(uint16_t));
	memcpy(&total, packet->data + 2 + sizeof(uint16_t), sizeof(uint32_t));
	memcpy(&offset, packet->data + 2 + sizeof(uint16_t) + sizeof(uint32_t), sizeof(uint32_t));

	contentFiles().takeChunk(id, total, offset, packet->data + headerBytes, packet->dataLength - headerBytes);

	//That was the end of a batch, so the server is waiting to hear we took it before sending the next
	//Asked for whether or not the part itself was any good, or a bad one would leave the download stuck
	if (flags & ServerFileFlag_BatchEnd)
		contentFiles().batchArrived();

	return true;
}

ServerFileDataPacket::ServerFileDataPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

ServerFileDataPacket::~ServerFileDataPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
