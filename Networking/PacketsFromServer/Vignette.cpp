#include "Vignette.h"

bool VignettePacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	/*
		1 byte		-	packet type
		4 floats	-	red, green, blue, alpha, 0-1, how opaque the color is at the edges as the effect starts
		1 float		-	strength, how hard the picture wobbles, 0 for none, 1 about as much as being underwater
		4 bytes		-	unsigned int, how long it lasts in milliseconds, 0 to clear the vignette that's showing
	*/

	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 1 + sizeof(float) * 5 + sizeof(unsigned int))
		return true;

	int byteIterator = 1;

	float values[5];
	for (int a = 0; a < 5; a++)
	{
		memcpy(&values[a], packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);
	}

	unsigned int durationMS;
	memcpy(&durationMS, packet->data + byteIterator, sizeof(unsigned int));

	ScreenVignette& vignette = simulation.vignette;
	vignette.color = glm::vec3(values[0], values[1], values[2]);
	vignette.alpha = values[3];
	vignette.strength = values[4];
	vignette.durationMS = (float)durationMS;
	vignette.elapsedMS = 0.0f;

	return true;
}

VignettePacket::VignettePacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

VignettePacket::~VignettePacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
