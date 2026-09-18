#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"

/*
	See makeAppearanceChoicePacket in ClientPacketCreators.h for the layout

	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void appearanceChoice(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	//Sent right after the connection request, a client that request didn't let in has none
	auto clientData = pd->getClient(source->me);
	if (!clientData)
		return;

	size_t byteIterator = 1;

	//False if the packet ends early or the name is too long
	auto readName = [&](std::string& name) -> bool
	{
		if (byteIterator >= packet->dataLength)
			return false;

		size_t length = packet->data[byteIterator++];
		if (length > PlayerAppearance::maxNameLength || byteIterator + length > packet->dataLength)
			return false;

		name = std::string((char*)packet->data + byteIterator, length);
		byteIterator += length;
		return true;
	};

	PlayerAppearance appearance;
	if (!readName(appearance.face) || !readName(appearance.shirt) || !readName(appearance.hat) || byteIterator + 6 > packet->dataLength)
		return;

	//The hat's color, or none for its own look, and its size
	if (packet->data[byteIterator])
		appearance.hatColor = glm::vec4(glm::vec3(packet->data[byteIterator + 1], packet->data[byteIterator + 2], packet->data[byteIterator + 3]) / 255.0f, 1.0f);
	byteIterator += 4;
	appearance.hatScale = glm::clamp(packet->data[byteIterator] / 100.0f, PlayerAppearance::minHatScale, PlayerAppearance::maxHatScale);
	byteIterator++;

	unsigned int parts = std::min((unsigned int)packet->data[byteIterator++], PlayerAppearance::maxColors);
	for (unsigned int a = 0; a < parts; a++)
	{
		std::string meshName;
		if (!readName(meshName) || byteIterator + 3 > packet->dataLength)
			return;

		glm::vec3 color(packet->data[byteIterator], packet->data[byteIterator + 1], packet->data[byteIterator + 2]);
		byteIterator += 3;

		appearance.colors.emplace_back(lowercase(meshName), color / 255.0f);
	}

	PlayerAppearance previous = clientData->appearance;
	clientData->appearance = appearance;

	//A change saved while they're playing shows up on the player Lua already put their appearance on
	if (std::shared_ptr<Dynamic> player = clientData->appearanceTarget.lock())
		applyAppearance(server, *clientData, player, &previous);
}
