#include "AddSimObjects.h"
#include <tuple>

bool AddSimObjectsPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	//Too short to have any objects
	if (packet->dataLength < 4)
		return true;

	switch ((SimObjectType)packet->data[1])
	{
		case StaticTypeId:
		{
			unsigned int numObjects = packet->data[2];
			unsigned int byteIterator = 3;
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id, type;
				memcpy(&type, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);
				memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);

				glm::vec3 pos;
				glm::quat rot;

				memcpy(&pos, packet->data + byteIterator, sizeof(glm::vec3));
				byteIterator += sizeof(glm::vec3);

				memcpy(&rot, packet->data + byteIterator, sizeof(glm::quat));
				byteIterator += sizeof(glm::quat);

				float friction, restitution;
				memcpy(&friction, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
				memcpy(&restitution, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);

				unsigned char flags = packet->data[byteIterator];
				byteIterator++;

				std::vector<int> meshIdxs;
				std::vector<glm::vec4> meshColors;

				//Set mesh specific colors for each dynamic
				int numMeshes = packet->data[byteIterator];
				byteIterator++;

				for (int i = 0; i < numMeshes; i++)
				{
					int meshIdx = packet->data[byteIterator];
					byteIterator++;

					glm::vec4 color;
					memcpy(&color.r, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.g, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.b, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.a, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);

					meshIdxs.push_back(meshIdx);
					meshColors.push_back(color);
				}

				bool hasHighlight = packet->data[byteIterator] != 0;
				byteIterator++;

				glm::vec4 highlightColor(0, 0, 0, 0);
				float highlightThickness = 0;
				if (hasHighlight)
				{
					memcpy(&highlightColor.r, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.g, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.b, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.a, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightThickness, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
				}

				std::shared_ptr<DynamicType> foundType = nullptr;
				for (unsigned int i = 0; i < simulation.dynamicTypes.size(); i++)
				{
					if (simulation.dynamicTypes[i]->getID() == type)
					{
						foundType = simulation.dynamicTypes[i];
						break;
					}
				}

				//TODO: Actually return false if we can't find a type with the type ID given
				if (!foundType)
				{
					error("Could not find dynamic type server requested for static object creation.");
					continue;
				}

				/*
					TODO: Server can sometimes send objects twice if they are created right when the client joins
					Once in sendRecentCreations and once in sendAll, this extra check really shouldn't be needed
				*/
				if (simulation.statics->find(id))
					continue;

				simulation.statics->clientSetNextId(id); 
				std::shared_ptr<StaticObject> newStatic = simulation.statics->create(foundType, btVector3(pos.x, pos.y, pos.z), btQuaternion(rot.x, rot.y, rot.z, rot.w));
				newStatic->body->setFriction(friction);
				newStatic->body->setRestitution(restitution);
				 
				if(flags & 1)
					newStatic->setHidden(true);
				else
					newStatic->setHidden(false);
				if (flags & 2)
					newStatic->setColliding(true);
				else
					newStatic->setColliding(false);

				//Set the mesh colors we read earlier
				for (int i = 0; i < meshIdxs.size(); i++)
					newStatic->setMeshColor(meshIdxs[i], meshColors[i]);

				if (hasHighlight)
					newStatic->setHighlight(highlightColor, highlightThickness);

				if (byteIterator >= packet->dataLength)
					break;
			}

			//Point light shadows get redrawn, see LoopClient::renderEverything
			simulation.staticsChanged++;
			break;
		}
		case LightTypeId:
		{
			unsigned int numObjects = packet->data[2];
			unsigned int byteIterator = 3;
			for (unsigned int a = 0; a < numObjects; a++)
			{
				if (byteIterator + sizeof(netIDType) + Light::packetBytes > packet->dataLength)
					break;

				netIDType id;
				memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);

				//Can be sent twice, see the note on statics above
				if (!simulation.lights->find(id))
				{
					simulation.lights->clientSetNextId(id);
					std::shared_ptr<Light> newLight = simulation.lights->create(glm::vec3(0), glm::vec3(1), 0.0f, 0.0f, 0.0f);
					newLight->readFromPacket(packet->data + byteIterator);
				}

				byteIterator += Light::packetBytes;
			}
			break;
		}
		case EmitterTypeId:
		{
			unsigned int numObjects = packet->data[2];
			unsigned int byteIterator = 3;
			for (unsigned int a = 0; a < numObjects; a++)
			{
				if (byteIterator + sizeof(netIDType) + sizeof(uint32_t) + Emitter::packetBytes > packet->dataLength)
					break;

				netIDType id;
				memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);

				uint32_t ageMS;
				memcpy(&ageMS, packet->data + byteIterator, sizeof(uint32_t));
				byteIterator += sizeof(uint32_t);

				//Can be sent twice, see the note on statics above
				if (!simulation.emitters->find(id))
				{
					simulation.emitters->clientSetNextId(id);
					std::shared_ptr<Emitter> newEmitter = simulation.emitters->create((uint16_t)0, glm::vec3(0));
					newEmitter->readFromPacket(packet->data + byteIterator);
					newEmitter->startMS = (int64_t)SDL_GetTicks() - ageMS;
				}

				byteIterator += Emitter::packetBytes;
			}
			break;
		}
		case VehicleTypeId:
		{
			unsigned int numObjects = packet->data[2];
			unsigned int byteIterator = 3;
			for (unsigned int a = 0; a < numObjects; a++)
			{
				unsigned int bytes = Vehicle::readCreationBytes(packet->data + byteIterator, packet->dataLength - byteIterator);
				if (bytes == 0)
					break;

				netIDType id;
				memcpy(&id, packet->data + byteIterator, sizeof(netIDType));

				//Can be sent twice, see the note on statics above
				if (!simulation.vehicles->find(id))
				{
					simulation.vehicles->clientSetNextId(id);
					std::shared_ptr<Vehicle> vehicle = simulation.vehicles->create();
					vehicle->readCreation(packet->data + byteIterator);

					//Its bricks come in VehicleBricks packets after this, a model vehicle has none to wait for
					if (vehicle->hasAllBricks())
						vehicle->finishClient(&pd.brickTypes, pd.brickRenderer, pd.tireModel, simulation.dynamicTypes);
				}

				byteIterator += bytes;
			}
			break;
		}
		case DynamicTypeId:
		{
			unsigned int numObjects = packet->data[2];
			unsigned int byteIterator = 3;
			for (unsigned int a = 0; a < numObjects; a++)
			{
				netIDType id, type;
				memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);
				memcpy(&type, packet->data + byteIterator, sizeof(netIDType));
				byteIterator += sizeof(netIDType);
				glm::vec3 pos;
				getPosition(packet->data + byteIterator, pos);
				byteIterator += PositionBytes;
				glm::quat rot;
				getQuaternion(packet->data + byteIterator, rot);
				byteIterator += QuaternionBytes;

				std::shared_ptr<DynamicType> foundType = nullptr;
				for (unsigned int i = 0; i < simulation.dynamicTypes.size(); i++)
				{
					if (simulation.dynamicTypes[i]->getID() == type)
					{
						foundType = simulation.dynamicTypes[i];
						break;
					}
				}

				std::vector<int> meshIdxs;
				std::vector<glm::vec4> meshColors;

				//Set mesh specific colors for each dynamic
				int numMeshes = packet->data[byteIterator];
				byteIterator++;

				for (int i = 0; i < numMeshes; i++)
				{
					int meshIdx = packet->data[byteIterator];
					byteIterator++;

					glm::vec4 color;
					memcpy(&color.r, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.g, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.b, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&color.a, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);

					meshIdxs.push_back(meshIdx);
					meshColors.push_back(color);
				}

				bool hasHighlight = packet->data[byteIterator] != 0;
				byteIterator++;

				glm::vec4 highlightColor(0, 0, 0, 0);
				float highlightThickness = 0;
				if (hasHighlight)
				{
					memcpy(&highlightColor.r, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.g, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.b, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightColor.a, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
					memcpy(&highlightThickness, packet->data + byteIterator, sizeof(float));
					byteIterator += sizeof(float);
				}

				float buoyancy;
				memcpy(&buoyancy, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);

				//Faces by mesh index, from Dynamic::setMeshDecal
				std::vector<std::pair<int, int>> meshDecals;
				unsigned int numDecals = packet->data[byteIterator];
				byteIterator++;
				for (unsigned int i = 0; i < numDecals && byteIterator + 2 <= packet->dataLength; i++)
				{
					int meshIdx = packet->data[byteIterator];
					unsigned int nameLength = packet->data[byteIterator + 1];
					byteIterator += 2;
					if (byteIterator + nameLength > packet->dataLength)
						break;

					//A face or shirt this game doesn't have is left off
					meshDecals.emplace_back(meshIdx, pd.getDecal(std::string((char*)packet->data + byteIterator, nameLength)));
					byteIterator += nameLength;
				}

				//Text drawn floating over it, from Dynamic::setNameTag
				std::string nameTag = "";
				glm::vec3 nameTagColor(1, 1, 1);
				if (byteIterator < packet->dataLength)
				{
					unsigned int nameTagLength = packet->data[byteIterator];
					byteIterator++;

					if (nameTagLength > 0)
					{
						if (byteIterator + sizeof(float) * 3 + nameTagLength > packet->dataLength)
							break;

						memcpy(&nameTagColor.r, packet->data + byteIterator, sizeof(float) * 3);
						byteIterator += sizeof(float) * 3;
						nameTag = std::string((char*)packet->data + byteIterator, nameTagLength);
						byteIterator += nameTagLength;
					}
				}

				//Animations the server has looping on it, see Dynamic::startLoop
				std::vector<unsigned char> loops;
				if (byteIterator < packet->dataLength)
				{
					unsigned int numLoops = packet->data[byteIterator];
					byteIterator++;
					if (byteIterator + numLoops > packet->dataLength)
						break;

					loops.assign(packet->data + byteIterator, packet->data + byteIterator + numLoops);
					byteIterator += numLoops;
				}

				//Models worn on it, like a hat, by slot, from Dynamic::setPart
				std::vector<std::tuple<std::string, Model*, glm::vec4, float>> parts;
				bool partsMalformed = false;
				if (byteIterator < packet->dataLength)
				{
					unsigned int numParts = packet->data[byteIterator];
					byteIterator++;
					for (unsigned int i = 0; i < numParts; i++)
					{
						if (byteIterator + 1 > packet->dataLength)
						{
							partsMalformed = true;
							break;
						}
						unsigned int slotLength = packet->data[byteIterator];
						byteIterator++;

						if (byteIterator + slotLength + 1 > packet->dataLength)
						{
							partsMalformed = true;
							break;
						}
						std::string slot((char*)packet->data + byteIterator, slotLength);
						byteIterator += slotLength;
						unsigned int nameLength = packet->data[byteIterator];
						byteIterator++;

						if (byteIterator + nameLength + sizeof(float) * 5 > packet->dataLength)
						{
							partsMalformed = true;
							break;
						}
						std::string partName((char*)packet->data + byteIterator, nameLength);
						byteIterator += nameLength;
						glm::vec4 partColor;
						memcpy(&partColor.r, packet->data + byteIterator, sizeof(float) * 4);
						byteIterator += sizeof(float) * 4;
						float partScale;
						memcpy(&partScale, packet->data + byteIterator, sizeof(float));
						byteIterator += sizeof(float);

						//A part this game doesn't have is left off
						parts.emplace_back(slot, pd.getPartModel(partName), partColor, partScale);
					}
				}
				if (partsMalformed)
					break;

				//Items are followed by who carries them and what they play, see Item::writeState
				unsigned char kind = DynamicKind_Plain;
				enet_uint8* itemState = nullptr;
				if (byteIterator < packet->dataLength)
				{
					kind = packet->data[byteIterator];
					byteIterator++;

					if (kind == DynamicKind_Item)
					{
						if (byteIterator + Item::stateBytes > packet->dataLength)
							break;

						itemState = packet->data + byteIterator;
						byteIterator += Item::stateBytes;
					}
				}

				//TODO: Actually return false if we can't find a type with the type ID given
				if (!foundType)
				{
					error("Could not find dynamic type server requested for object creation.");
					continue;
				}

				/*
					TODO: Server can sometimes send objects twice if they are created right when the client joins
					Once in sendRecentCreations and once in sendAll, this extra check really shouldn't be needed
				*/
				if(simulation.dynamics->find(id))
					continue;

				simulation.dynamics->clientSetNextId(id);
				std::shared_ptr<Dynamic> newDynamic;
				if (kind == DynamicKind_Item)
					newDynamic = simulation.dynamics->createDerived<Item>(foundType, btVector3(pos.x, pos.y, pos.z), btQuaternion(rot.x, rot.y, rot.z, rot.w));
				else
					newDynamic = simulation.dynamics->create(foundType, btVector3(pos.x,pos.y,pos.z), btQuaternion(rot.x,rot.y,rot.z,rot.w));

				//Set the mesh colors we read earlier
				for(int i = 0; i < meshIdxs.size(); i++)
					newDynamic->setMeshColor(meshIdxs[i], meshColors[i]);

				if (hasHighlight)
					newDynamic->setHighlight(highlightColor, highlightThickness);

				newDynamic->buoyancy = buoyancy;

				for (const auto& [meshIdx, decalId] : meshDecals)
					newDynamic->setMeshDecal(meshIdx, decalId);

				if (!nameTag.empty())
					newDynamic->setNameTag(nameTag, nameTagColor);

				newDynamic->syncLoops(loops);

				for (const auto& [slot, partModel, partColor, partScale] : parts)
					newDynamic->setPart(slot, partModel, partColor, partScale);

				if (itemState)
					std::static_pointer_cast<Item>(newDynamic)->readState(itemState, true, simulation.idealBufferSize);

				//The server has projectiles pass through whoever fired them and hit things itself, so ours is only ever drawn where the server says
				//Otherwise one fired from inside our own player gets stuck on us by collision prediction, see LoopClient::predictLocalCollisions
				if (kind == DynamicKind_Projectile)
				{
					newDynamic->isProjectile = true;
					newDynamic->body->setCollisionFlags(newDynamic->body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
				}

				if (byteIterator >= packet->dataLength)
					break;
			}
			break;
		}
	}

	return true;
}

AddSimObjectsPacket::AddSimObjectsPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

AddSimObjectsPacket::~AddSimObjectsPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
