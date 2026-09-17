#include "Client.h"

float Client::getIncoming() 
{ 
	if(getTicksMS() - lastIncomingQueryTime > 1000)
	{
		lastIncoming = (float)client->totalReceivedData / (1024.0f * 1.0f);
		lastIncomingQueryTime = getTicksMS();
		client->totalReceivedData = 0;
	}

	return lastIncoming; 
}

float Client::getOutgoing() 
{
	if (getTicksMS() - lastOutgoingQueryTime > 1000)
	{
		lastOutgoing = (float)client->totalSentData / (1024.0f * 1.0f);
		lastOutgoingQueryTime = getTicksMS();
		client->totalSentData = 0;
	}

	return lastOutgoing;
}

void Client::tryApplyHeldPackets(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	auto iter = packets.begin();
	while (iter != packets.end())
	{
		HeldServerPacket* tmp = *iter;
		if (tmp->applyPacket(pd,simulation,cmdArgs))
		{
			delete tmp;
			tmp = 0;
			iter = packets.erase(iter);
		}
		else if (tmp->isExpired())
		{
			error("Discarding held packet that expired without being applied");
			delete tmp;
			tmp = 0;
			iter = packets.erase(iter);
		}
		else
			++iter;
	}
}

KickReason Client::run(const ClientProgramData& pd,Simulation &simulation, const ExecutableArguments& cmdArgs)
{
	scope("Client::run");

	if (!valid) //Not really a kick but we can't use this client anyway
		return KickReason::OtherReason;
	 
	if(packets.size() > 5)
		std::cout << packets.size() << " held packets\n";

	tryApplyHeldPackets(pd,simulation,cmdArgs);

	//Handle everything that arrived since last frame. Taking one event per frame falls further behind the server every
	//frame on a slow client, and a big burst like a join's brick packets takes one frame per packet
	ENetEvent event;
	int ret;
	while ((ret = enet_host_service(client, &event, 0)) > 0)
	{
		KickReason reason = handleEvent(event);
		if (reason != NotKicked)
			return reason;
	}

	if (ret < 0)
		error("enet_host_server failed");

	return NotKicked;
}

