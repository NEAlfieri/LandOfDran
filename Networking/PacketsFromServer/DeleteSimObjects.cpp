#include "DeleteSimObjects.h"

bool DeleteSimObjectsPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	//Too short to have any objects
	if (packet->dataLength < 4)
		return true;

	switch ((SimObjectType)packet->data[1])
	{
		case DynamicTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				for (int b = 0; b < simulation.controlledDynamics.size(); b++)
				{
					if (simulation.controlledDynamics[b]->getID() == id)
					{
						simulation.controlledDynamics.erase(simulation.controlledDynamics.begin() + b);
						break;
					} 
				}

				simulation.dynamics->destroyByID(id);
			}
			break;
		}
		case StaticTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				simulation.statics->destroyByID(id);
			}
			simulation.staticsChanged++;
			break;
		}
		case LightTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				simulation.lights->destroyByID(id);
			}
			break;
		}
		case EmitterTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				simulation.emitters->destroyByID(id);
			}
			break;
		}
		case RopeTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects && simulation.ropes; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				//One we never made, because its creation packet was cut short, isn't worth an error
				if (simulation.ropes->find(id))
					simulation.ropes->destroyByID(id);
			}
			break;
		}
		case VehicleTypeId:
		{
			unsigned int numObjects = packet->data[2];
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id;
				memcpy(&id, packet->data + 3 + a * sizeof(netIDType), sizeof(netIDType));

				//Whoever was driving or riding it gets out right where they were
				std::shared_ptr<Vehicle> vehicle = simulation.vehicles->find(id);
				if (!vehicle)
					continue;

				vehicle->releaseEveryone(simulation.idealBufferSize);
				vehicle.reset();
				simulation.vehicles->destroyByID(id);
			}
			break;
		}
	}

	return true;
}

DeleteSimObjectsPacket::DeleteSimObjectsPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

DeleteSimObjectsPacket::~DeleteSimObjectsPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
