#include "../Server.h"
#include "../ServerFiles.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../GameLoop/ClientData.h"

/*
	1 byte		-	packet type
	2 bytes		-	how many file IDs follow
	2 bytes each	-	the IDs of the offered files they want sent

	The client's answer to the list of add-on files it was offered as it joined, which is how it says it
	has looked at the list, even when it wants nothing off it. Whatever it asks for is sent on the join
	channel, and then everything that comes with joining goes out after it
*/
void serverFileRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 1 + sizeof(uint16_t))
		return;

	//They already answered, everything they were joining for has been sent
	if (source->sentJoinData)
		return;

	uint16_t count;
	memcpy(&count, packet->data + 1, sizeof(uint16_t));

	if (packet->dataLength < 1 + sizeof(uint16_t) + (size_t)count * sizeof(uint16_t))
		return;

	std::vector<uint16_t> ids;
	ids.reserve(count);

	for (uint16_t a = 0; a < count; a++)
	{
		uint16_t id;
		memcpy(&id, packet->data + 1 + sizeof(uint16_t) * (1 + a), sizeof(uint16_t));

		if (id >= pd->serverFiles.size())
			continue;

		//Asking for the same file over and over would be a way to make the server send far more than it offered
		if (std::find(ids.begin(), ids.end(), id) != ids.end())
			continue;

		ids.push_back(id);
	}

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	if (ids.size() > 0)
		info(source->name + " wants " + std::to_string(ids.size()) + " of the server's add-on files");

	//The rest of the join waits until the last batch of them has gone out, see serverFileResume
	if (startServerFiles(pd, source, client.get(), ids))
		sendJoinData(pdv, source);
}

/*
	1 byte		-	packet type

	They've taken everything in the last batch of files, so the next one goes out. Everything that comes
	with joining follows the last batch, see Networking/ServerFiles.h
*/
void serverFileResume(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	//They already have the lot, or never asked for any
	if (source->sentJoinData)
		return;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client || !client->fileSend.sending)
		return;

	if (sendNextServerFileBatch(pd, source, client.get()))
		sendJoinData(pdv, source);
}