KickReason Client::handleEvent(ENetEvent& event)
{
	switch (event.type)
	{
		case ENET_EVENT_TYPE_DISCONNECT:
		{
			alreadyDisconnected = true;

			//Lack of a kick reason because the server didn't intentionally kick us
			if(event.data == 0)
				event.data = KickReason::OtherReason;

			info("Lost connection with server! Code: " + std::to_string(event.data));
			return (KickReason)event.data;
		}
		case ENET_EVENT_TYPE_RECEIVE:
		{
			//Each packet type gets its own file with the function that processes it
			FromServerPacketType type = (FromServerPacketType)event.packet->data[0];
			switch (type)
			{
				case AcceptConnection:
					packets.push_back(new AcceptConnectionPacket(packetHoldTime, event.packet));
					return NotKicked;

				case AddSimObjectType:
					packets.push_back(new AddSimObjectTypePacket(packetHoldTime, event.packet));
					return NotKicked;

				case AddSimObjects:
					packets.push_back(new AddSimObjectsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case UpdateSimObjects:
					packets.push_back(new UpdateSimObjectsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case DeleteSimObjects:
					packets.push_back(new DeleteSimObjectsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case ChatMessageFromServer:
					packets.push_back(new ChatMessagePacket(packetHoldTime, event.packet));
					return NotKicked;

				case ChatSuggestion:
					packets.push_back(new ChatSuggestionPacket(packetHoldTime, event.packet));
					return NotKicked;

				case EvalLoginResponse:
					packets.push_back(new EvalLoginResponsePacket(packetHoldTime, event.packet));
					return NotKicked;

				case ConsoleLine:
					packets.push_back(new ConsoleLinePacket(packetHoldTime, event.packet));
					return NotKicked;

				case TakeOverPhysics:
					packets.push_back(new TakeOverPhysicsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case CameraSettings:
					packets.push_back(new CameraSettingsPacket(packetHoldTime, event.packet));
					return NotKicked;
					 
				case MovementSettings:
					packets.push_back(new MovementSettingsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case MeshAppearance:
					packets.push_back(new MeshAppearancePacket(packetHoldTime, event.packet));
					return NotKicked;

				case MeshDecal:
					packets.push_back(new MeshDecalPacket(packetHoldTime, event.packet));
					return NotKicked;

				case ServerPerformanceDetails:
					packets.push_back(new ServerPerformanceDetailsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case CenterPrint:
					packets.push_back(new CenterPrintPacket(packetHoldTime, event.packet));
					return NotKicked;

				case SetClickAction:
					packets.push_back(new SetClickActionPacket(packetHoldTime, event.packet));
					return NotKicked;

				case HighlightAppearance:
					packets.push_back(new HighlightAppearancePacket(packetHoldTime, event.packet));
					return NotKicked;

				case NameTag:
					packets.push_back(new NameTagPacket(packetHoldTime, event.packet));
					return NotKicked;

				case DynamicBuoyancy:
					packets.push_back(new DynamicBuoyancyPacket(packetHoldTime, event.packet));
					return NotKicked;

				case WorldStateUpdate:
					packets.push_back(new WorldStateUpdatePacket(packetHoldTime, event.packet));
					return NotKicked;

				case AddBricks:
					packets.push_back(new AddBricksPacket(packetHoldTime, event.packet));
					return NotKicked;

				case RemoveBricks:
					packets.push_back(new RemoveBricksPacket(packetHoldTime, event.packet));
					return NotKicked;

				case SpecialBrickTypes:
					packets.push_back(new SpecialBrickTypesPacket(packetHoldTime, event.packet));
					return NotKicked;

				case BrickPrintTypes:
					packets.push_back(new BrickPrintTypesPacket(packetHoldTime, event.packet));
					return NotKicked;

				case AddSoundType:
					packets.push_back(new AddSoundTypePacket(packetHoldTime, event.packet));
					return NotKicked;

				case ParticleEmitterType:
					packets.push_back(new ParticleEmitterTypePacket(packetHoldTime, event.packet));
					return NotKicked;

				case OneShotSound:
					packets.push_back(new OneShotSoundPacket(packetHoldTime, event.packet));
					return NotKicked;

				case SoundLoop:
					packets.push_back(new SoundLoopPacket(packetHoldTime, event.packet));
					return NotKicked;

				case AudioEffect:
					packets.push_back(new AudioEffectPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VoiceFrameFromServer:
					packets.push_back(new VoiceFrameFromServerPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VoiceStatus:
					packets.push_back(new VoiceStatusPacket(packetHoldTime, event.packet));
					return NotKicked;

				case PlayerAbilities:
					packets.push_back(new PlayerAbilitiesPacket(packetHoldTime, event.packet));
					return NotKicked;

				case OpenWrenchDialog:
					packets.push_back(new OpenWrenchDialogPacket(packetHoldTime, event.packet));
					return NotKicked;

				case OpenPrintMenu:
					packets.push_back(new OpenPrintMenuPacket(packetHoldTime, event.packet));
					return NotKicked;

				case SkyboxPaths:
					packets.push_back(new SkyboxPathsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case ItemState:
					packets.push_back(new ItemStatePacket(packetHoldTime, event.packet));
					return NotKicked;

				case InventoryContents:
					packets.push_back(new InventoryContentsPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VehicleBricks:
					packets.push_back(new VehicleBricksPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VehicleDriver:
					packets.push_back(new VehicleDriverPacket(packetHoldTime, event.packet));
					return NotKicked;

				case OpenVehicleWrench:
					packets.push_back(new OpenVehicleWrenchPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VehicleSaveData:
					packets.push_back(new VehicleSaveDataPacket(packetHoldTime, event.packet));
					return NotKicked;

				case VehicleBricksBroken:
					packets.push_back(new VehicleBricksBrokenPacket(packetHoldTime, event.packet));
					return NotKicked;

				//Can't process packet
				case InvalidServer:
				default:
					error("Invalid packet type " + std::to_string(type) + " received from server.");
					enet_packet_destroy(event.packet);
					return NotKicked;
			}

			//Packet will be destroyed in destructor of HeldServerPacket
			return NotKicked;
		}
		case ENET_EVENT_TYPE_CONNECT:
		{
			error("Got some kind of double connection from server!");
			return NotKicked;
		}
	}
	return NotKicked;
}

Client::Client(std::string ip,unsigned int port,unsigned int _packetHoldTime, std::function<void()> pump) : packetHoldTime(_packetHoldTime)
{
	scope("Client::Client");

	client = enet_host_create(NULL, 1, EndOfChannels, 0, 0);
	if (!client)
	{
		error("enet_host_create failed");
		return;
	}

	ENetAddress address;
	enet_address_set_host(&address, ip.c_str());
	address.port = port;

	peer = enet_host_connect(client, &address, EndOfChannels, 0);
	if (!peer)
	{
		error("enet_host_connect failed");
		return;
	}

	//Wait up to 5000ms total for the connection, in short slices so we can call pump()
	//between them. Without a pump, this behaves the same as one big 5000ms wait.
	const unsigned int totalWaitMS = 5000;
	const unsigned int pollStepMS = pump ? 20 : totalWaitMS;

	ENetEvent event;
	bool connected = false;
	for (unsigned int waited = 0; waited < totalWaitMS; waited += pollStepMS)
	{
		int ret = enet_host_service(client, &event, pollStepMS);
		if (ret > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			connected = true;
			break;
		}
		if (ret < 0)
			break;
		if (pump)
			pump();
	}

	if (connected)
	{
		info("Connection made!");
		valid = true;
	}
	else
	{
		error("Could not connect!");
		valid = false;
	}
}

Client::~Client()
{
	if (valid)
	{
		if (!alreadyDisconnected)
		{
			enet_peer_disconnect(peer, 0);

			//Run the client for a little while longer to make sure the server acknowledges the disconnect
			ENetEvent event;
			if (enet_host_service(client, &event, 1500) > 0)
			{
				if (event.type == ENET_EVENT_TYPE_DISCONNECT)
					info("Server acknowledged disconnect");
			}

		}
		
		enet_peer_reset(peer);
		enet_host_destroy(client);
	}
}

void Client::send(const char* data, unsigned int len, PacketChannel channel)
{
	ENetPacket* packet = enet_packet_create(data, len, getFlagsFromChannel(channel));
	if (!packet)
	{
		scope("Client::send");
		error("enet_packet_create failed");
		return;
	}
	if (enet_peer_send(peer, channel, packet) < 0)
	{
		scope("JoinedClient::send");
		error("enet_peer_send failed");
	}
}

void Client::send(ENetPacket *packet, PacketChannel channel)
{
	if (enet_peer_send(peer, channel, packet) < 0)
	{
		scope("JoinedClient::send");
		error("enet_peer_send failed");
	}
}
