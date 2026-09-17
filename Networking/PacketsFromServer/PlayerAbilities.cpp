#include "PlayerAbilities.h"

bool PlayerAbilitiesPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte - packet type
		1 byte - PlayerAbility flags
	*/

	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 2)
		return true;

	simulation.jetsEnabled = packet->data[1] & PlayerAbility_Jets;
	simulation.flashlightEnabled = packet->data[1] & PlayerAbility_Flashlight;
	simulation.freeCameraEnabled = packet->data[1] & PlayerAbility_FreeCamera;

	return true;
}

PlayerAbilitiesPacket::PlayerAbilitiesPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

PlayerAbilitiesPacket::~PlayerAbilitiesPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
