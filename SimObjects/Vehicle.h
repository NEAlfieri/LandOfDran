#pragma once

#include "SimObject.h"
#include "Dynamic.h"
#include "ParticleTypes.h"
#include "../Bricks/Brick.h"
#include "../Bricks/BrickTypes.h"

class InstancedBrickRenderer;
struct ClientData;

//One of a Vehicle's wheels, made from a wheel brick
struct VehicleWheel
{
	//Where its suspension hangs from in the vehicle body's space, its middle hangs straight down from there
	glm::vec3 connection = glm::vec3(0);
	//Half the wheel brick's height, world units
	float radius = 1.0f;
	//The wheel brick's thin side, world units
	float width = 1.0f;
	//From the wheel brick's wrench dialog, clients only get its suspension length
	WheelSettings settings;

	//From the server: radians it's turned for steering, how far below connection its middle is, and whether it's on the ground and throwing up dirt
	float steering = 0.0f;
	float suspension = 0.0f;
	bool contact = false;
	bool dirt = false;

	//Client: radians it has rolled, when its dirt emitter ejects, and its tire model
	float spin = 0.0f;
	EmitterClock dirtClock;
	ModelInstance* tire = nullptr;
};

//One of a Vehicle's seat bricks, where a passenger stands while someone else drives
struct PassengerSeat
{
	//Middle of the seat brick's top in the vehicle body's space, where a passenger's feet go
	glm::vec3 top = glm::vec3(0);

	//Net ID of the dynamic standing on it, NO_ID for none
	netIDType riderID = NO_ID;

	//Server: the client riding on it
	std::weak_ptr<ClientData> rider;

	//Client: the dynamic standing on it as of the last LoopClient::placeVehicleDrivers
	std::weak_ptr<Dynamic> seated;

	//Server: its seat brick was broken off, so nobody can ride on it anymore, see Lua's radiusImpulse
	bool broken = false;
};

/*
	Bricks sliced out of the world into one body that drives on wheels, like the old game's brick cars
	The server simulates it with Bullet's raycast vehicle, a player who right clicks it drives it from behind its steering wheel,
	and clients draw its bricks where the server says it is, with a body that follows along for bumping into and clicking
	Its bricks go out in VehicleBricks packets after its creation packet, see makeBrickPackets
*/
class Vehicle : public SimObject
{
	friend ObjHolder<Vehicle>;

	//Box shapes for its basic bricks, special bricks use their type's shape
	std::vector<btCollisionShape*> ownedShapes;
	btCompoundShape* shape = nullptr;

	//Server only
	btVehicleRaycaster* raycaster = nullptr;
	btRaycastVehicle* raycastVehicle = nullptr;

	//Server: getTicksMS of the last update sent
	unsigned int lastSentTime = 0;

	//Server: its driver is turning or braking, so fast wheels throw dirt
	bool throwsDirt = false;

	//Client: the renderer drawing its bricks, and their group there, see finishClient
	InstancedBrickRenderer* renderer = nullptr;
	int brickGroup = -1;

	//Makes shape from its colliding bricks, false if none of them collide
	bool buildShape(const BrickTypes* types);

	//Where its body is, from the physics on the server or where it's drawn on a client
	void getBodyTransform(bool drawn, glm::vec3& origin, glm::quat& rotation) const;

	protected:

	Vehicle();

	virtual void onCreation() override {}

	virtual void requestDestruction() override;

	public:

	//Like the old game
	static constexpr size_t maxBricks = 10000;
	static constexpr size_t maxWheels = 24;
	static constexpr size_t maxSeats = 32;

	//The seat number meaning the driver's seat rather than a passenger seat, see ClientData::vehicleSeat
	static constexpr int driverSeat = -1;

	//How far above where they stood a passenger is let out, world units
	static constexpr float passengerExitLift = 0.2f;
	//Longest its bricks can reach along any axis, world units
	static constexpr float maxSize = 40.0f;

	//Creation packet bytes per wheel: connection, radius, width, suspension length
	static constexpr unsigned int wheelCreationBytes = sizeof(float) * 6;
	//Update packet bytes per wheel: steering, suspension, contact and dirt flags
	static constexpr unsigned int wheelUpdateBytes = 3;
	//Creation packet bytes per passenger seat: its top, who rides on it
	static constexpr unsigned int seatCreationBytes = sizeof(float) * 3 + sizeof(netIDType);

	/*
		Its bricks, with positions from the min corner of the box their grid boxes fill, so every coordinate is 0 or more
		Special bricks have the server's type IDs on the server and ours on clients, see VehicleBricksPacket
		Server: attachments are the ones the bricks had in the world
	*/
	std::vector<Brick> bricks;

	//Server: its wheel bricks the same way, with their settings, for saving it
	std::vector<Brick> wheelBricks;

	//Client: how many bricks the server has, its body and drawing wait for all of them
	unsigned int expectedBricks = 0;

	//Where the min corner of its bricks' box is in its body's space, world units
	glm::vec3 brickOffset = glm::vec3(0);

	//Which way it drives in its body's space, along x or z, the way its steering wheel faces
	glm::vec3 forward = glm::vec3(0, 0, 1);

	//Where its driver stands in its body's space, and how far above there they're let out
	glm::vec3 seat = glm::vec3(0);
	float exitHeight = 1.0f;

	std::vector<VehicleWheel> wheels;

	//One per seat brick, in the order they were found
	std::vector<PassengerSeat> passengerSeats;

	//Its steering wheel brick's settings
	SteeringSettings steering;

	//Emitter type its wheels throw dirt with, noEmitterType for none, see Lua's setVehicleDirtEmitter
	static constexpr uint16_t noEmitterType = 65535;
	uint16_t dirtEmitterType = noEmitterType;

