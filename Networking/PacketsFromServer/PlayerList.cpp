#include "PlayerList.h"

bool PlayerListPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte - packet type
		1 byte - how many players follow
		For each: their client net ID, 1 byte of PlayerListFlag bits, 2 bytes of ping in ms,
		1 byte name length then the name, 1 byte score text length then the text
	*/

	//It's only a list to show, so it doesn't wait for InGame: someone still loading already knows who's here
	if (packet->dataLength < 2)
		return true;

	unsigned int count = packet->data[1];
	size_t byteIterator = 2;

	//Reads a length byte and that many characters, false if the packet ends first
	auto readString = [this, &byteIterator](std::string& text)
	{
		if (packet->dataLength < byteIterator + 1)
			return false;
		unsigned int length = packet->data[byteIterator];
		byteIterator++;

		if (packet->dataLength < byteIterator + length)
			return false;
		text = std::string((char*)packet->data + byteIterator, length);
		byteIterator += length;
		return true;
	};

	std::vector<PlayerListEntry> players;
	for (unsigned int a = 0; a < count; a++)
	{
		if (packet->dataLength < byteIterator + sizeof(netIDType) + 1 + sizeof(uint16_t))
			return true;

		PlayerListEntry player;
		memcpy(&player.id, packet->data + byteIterator, sizeof(netIDType));
		byteIterator += sizeof(netIDType);

		player.admin = packet->data[byteIterator] & PlayerListFlag_Admin;
		byteIterator++;

		uint16_t ping;
		memcpy(&ping, packet->data + byteIterator, sizeof(uint16_t));
		byteIterator += sizeof(uint16_t);
		player.pingMS = ping;

		if (!readString(player.name) || !readString(player.scoreText))
			return true;

		//A client the server was part way through removing
		if (player.id == NO_ID)
			continue;

		players.push_back(player);
	}

	pd.playerList->setPlayers(std::move(players));

	return true;
}

PlayerListPacket::PlayerListPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

PlayerListPacket::~PlayerListPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
