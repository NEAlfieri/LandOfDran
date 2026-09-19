#include "BrickPrintTypes.h"

bool BrickPrintTypesPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte  - packet type
		2 bytes - how many prints follow
		For each:
		2 bytes - the server's print ID
		1 byte  - name length, then the name
	*/

	//Comes in before any bricks do, so this doesn't wait for InGame
	if (packet->dataLength < 3)
		return true;

	uint16_t count;
	memcpy(&count, packet->data + 1, sizeof(uint16_t));

	size_t byteIterator = 3;
	int missing = 0;
	std::string missingNames = "";

	for (unsigned int a = 0; a < count; a++)
	{
		if (packet->dataLength < byteIterator + 3)
			break;

		uint16_t serverID;
		memcpy(&serverID, packet->data + byteIterator, sizeof(uint16_t));
		unsigned int nameLength = packet->data[byteIterator + 2];
		byteIterator += 3;

		if (packet->dataLength < byteIterator + nameLength)
			break;

		std::string name((char*)packet->data + byteIterator, nameLength);
		byteIterator += nameLength;

		int local = pd.prints.find(name);
		if (simulation.printFromServer.size() <= serverID)
			simulation.printFromServer.resize(serverID + 1, 0);
		simulation.printFromServer[serverID] = local < 0 ? 0 : (uint16_t)(local + 1);

		//The wrench dialog offers what the server has, whether or not we have the image for it
		simulation.serverPrintNames.push_back(name);
		simulation.serverPrintIDs.push_back(serverID);

		if (local < 0)
		{
			missing++;
			if (missing <= 10)
				missingNames += (missingNames.empty() ? "" : ", ") + name;
		}
	}

	/*
		Files from this server may still be on their way, and a print among them is matched up once they're
		all here, so there's nothing to complain about yet, see LoopClient::run
	*/
	if (missing > 0 && !contentFiles().downloadsPending())
		error("The server has " + std::to_string(missing) + " prints we don't, bricks wearing them will look plain: " + missingNames);

	return true;
}

BrickPrintTypesPacket::BrickPrintTypesPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

BrickPrintTypesPacket::~BrickPrintTypesPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
