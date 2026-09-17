#include "WorldStateUpdate.h"

bool WorldStateUpdatePacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 1 + sizeof(double) + sizeof(float) * 2 + 1 + sizeof(float) * DayCycle::networkFloats + sizeof(float))
		return true;

	enet_uint8* data = packet->data + 1;

	memcpy(&simulation.worldTimeSeconds, data, sizeof(double));
	data += sizeof(double);

	memcpy(&simulation.timeScale, data, sizeof(float));
	data += sizeof(float);

	memcpy(&simulation.waterLevel, data, sizeof(float));
	data += sizeof(float);

	simulation.waterEnabled = data[0] != 0;
	data++;

	auto readColor = [&data](glm::vec3& color)
	{
		memcpy(&color[0], data, sizeof(float) * 3);
		data += sizeof(float) * 3;
	};
	for (SkyKeyframe& phase : simulation.dayCycle.phases)
	{
		readColor(phase.skyColor);
		readColor(phase.fogColor);
		readColor(phase.lightColor);
		readColor(phase.ambientColor);
	}

	memcpy(&simulation.dayCycle.fogStart, data, sizeof(float));
	data += sizeof(float);

	memcpy(&simulation.dayCycle.fogEnd, data, sizeof(float));
	data += sizeof(float);

	memcpy(&simulation.dayCycle.fogHeight, data, sizeof(float));
	data += sizeof(float);

	memcpy(&simulation.rainIntensity, data, sizeof(float));
	simulation.rainIntensity = std::clamp(simulation.rainIntensity, 0.0f, 1.0f);

	return true;
}

WorldStateUpdatePacket::WorldStateUpdatePacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

WorldStateUpdatePacket::~WorldStateUpdatePacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
