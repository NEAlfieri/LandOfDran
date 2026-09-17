#include "ChatSuggestion.h"

bool ChatSuggestionPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte - packet type
		1 byte - command length, then the command, lowercase without the slash
		1 byte - suggestion text length, then the text
	*/

	//Sent while joining like sound types, so this doesn't wait for InGame
	if (packet->dataLength < 3)
		return true;

	unsigned int commandLength = packet->data[1];
	if (packet->dataLength < 3 + commandLength)
		return true;

	std::string command((char*)packet->data + 2, commandLength);

	unsigned int textLength = packet->data[2 + commandLength];
	if (packet->dataLength < 3 + commandLength + textLength)
		return true;

	std::string text((char*)packet->data + 3 + commandLength, textLength);

	pd.chatWindow->addSuggestion(command, text);

	return true;
}

ChatSuggestionPacket::ChatSuggestionPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

ChatSuggestionPacket::~ChatSuggestionPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
