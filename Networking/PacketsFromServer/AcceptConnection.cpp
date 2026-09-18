#include "AcceptConnection.h"

bool AcceptConnectionPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	//Invalid empty packet
	if (packet->dataLength < 2)
		return true;

	switch (packet->data[1])
	{
		case ConnectionWrongVersion:
		{
			error("Server had different version than us!");
			pd.serverBrowser->setConnectionNote("Server version mismatch");
			return true;
		}
		case ConnectionNameUsed:
		{
			error("Someone is already using the name we requested!");
			pd.serverBrowser->setConnectionNote("Name already in use!");
			return true;
		}
		case ConnectionOkay:
		{
			//The menu demo joins behind the browser rather than through it, so it says nothing there
			if (!pd.menuDemo)
				pd.serverBrowser->setConnectionNote("Connection accepted, loading...");

			if (packet->dataLength < 6)
			{
				//Not enough bytes for number of types
				error("Invalid acceptance packet!");
				return true;
			}

			info("Join request accepted!");

			//Let the main loop know we're ready to start loading SimObject types, i.e. phase 1
			pd.getSignals()->startPhaseOneLoading = true;
			//How many SimObject types will we need to load until we're done with phase 1 loading
			memcpy(&pd.getSignals()->typesToLoad, packet->data + 2, sizeof(unsigned int));

			return true;
		}
	}

	return true;
}

AcceptConnectionPacket::AcceptConnectionPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

AcceptConnectionPacket::~AcceptConnectionPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
