#pragma once

#include "../LandOfDran.h"
#include "../GameLoop/ClientProgramData.h"

/*
	Types of SimObject, held by SimObject and NetType
	This is to net code as RigidBodyUserIndex is to physics code
*/
enum SimObjectType : unsigned char //might be used for packets who knows
{
	InvalidSimTypeId = 0,			//Default?
	DynamicTypeId = 1,				//Objects that can move around the scene, includes projectiles
	StaticTypeId = 2,				//Objects that don't move each frame, like a brick, but they just have their own mesh
	ClientTypeId = 3,				//Not currently an actual SimObject child class
	BrickTypeId = 4,				//Not a SimObject either, see Bricks/Brick.h
	LightTypeId = 5,				//Point lights, see SimObjects/Light.h
	EmitterTypeId = 6,				//Particle emitters, see SimObjects/Emitter.h
	VehicleTypeId = 7,				//Drivable vehicles sliced out of bricks, see SimObjects/Vehicle.h
	RopeTypeId = 8,					//Ropes tied between two things, see SimObjects/Rope.h
};

/*
	Abstract base class for net types, permits server iterating over allTypes to send types to clients
	net types in turn describe types of SimObjects
*/
class NetType
{
	protected:

	bool loaded = false; 

	SimObjectType type = InvalidSimTypeId;

	netIDType id = -1; //Default value suppresses warning

	public:

	const netIDType &getID() const { return id; }

	//Net types as opposed to SimObjects always just get their own packet
	//This is the packet from createTypePacket
	//Returns true on error
	virtual bool loadFromPacket(ENetPacket const* const packet, const ClientProgramData& pd) = 0;

	//Create the packet on the server to be passed to loadFromPacket on the client
	virtual ENetPacket * createTypePacket() const = 0; 

	virtual ~NetType() {};
	NetType(SimObjectType _type) : type(_type) {}
};

