#include "Server.h"

size_t Server::getNumClients() const
{
	return clients.size();
}

std::shared_ptr<JoinedClient> Server::getClientByNetId(netIDType id) const
{
	for (unsigned int a = 0; a < clients.size(); a++)
	{
		if (clients[a]->getNetId() == id)
			return clients[a];
	}
	return nullptr;
}

std::shared_ptr<JoinedClient> Server::getClientByIndex(unsigned int idx) const
{
	if (idx >= clients.size())
		return nullptr;
	return clients[idx];
}

void Server::broadcast(const char* data, unsigned int len, PacketChannel channel) const
{
	ENetPacket* packet = enet_packet_create(data, len, getFlagsFromChannel(channel));
	if (!packet)
	{
		scope("Server::broadcast");
		error("enet_packet_create failed");
		return;
	}
	enet_host_broadcast(server, channel, packet);
}

void Server::broadcast(ENetPacket* packet, PacketChannel channel) const
{
	enet_host_broadcast(server, channel, packet);
}

void Server::switchPacketType(JoinedClient * source, ENetPacket* packet, const void* pd)
{
	FromClientPacketType type = (FromClientPacketType)packet->data[0];

	switch (type)
	{
		case ConnectionRequest:
		{
			applyConnectionRequest(source,this,packet,pd);
			return;
		}
		case LoadingFinished:
		{
			clientFinishedLoading(source, this, packet, pd);
			return;
		}
		case ChatMessage:
		{
			chatMessageSent(source, this, packet, pd);
			return;
		}
		case EvalLogin:
		{
			attemptEvalLogin(source, this, packet, pd);
			return;
		}
		case EvalCommand :
		{
			parseEvalCommand(source, this, packet, pd);
			return;
		}
		case ControlledPhysics:
		{
			applyPhysicsAdjustment(source, this, packet, pd);
			return;
		}
		case MovementInputs:
		{
			movementInputs(source, this, packet, pd);
			return;
		}
		case ClickDetails:
		{
			clickDetails(source, this, packet, pd);
			return;
		}
		case PlantBrickRequest:
		{
			plantBrick(source, this, packet, pd);
			return;
		}
		case UndoBrickRequest:
		{
			undoBrick(source, this, packet, pd);
			return;
		}
		case VoiceFrame:
		{
			voiceFrame(source, this, packet, pd);
			return;
		}
		case FlashlightRequest:
		{
			flashlightRequest(source, this, packet, pd);
			return;
		}
		case AppearanceChoice:
		{
			appearanceChoice(source, this, packet, pd);
			return;
		}
		case WrenchRequest:
		{
			wrenchRequest(source, this, packet, pd);
			return;
		}
		case WrenchSubmit:
		{
			wrenchSubmit(source, this, packet, pd);
			return;
		}
		case PrintSubmit:
		{
			printSubmit(source, this, packet, pd);
			return;
		}
		case PlayerGrab:
		{
			playerGrab(source, this, packet, pd);
			return;
		}
		case InventorySelect:
		{
			inventorySelect(source, this, packet, pd);
			return;
		}
		case DropItemRequest:
		{
			dropItemRequest(source, this, packet, pd);
			return;
		}
		case PaintCanRequest:
		{
			paintCanRequest(source, this, packet, pd);
			return;
		}
		case PaintChoice:
		{
			paintChoice(source, this, packet, pd);
			return;
		}
		case SliceRequest:
		{
			sliceRequest(source, this, packet, pd);
			return;
		}
		case VehicleWrenchSubmit:
		{
			vehicleWrenchSubmit(source, this, packet, pd);
			return;
		}
		case VehicleSaveRequest:
		{
			vehicleSaveRequest(source, this, packet, pd);
			return;
		}
		case VehicleUpload:
		{
			vehicleUpload(source, this, packet, pd);
			return;
		}
		case VehicleRemoveRequest:
		{
			vehicleRemoveRequest(source, this, packet, pd);
			return;
		}

		case InvalidClient:
		default:
			error("Invalid packet from client " + std::to_string(type));
			return;
	}
}

//Send something from the logger to any client with admin
void Server::updateAdminConsoles(const loggerLine &message) const
{
	for (unsigned int a = 0; a < getNumClients();  a++)
	{
		if (!clients[a]->isAdmin)
			continue;

		//TODO: Honestly no clue if it's safe to send the same allocated packet to multiple clients
		//Since ENet 'takes management' of the packets memory after you send it to a client
		ENetPacket* packet = enet_packet_create(NULL, message.text.length() + 3, getFlagsFromChannel(OtherReliable));
		packet->data[0] = (unsigned char)ConsoleLine;
		packet->data[1] = (message.isError ? LogFlag_Error : 0) | (message.isDebug ? LogFlag_Debug : 0);
		packet->data[2] = (unsigned char)message.text.length();
		memcpy(packet->data + 3, message.text.c_str(), message.text.length());

		clients[a]->send(packet, OtherReliable);
	}
}

void Server::broadcastChat(std::string message) const
{
	ENetPacket* ret = enet_packet_create(NULL, message.length() + 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)ChatMessageFromServer;
	ret->data[1] = (unsigned char)message.length();
	memcpy(ret->data + 2, message.c_str(), message.length());
	broadcast(ret, OtherReliable);
}

void Server::run(const void* pd, lua_State* L, EventManager * eventManager)
{
	scope("Server::run");

	if (!valid)
		return;

	//Handle everything that arrived since last tick. One event a tick (40 a second) falls behind as soon as one person talks,
	//voice chat alone is 50 packets a second per talker
	ENetEvent netEvent;
	int enetValue;
	while ((enetValue = enet_host_service(server, &netEvent, 0)) > 0)
	{
		switch (netEvent.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
			{
				clients.push_back(std::make_shared<JoinedClient>(netEvent,lastNetID));
				clients.back()->me = clients.back();
				lastNetID++;
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:
			{
				JoinedClient* client = (JoinedClient*)netEvent.peer->data;
				if (!client)
				{
					error("Got packet that had no JoinedClient pointer!");
					enet_packet_destroy(netEvent.packet);
					break;
				}

				switchPacketType(client,netEvent.packet,pd);
				enet_packet_destroy(netEvent.packet);

				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				//We could just jump directly to the joined client with the data pointer but we'd need to iterate the vector anyway to remove it
				auto iter = clients.begin();
				while (iter != clients.end())
				{
					std::shared_ptr<JoinedClient> client = *iter;

					if (client.get() == (JoinedClient*)netEvent.peer->data)
					{
						handleDisconnect((JoinedClient*)netEvent.peer->data,this,pd,L,eventManager);

						client->me.reset();
						client.reset();
						clients.erase(iter);
						break;
					}
					++iter;
				}
				break;
			}
			default:
				break;
		}
	}

	if (enetValue < 0)
		error("enet_host_service error");
}

Server::Server(int port)
{
	scope("Server::Server");

	address.host = ENET_HOST_ANY;
	address.port = port;
	server = enet_host_create(&address, 32, EndOfChannels, 0, 0);

	if (!server)
	{
		error("Could not create Enet server!");
		return;
	}

	valid = true;
}

Server::~Server()
{
	//enet_host_create failed (e.g. the port is already in use), there's no host to service or destroy
	if (!valid)
		return;

	for (unsigned int a = 0; a < clients.size(); a++)
		clients[a]->kick(KickReason::ServerShutdown);

	//Give disconnection packets time to send to clients
	ENetEvent event;
	for(int a = 0; a<10; a++)
		enet_host_service(server, &event, 100);

	enet_host_destroy(server);
}
