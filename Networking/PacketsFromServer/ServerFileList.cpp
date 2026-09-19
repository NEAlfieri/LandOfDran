#include "ServerFileList.h"

/*
	1 byte		-	packet type
	4 bytes		-	how many files the whole list has
	1 byte		-	how many of them are in this packet
	Each one:
		2 bytes		-	file ID
		1 byte		-	what kind of file it is, see ContentFileKind
		4 bytes		-	size in bytes
		4 bytes		-	CRC32 of the whole file
		1 byte		-	path length, then the path

	This comes while we're still connecting, before any type that could name one of these files, so it
	doesn't wait for InGame like most packets do
*/
bool ServerFileListPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	static constexpr unsigned int headerBytes = 1 + sizeof(uint32_t) + 1;

	if (packet->dataLength < headerBytes)
		return true;

	uint32_t total;
	memcpy(&total, packet->data + 1, sizeof(uint32_t));
	unsigned int count = packet->data[1 + sizeof(uint32_t)];

	contentFiles().startList(total);

	unsigned int byteIterator = headerBytes;
	for (unsigned int a = 0; a < count; a++)
	{
		if (packet->dataLength < byteIterator + sizeof(uint16_t) + 1 + sizeof(uint32_t) * 2 + 1)
			return true;

		ContentFile file;
		unsigned char kind = 0;

		memcpy(&file.id, packet->data + byteIterator, sizeof(uint16_t));
		byteIterator += sizeof(uint16_t);
		kind = packet->data[byteIterator];
		byteIterator++;
		memcpy(&file.size, packet->data + byteIterator, sizeof(uint32_t));
		byteIterator += sizeof(uint32_t);
		memcpy(&file.checksum, packet->data + byteIterator, sizeof(uint32_t));
		byteIterator += sizeof(uint32_t);

		unsigned int pathLength = packet->data[byteIterator];
		byteIterator++;

		if (packet->dataLength < byteIterator + pathLength)
			return true;

		file.path = std::string((char*)packet->data + byteIterator, pathLength);
		byteIterator += pathLength;
		file.kind = (ContentFileKind)kind;

		contentFiles().addOffer(file);
	}

	return true;
}

ServerFileListPacket::ServerFileListPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

ServerFileListPacket::~ServerFileListPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