	btRigidBody* body = nullptr;

	//Net ID of the dynamic driving it, NO_ID for none
	netIDType driverID = NO_ID;

	//Server: the client driving it
	std::weak_ptr<ClientData> driver;

	//Server: net ID of the client that sliced it, NO_ID if Lua did
	netIDType builderID = NO_ID;

	//Server: whether Lua's radiusImpulse breaks its bricks off, see vehicle:setDestructable
	bool destructable = false;

	//Server: music from its wrench dialog, and the loop playing it, NO_ID for none
	std::string musicName = "";
	float musicVolume = 1.0f;
	float musicPitch = 1.0f;
	unsigned int musicLoopID = NO_ID;

	//Server: lights and emitters carried over from its bricks, removed with it
	std::vector<netIDType> lightIDs;
	std::vector<netIDType> emitterIDs;

	//Server: per wheel, whether it's under water, and when it last splashed, see LoopServer::updateVehicles
	std::vector<bool> wheelInWater;
	std::vector<unsigned int> lastSplashMS;

	//Server: when its driver was last told they're going as fast as it goes
	unsigned int lastSpeedWarningMS = 0;

	//Client: from the server's updates
	Interpolator interpolator;
	glm::vec3 serverVelocity = glm::vec3(0);

	//Client: where it's drawn, following the interpolator smoothly
	glm::vec3 renderedPosition = glm::vec3(0);
	glm::quat renderedRotation = glm::quat(1, 0, 0, 0);
	bool renderedTransformInitialized = false;

	//Client: the dynamic standing in its seat as of the last LoopClient::placeVehicleDrivers
	std::weak_ptr<Dynamic> seated;

	//Server: makes its body and wheels with its body at origin. bricks, wheels, forward, seat, and steering have to be set first. False if none of its bricks collide
	bool buildServer(const BrickTypes* types, const btVector3& origin);

	//Server: engine, brakes, and steering for the next step from its driver's keys, true if it's going too fast for the engine to push it any faster
	bool drive(bool forwardHeld, bool backwardHeld, bool leftHeld, bool rightHeld, bool brakeHeld);

	//Server: no engine or steering, and every wheel's brakes on, for a vehicle nobody is driving
	void park();

	//Server: copies each wheel's steering, suspension, and contact out of the physics, after a step
	void updateWheelStates();

	//Server: a wheel's middle and turn in the world as of the last step
	btTransform getWheelTransform(int wheel) const;

	//Where its driver stands in the world, facing the way it drives, from its body on the server or where it's drawn on a client
	btTransform getSeatTransform(bool drawn) const;

	//Where a passenger stands on one of its passenger seats in the world, feet on its top, turned around its up to face the way look points
	btTransform getPassengerTransform(int seat, const Dynamic& rider, const glm::vec3& look, bool drawn) const;

	//Server: the free passenger seat nearest a world position, -1 if they're all taken or it has none
	int findFreeSeat(const glm::vec3& targetPos) const;

	//Client: once every brick has arrived, makes its body, has renderer draw its bricks, and gives each wheel an instance of tireModel (which can be nullptr)
	void finishClient(const BrickTypes* types, InstancedBrickRenderer* _renderer, Model* tireModel);
	bool hasAllBricks() const { return bricks.size() >= expectedBricks; }

	/*
		Takes some of its bricks out by index (in any order, repeats and ones past the end ignored), rebuilding its body's shape around the rest and on the server its weight,
		and on a client drawing what's left. Its body keeps its old shape if none of the rest collide
	*/
	void removeBricks(std::vector<uint16_t> indices, const BrickTypes* types);
	int getBrickGroup() const { return brickGroup; }

	//Client: moves where it's drawn toward the latest snapshot and puts its body there, and rolls its wheels, before the physics step
	void updateSnapshot(float deltaT);

	//Client: transform it's drawn with, and the one its bricks' grid is drawn with
	glm::mat4 getDrawnTransform() const;
	glm::mat4 getBrickTransform() const;

	//Client: a wheel's middle and turn as it's drawn, without its tire's scale
	glm::mat4 getDrawnWheelTransform(int wheel) const;

	//Client: puts whoever is in a seat (driverSeat for the driver's) back into the physics world just above where they were
	void releaseSeated(int seat, float idealBufferSize);

	//Client: releaseSeated for the driver and every passenger
	void releaseEveryone(float idealBufferSize);

	//Client: bytes of a creation packet starting at src, 0 if available is too few
	static unsigned int readCreationBytes(const enet_uint8* src, size_t available);

	//Client: everything in a creation packet after the net ID
	void readCreation(const enet_uint8* src);

	//Client: getUpdatePacketBytes of an update packet
	void readUpdate(const enet_uint8* src, float idealBufferSize);

	//Server: VehicleBricks packets with all of its bricks
	std::vector<ENetPacket*> makeBrickPackets() const;

	//Server: a VehicleBricksBroken packet for bricks an impulse broke off, with how many it had before removeBricks took them out
	ENetPacket* makeBricksBrokenPacket(uint16_t bricksBefore, const std::vector<uint16_t>& indices, const glm::vec3& center, float strength) const;

	//Server: a VehicleDriver packet saying who's driving it and riding on each of its seats
	ENetPacket* makeDriverPacket() const;

	virtual bool requiresNetUpdate() override;

	virtual unsigned int getCreationPacketBytes() const override;

	virtual unsigned int getUpdatePacketBytes() const override;

	virtual void addToCreationPacket(enet_uint8* dest) const override;

	virtual void addToUpdatePacket(enet_uint8* dest) override;

	~Vehicle();
};

//The vehicle a body belongs to, nullptr if it isn't a vehicle's
std::shared_ptr<Vehicle> vehicleFromBody(const btCollisionObject* body);
