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
	VoiceData = 5,			//Unreliable sequenced: Voice chat frames, on their own so they're never held up behind anything else
	EndOfChannels = 6		//Not a channel, used in enet_host_create and enet_host_connect
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
		case VoiceData:
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
	PlantBrickRequest = 9,	//Place the client's ghost brick
	UndoBrickRequest = 10,	//Remove the last brick this client planted
	VoiceFrame = 11,		//One 20 ms Opus frame of voice chat while push to talk is held, see Audio/VoiceChat.h
	FlashlightRequest = 12,	//Turn the client's flashlight on or off, and what color it is
	AppearanceChoice = 13,	//How the client wants their player to look, sent as they connect and whenever they save a change, see PlayerAppearance
	WrenchRequest = 14,		//Wrench whatever brick is under the crosshair, see Networking/PacketsFromClient/Wrench.cpp
	WrenchSubmit = 15,		//What the client changed in the wrench dialog the server last sent them
	PlayerGrab = 16,		//The client left clicked in game, so their player plays its grab animation for everyone else
	InventorySelect = 17,	//Whether the client's item bar is out and which slot it has picked, see Networking/PacketsFromClient/Inventory.cpp
	DropItemRequest = 18,	//The client pressed the drop item keys, with the slot their item bar has picked
	PaintChoice = 19,		//The color and material the client's paint palette has picked, sent as they connect and whenever it changes
	SliceRequest = 20,		//Turn the bricks in the client's selection box into a vehicle, see Networking/PacketsFromClient/Slice.cpp
	VehicleWrenchSubmit = 21,	//What the client changed in the vehicle wrench dialog the server last sent them
	VehicleSaveRequest = 22,	//Send the client a save of a vehicle to write to their own computer, see Networking/PacketsFromClient/VehicleFiles.cpp
	VehicleUpload = 23,		//Part of a vehicle save from the client's computer, with where they placed its ghost
	VehicleRemoveRequest = 24,	//Remove the vehicle in the wrench dialog the server last sent them
	PaintCanRequest = 25,	//Whether the client's paint palette wants a paint can in their hand, see Networking/PacketsFromClient/Inventory.cpp
	PrintSubmit = 26,		//The print the client picked in the print menu the server last sent them, see Networking/PacketsFromClient/Print.cpp
};

//Flags byte after the mask of a ClickDetails packet
#define ClickFlag_Release 1			//A mouse button was let go rather than pressed, mask is just that button
#define ClickFlag_RightPress 2		//The button pressed was the right one, the mask can't say which when another is held too
#define ClickFlag_LeftPress 4		//The button pressed was the left one

//Byte after a dynamic's decals in creation packets, saying what kind of dynamic it is and what follows
enum DynamicKind : unsigned char
{
	DynamicKind_Plain = 0,		//Nothing follows
	DynamicKind_Item = 1,		//Item::stateBytes follow, see SimObjects/Item.h
	DynamicKind_Projectile = 2	//Nothing follows, made by Lua's addProjectile, clients only draw it where the server says

};

//Flags byte of an item's state, see Item::writeState
#define ItemFlag_Held 1				//In someone's inventory
#define ItemFlag_Equipped 2			//In their hand: their item bar is out with its slot picked

//Second flags byte of a dynamic in UpdateSimObjects and ControlledPhysics packets
#define DynamicExtra_Look 1			//2 bytes follow: where a player looks, see Dynamic::lookDirection
#define DynamicExtra_OneShot 2		//2 bytes follow: an animation ID played once and a count that changes each time it plays
#define DynamicExtra_PositionDelta 4	//The position is PositionDeltaBytes measured from the last keyframe rather than PositionBytes from the origin
//Two bits saying which keyframe the position either is, or is measured from, so a receiver that missed one drops the deltas that follow it
#define DynamicExtra_GenerationShift 3
#define DynamicExtra_GenerationMask 24

//Movement flags byte of MovementInputs packets
#define MovementFlag_Jump 1			//Jump was just pressed
#define MovementFlag_Forward 2
#define MovementFlag_Backward 4
#define MovementFlag_Left 8
#define MovementFlag_Right 16
#define MovementFlag_JumpHeld 32	//Jump is down at all, swims up
#define MovementFlag_Jet 64			//Right mouse is held, see PlayerController::control

//Flags byte of VoiceFrame and VoiceFrameFromServer packets
#define VoiceFlag_End 1			//Push to talk was let go, no Opus frame follows

