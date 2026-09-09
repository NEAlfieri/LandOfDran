#include "MeshAppearance.h"

bool MeshAppearancePacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 3 + sizeof(netIDType) + sizeof(glm::vec4))
		return true;

	unsigned char simObjectType = packet->data[1];

	netIDType id;
	memcpy(&id, packet->data + 2, sizeof(netIDType));

	unsigned char meshIdx = packet->data[2 + sizeof(netIDType)];

	glm::vec4 color;
	memcpy(&color.r, packet->data + 3 + sizeof(netIDType) + sizeof(float) * 0, sizeof(float));
	memcpy(&color.g, packet->data + 3 + sizeof(netIDType) + sizeof(float) * 1, sizeof(float));
	memcpy(&color.b, packet->data + 3 + sizeof(netIDType) + sizeof(float) * 2, sizeof(float));
	memcpy(&color.a, packet->data + 3 + sizeof(netIDType) + sizeof(float) * 3, sizeof(float));

	if (simObjectType == DynamicTypeId)
	{
		std::shared_ptr<Dynamic> dynamic = simulation.dynamics->find(id);
		if (!dynamic)
			return false;

		dynamic->setMeshColor(meshIdx, color);

		return true;
	}
	else if (simObjectType == StaticTypeId)
	{
		std::shared_ptr<StaticObject> staticObject = simulation.statics->find(id);
		if (!staticObject)
			return false;

		staticObject->setMeshColor(meshIdx, color);

		return true;
	}

	error("Invalid simobject type for mesh appearance packet");
	return true;
}

MeshAppearancePacket::MeshAppearancePacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

MeshAppearancePacket::~MeshAppearancePacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
