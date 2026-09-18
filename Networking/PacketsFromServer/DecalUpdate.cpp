#include "DecalUpdate.h"

bool DecalUpdatePacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	//Types come with the sounds and particles, before the server has even heard we're loading, see LoopClient::run
	if (!simulation.worldDecals)
		return false;

	if (packet->dataLength < 2)
		return true;

	size_t byteIterator = 2;

	switch ((DecalUpdateKind)packet->data[1])
	{
		case DecalUpdateType:
		{
			if (packet->dataLength < byteIterator + sizeof(uint16_t) + 1)
				return true;

			uint16_t id;
			memcpy(&id, packet->data + byteIterator, sizeof(uint16_t));
			byteIterator += sizeof(uint16_t);

			std::string text[2];
			for (std::string& part : text)
			{
				if (packet->dataLength < byteIterator + 1)
					return true;
				size_t length = packet->data[byteIterator++];
				if (packet->dataLength < byteIterator + length)
					return true;
				part = std::string((char*)packet->data + byteIterator, length);
				byteIterator += length;
			}

			simulation.worldDecals->setType(id, text[0], text[1], pd.textures);
			return true;
		}

		case DecalUpdateAdd:
		{
			if (packet->dataLength < byteIterator + sizeof(uint16_t) * 2)
				return true;

			uint16_t maxDecals, count;
			memcpy(&maxDecals, packet->data + byteIterator, sizeof(uint16_t));
			byteIterator += sizeof(uint16_t);
			memcpy(&count, packet->data + byteIterator, sizeof(uint16_t));
			byteIterator += sizeof(uint16_t);

			//The server had already forgotten the ones on bricks removed before these, so they don't count toward the limit
			simulation.worldDecals->prune(simulation.bricks);

			for (unsigned int a = 0; a < count && byteIterator + WorldDecal::recordBytes <= packet->dataLength; a++)
			{
				simulation.worldDecals->add(WorldDecal::read(packet->data + byteIterator));
				byteIterator += WorldDecal::recordBytes;
			}

			//With no decals at all this is Lua's setMaxDecals lowering the limit
			simulation.worldDecals->trim(maxDecals);
			return true;
		}

		case DecalUpdateClear:
			simulation.worldDecals->clear();
			return true;
	}

	return true;
}

DecalUpdatePacket::DecalUpdatePacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

DecalUpdatePacket::~DecalUpdatePacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
