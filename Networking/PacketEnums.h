#pragma once

#include "enet/enet.h"

//Represents an ID sent from server to client for unambigous identification
//Every SimObject and SimObjectType should have one that's unique to it within its type
//Different types of objects can share the same ID
typedef unsigned int netIDType;

//Especially for delta compression: indicates no assigned ID, unsigned equiv to -1
#define NO_ID 4294967295 

/*
	For use with enet_peer_disconnect
*/
enum KickReason
{
	NotKicked = 0,				//Default value, Client::run ran and did not report being kicked
	OtherReason = 1000,			//Given if the destructor is called on JoinedClient
	ServerShutdown = 1001,		//Broadcast from the destructor of the Server itself
	ConnectionRejected = 1002,	//See the details of the AcceptConnection packet for more info
	LuaKick = 1003,				//Kicked by Lua script
};

/*
	For use with enet_host_broadcast and enet_peer_send
	These channels should be the same between server and client
	Their values are arbitrary, it only matters that they're consistant
*/
enum PacketChannel
{
	JoinNegotiation = 0,	//Reliable: Used for authentication, and receiving data about SimObject types
	BrickLoading = 1,		//Reliable: Used just for loading the initial build, unfortunately no unsequenced reliable packets in Enet
	OtherReliable = 2,		//Reliable: Used for anything else that needs a relaible packet
	ObjectUpdates = 3,		//Unreliable sequenced: Used for SimObject updates, mainly snapshot interpolation
	Unreliable = 4,			//Unreliable unsequenced: Used for some minor effects like playing sounds
	EndOfChannels = 5		//Not a channel, used in enet_host_create and enet_host_connect
};

//Each PacketChannel has an ideal type of packet
inline enet_uint32 getFlagsFromChannel(PacketChannel channel)
{
	switch (channel)
	{
		case BrickLoading:
		case JoinNegotiation: 
		case OtherReliable:
			return ENET_PACKET_FLAG_RELIABLE;
		case ObjectUpdates:
			return 0;
		case Unreliable:
			return ENET_PACKET_FLAG_UNSEQUENCED;
		default:
			return 0;
	}
}

/*
	Up to 256 types of packet from client to server
*/
enum FromClientPacketType : unsigned char
{
	InvalidClient = 0,		//Default value
	ConnectionRequest = 1,	//Client wants to connect, includes their name and login token if not a guest
	LoadingFinished = 2,	//Let server know we loaded all types and can start receiving actual object updates now
	ChatMessage = 3,		//Send a chat message to the server
	EvalLogin = 4,			//Try to log into the server's eval console with admin password
	EvalCommand = 5,		//Send a Lua command to the server
	ControlledPhysics = 6,	//Client to server transform updates for objects in simulation.controlledObjects
	MovementInputs = 7	,	//Client to server movement inputs for player controller, server will cache these and apply them each frame until a new packet comes in
	ClickDetails = 8,		//The client clicked in-game, includes world position, direction, and which mouse button it was
};

//Used with ConsoleLine packet
#define LogFlag_Error 1
#define LogFlag_Debug 2

//Use with CameraSettings packet
#define CameraFlag_BoundToObject 1
#define CameraFlag_LockDirection 2
#define CameraFlag_LockPosition 4
#define CameraFlag_LockUpVector 8

/*
	Up to 256 types of packet from server to client
*/
enum FromServerPacketType : unsigned char
{
	InvalidServer = 0,		//Default value
	AcceptConnection = 1,	//Connection request accepted, say how many types we expect to load
	AddSimObjectType = 2,
	AddSimObjects = 3,
	UpdateSimObjects = 4,
	DeleteSimObjects = 5,
	ChatMessageFromServer = 6,
	EvalLoginResponse = 7,	//Response to EvalLogin, did you get the password right?
	ConsoleLine = 8,		//A line of text from the server's logger to clients that have admin
	TakeOverPhysics = 9,	//Server wants client to take over (or relinquish) physics simulation of this object, probably the client's player or a driven car
	CameraSettings = 10,	//Tell client to bind/unbind camera to object or change other settings
	MovementSettings = 11,	//Player controller movement parameters
	MeshAppearance = 12,	//Change something about how a dynamic instance is rendered, i.e. mesh colors
	ServerPerformanceDetails = 13,	//Server sends client the slowest frame time in MS every second
	CenterPrint = 14,		//Show a temporary message in the center of the client's screen
	HighlightAppearance = 15,	//Apply or clear the outline/highlight effect on a dynamic or static instance
};

//For use with AcceptConnection packets
enum ConnectionResponse	: unsigned char
{
	ConnectionOkay = 1,				//Accepted, includes info on amount of SimObjectTypes
	ConnectionWrongVersion = 2,		//Rejected, wrong game version
	ConnectionNameUsed = 3			//Rejected, someone already has your guest name
};
