#include "SetClickAction.h"
#include "../ClickAction.h"
#include "../../GameLoop/ClientProgramData.h"

bool SetClickActionPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	ClickAction action;
	if (!action.readFrom(packet->data + 1, packet->dataLength - 1))
	{
		error("Malformed click action packet");
		return true;
	}

	//NO_ID is how the server says there's nothing to predict any more
	if (action.itemID == NO_ID)
		pd.clickActions->clear();
	else
		pd.clickActions->setAction(action);

	return true;
}

SetClickActionPacket::SetClickActionPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

SetClickActionPacket::~SetClickActionPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
