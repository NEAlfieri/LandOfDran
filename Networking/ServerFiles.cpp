#include "ServerFiles.h"
#include "../GameLoop/ServerProgramData.h"
#include "../GameLoop/ClientData.h"

//File bytes per ServerFileData packet, under the MTU with the header, same as vehicle and brick saves use
static constexpr size_t fileChunkBytes = 1100;

bool addServerFile(ServerProgramData* pd, const std::string& path, std::string& problem)
{
	if (!pd)
	{
		problem = "no server";
		return false;
	}

	if (path.length() < 1 || path.length() > 255)
	{
		problem = "file paths have to be between 1 and 255 characters";
		return false;
	}

	if (!isPathInsideGameFolder(path))
	{
		problem = "file has to be inside the game's folder";
		return false;
	}

	ContentFileKind kind = contentFileKind(path);
	if (kind == ContentFileUnknown)
	{
		problem = "clients only take sounds, models, textures, and the .txt descriptors that go with a model";
		return false;
	}

	std::error_code trouble;
	if (!std::filesystem::is_regular_file(path, trouble))
	{
		problem = "no such file";
		return false;
	}

	//Already offering it, registering it again just picks up whatever it says now
	for (ContentFile& existing : pd->serverFiles)
	{
		if (existing.path != path)
			continue;

		existing.kind = kind;
		existing.size = (uint32_t)GetFileSize(path);
		existing.checksum = getFileChecksum(path.c_str());
		return true;
	}

	if (pd->serverFiles.size() >= maxContentFiles)
	{
		problem = "a server can only offer " + std::to_string(maxContentFiles) + " files";
		return false;
	}

	long size = GetFileSize(path);
	if (size < 1 || size > (long)maxContentFileBytes)
	{
		problem = "files have to be between 1 byte and " + contentFileSizeName(maxContentFileBytes);
		return false;
	}

	ContentFile file;
	file.id = (uint16_t)pd->serverFiles.size();
	file.path = path;
	file.kind = kind;
	file.size = (uint32_t)size;
	file.checksum = getFileChecksum(path.c_str());
	pd->serverFiles.push_back(file);

	debug("Offering " + path + " to clients, " + contentFileSizeName(file.size));
	return true;
}

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
*/
void sendServerFileList(const ServerProgramData* pd, JoinedClient* source)
{
	static constexpr unsigned int headerBytes = 1 + sizeof(uint32_t) + 1;
	//The biggest one entry can be, so a packet is closed before it would go over the MTU
	static constexpr size_t maxEntryBytes = sizeof(uint16_t) + 1 + sizeof(uint32_t) * 2 + 1 + 255;

	uint32_t total = (uint32_t)pd->serverFiles.size();

	size_t at = 0;
	//One time round even with nothing on offer: the client waits for the list before it answers
	do
	{
		std::string entries = "";
		unsigned char count = 0;

		while (at < pd->serverFiles.size() && count < 255 && entries.size() + maxEntryBytes < fileChunkBytes)
		{
			const ContentFile& file = pd->serverFiles[at];
			unsigned char pathLength = (unsigned char)file.path.length();
			unsigned char kind = (unsigned char)file.kind;

			entries.append((const char*)&file.id, sizeof(uint16_t));
			entries.append((const char*)&kind, 1);
			entries.append((const char*)&file.size, sizeof(uint32_t));
			entries.append((const char*)&file.checksum, sizeof(uint32_t));
			entries.append((const char*)&pathLength, 1);
			entries.append(file.path);

			count++;
			at++;
		}

		ENetPacket* packet = enet_packet_create(NULL, headerBytes + entries.size(), getFlagsFromChannel(JoinNegotiation));
		packet->data[0] = ServerFileList;
		memcpy(packet->data + 1, &total, sizeof(uint32_t));
		packet->data[1 + sizeof(uint32_t)] = count;
		if (entries.size() > 0)
			memcpy(packet->data + headerBytes, entries.data(), entries.size());

		source->send(packet, JoinNegotiation);
	} while (at < pd->serverFiles.size());
}