//Biggest Opus frame a voice packet can carry, a 20 ms frame at the bitrate VoiceChat uses is closer to 70 bytes
constexpr unsigned int maxVoiceFrameBytes = 400;

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
	WorldStateUpdate = 16,	//Time of day, how fast it passes, and water level, every second and whenever they change
	AddBricks = 17,			//Bricks added or changed, sent on BrickLoading like every brick packet so they stay in order
	RemoveBricks = 18,		//IDs of removed bricks
	AddSoundType = 19,		//A sound's ID, name, and file, for every sound as a client joins and whenever Lua adds one
	OneShotSound = 20,		//Play a sound once, with no position, at a position, or following a dynamic
	SoundLoop = 21,			//Start or stop a looping sound, see SoundLoopOperation
	AudioEffect = 22,		//Reverb preset for every sound, see Audio/ReverbPresets.h
	VoiceFrameFromServer = 23,	//Someone else's voice chat frame, with who's talking and where they are
	VoiceStatus = 24,		//Whether you're muted, or a talker's name, see VoiceStatusKind
	DynamicBuoyancy = 25,	//A dynamic's buoyancy, so clients simulating it in water match the server
	ParticleEmitterType = 26,	//A particle or emitter type, for every one as a client joins and whenever Lua adds one, see ParticleEmitterTypeKind
	PlayerAbilities = 27,	//Whether Lua lets this client use jets and a flashlight, see PlayerAbility flags
	MeshDecal = 28,			//Put a face from Assets/faces or a shirt from Assets/shirts on one mesh of a dynamic, or take it off, see Dynamic::setMeshDecal
	SpecialBrickTypes = 29,	//The server's special brick type IDs and names as a client joins, so it can match them to its own, see SpecialBrickTypesPacket
	OpenWrenchDialog = 30,	//Open the wrench dialog for a brick, with its collision, name, and attachments, see Interface/WrenchDialog.h
	SkyboxPaths = 31,		//The day and night skyboxes from Lua's setSkybox, see Graphics/Skybox.h
	ItemState = 32,			//Who carries an item, whether it's in their hand, and the animations it plays, see Item::writeState
	InventoryContents = 33,	//The net IDs of the items in each of your inventory slots, see ClientData::sendInventory
	VehicleBricks = 34,		//Some of a vehicle's bricks, which come after its creation packet, see Vehicle::makeBrickPackets
	VehicleDriver = 35,		//Which dynamic is driving a vehicle, if any
	OpenVehicleWrench = 36,	//Open the wrench dialog for a vehicle, with its music, see Interface/WrenchDialog.h
	VehicleSaveData = 37,	//Part of a vehicle save the client asked for, see VehicleSaveDataPacket
	VehicleBricksBroken = 38,	//Bricks Lua's radiusImpulse broke off a destructable vehicle, which fly off as debris, see VehicleBricksBrokenPacket
	NameTag = 39,			//Text drawn floating over a dynamic, like a player's name, see Dynamic::makeNameTagPacket
	BrickPrintTypes = 40,	//The server's print IDs and names as a client joins, so it can match them to its own, see BrickPrintTypesPacket
	SetClickAction = 41,	//What the client should play the moment they click with an item, before hearing back, see Networking/ClickAction.h
	OpenPrintMenu = 42,		//Open the print menu for a brick, so the print gun can put a print on it, see Interface/PrintMenu.h
};

//Flags byte of a PlayerAbilities packet
#define PlayerAbility_Jets 1
#define PlayerAbility_Flashlight 2

//Second byte of a VoiceStatus packet
enum VoiceStatusKind : unsigned char
{
	VoiceStatusMuted = 0,		//1 byte, whether the server muted you
	VoiceStatusTalkerName = 1	//Net ID of a client and their name, so the client can show who's talking
};

//Where an OneShotSound or SoundLoop packet's sound plays, and what follows this byte
enum SoundLocationKind : unsigned char
{
	SoundLocationFlat = 0,		//No position, nothing follows
	SoundLocationFixed = 1,		//x, y, z floats
	SoundLocationDynamic = 2,	//Net ID of a dynamic to follow
	SoundLocationVehicle = 3	//Net ID of a vehicle to follow
};

//Second byte of a SoundLoop packet
enum SoundLoopOperation : unsigned char
{
	SoundLoopStart = 0,
	SoundLoopStop = 1
};

//For use with AcceptConnection packets
enum ConnectionResponse	: unsigned char
{
	ConnectionOkay = 1,				//Accepted, includes info on amount of SimObjectTypes
	ConnectionWrongVersion = 2,		//Rejected, wrong game version
	ConnectionNameUsed = 3			//Rejected, someone already has your guest name
};