/*
	1 byte		-	packet type
	1 byte		-	flags, see ServerFileFlag_BatchEnd
	2 bytes		-	file ID
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	The rest	-	file bytes
*/
bool startServerFiles(const ServerProgramData* pd, JoinedClient* source, ClientData* client, const std::vector<uint16_t>& ids)
{
	client->fileSend.ids = ids;
	client->fileSend.at = 0;
	client->fileSend.offset = 0;
	client->fileSend.sending = !ids.empty();

	if (!client->fileSend.sending)
		return true;

	return sendNextServerFileBatch(pd, source, client);
}

bool sendNextServerFileBatch(const ServerProgramData* pd, JoinedClient* source, ClientData* client)
{
	static constexpr unsigned int headerBytes = 1 + 1 + sizeof(uint16_t) + sizeof(uint32_t) * 2;

	ClientData::ServerFileSend& sending = client->fileSend;
	size_t sentThisBatch = 0;
	//Only one part of a batch carries the flag that asks them for the next one
	bool askedThisBatch = false;

	while (sending.at < sending.ids.size())
	{
		uint16_t id = sending.ids[sending.at];

		if (id >= pd->serverFiles.size())
		{
			sending.at++;
			sending.offset = 0;
			continue;
		}

		const ContentFile& file = pd->serverFiles[id];

		std::ifstream reading(file.path, std::ios::binary);
		if (!reading.is_open())
		{
			error("Client asked for " + file.path + " but it can't be opened anymore");
			sending.at++;
			sending.offset = 0;
			continue;
		}

		//It changed since it was offered, so what the client was told about it is wrong and it would only throw it away
		reading.seekg(0, std::ios::end);
		if ((uint32_t)reading.tellg() != file.size)
		{
			error("Client asked for " + file.path + " but it isn't the size it was when it was offered");
			sending.at++;
			sending.offset = 0;
			continue;
		}

		if (sending.offset == 0)
			info("Sending " + file.path + " to " + source->name);

		//As much of this file as is left, up to what fits in one packet
		size_t length = std::min((size_t)fileChunkBytes, (size_t)(file.size - sending.offset));
		std::string bytes(length, '\0');
		reading.seekg(sending.offset, std::ios::beg);
		reading.read(&bytes[0], length);
		reading.close();

		uint32_t total = file.size;
		uint32_t at = sending.offset;

		sending.offset += (uint32_t)length;
		if (sending.offset >= file.size)
		{
			sending.at++;
			sending.offset = 0;
		}

		sentThisBatch += length;

		//Whether anything at all is left decides if the client is asked to come back for more
		bool finished = sending.at >= sending.ids.size();
		bool batchFull = sentThisBatch >= serverFileBatchBytes;

		/*
			They're asked for the next batch half way through this one rather than at the end of it, so
			their answer is on its way while the rest of this batch is still going out and the connection
			never runs dry waiting for it. At most a batch and a half is ever queued for them
		*/
		bool askNow = !askedThisBatch && sentThisBatch >= serverFileBatchBytes / 2 && !finished;
		askedThisBatch = askedThisBatch || askNow;

		ENetPacket* chunk = enet_packet_create(NULL, headerBytes + length, getFlagsFromChannel(JoinNegotiation));
		chunk->data[0] = ServerFileData;
		chunk->data[1] = askNow ? ServerFileFlag_BatchEnd : 0;
		memcpy(chunk->data + 2, &id, sizeof(uint16_t));
		memcpy(chunk->data + 2 + sizeof(uint16_t), &total, sizeof(uint32_t));
		memcpy(chunk->data + 2 + sizeof(uint16_t) + sizeof(uint32_t), &at, sizeof(uint32_t));
		memcpy(chunk->data + headerBytes, bytes.data(), length);
		source->send(chunk, JoinNegotiation);

		if (finished || batchFull)
			break;
	}

	bool finished = sending.at >= sending.ids.size();
	if (finished)
	{
		sending.sending = false;
		sending.ids.clear();
	}

	return finished;
}
