#pragma once

#include "SimObject.h"
#include "Dynamic.h"
#include "ParticleTypes.h"
#include "../Bricks/Brick.h"
#include "../Bricks/BrickTypes.h"
#include "../Bricks/BrickAttachments.h"

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
	What makes a vehicle fly rather than only drive, from Lua's spawnModelVehicle settings.flight, see
	vehicle:setFlight. Every number is in the same studs and seconds the rest of the game is in, and the
	ones that fight gravity are given as speeds rather than forces so they mean the same thing whatever
	the vehicle weighs: a Blockland plane's datablock is a pile of forces divided by a mass, which stops
	meaning anything the moment either the mass or the gravity is different from Torque's

	None of this is sent to clients. A plane is an ordinary vehicle as far as they are concerned: the
	server pushes its body around and they draw it where the server says it is
*/
struct VehicleFlight
{
	//Whether any of the rest of this is used at all
	bool enabled = false;

	//Studs a second squared along its nose with the throttle forward (W) and back (S)
	float thrust = 0.0f;
	float reverseThrust = 0.0f;

	//Studs a second the throttle stops pushing past, forward and backward
	float maxSpeed = 120.0f;
	float maxReverseSpeed = 30.0f;

	/*
		Airspeed along its nose where its wings hold up exactly its own weight, so it flies level at this
		speed with its nose level. Lift goes with the square of the airspeed from there, and never pushes
		harder than maxLift times its weight, so a dive doesn't turn into a slingshot
	*/
	float liftSpeed = 60.0f;
	float maxLift = 8.0f;

	//Airspeed its wings and controls reach their full effect at, and do nothing at all at a standstill
	float stallSpeed = 20.0f;

	/*
		How much more lift the wings make per radian of angle of attack, which is how far its nose is above
		the way it's actually going: without this a plane's wings only know how fast it's going, so pulling
		the nose up would only slow it down and sink it. Past stallAngle the wings start to give up, and
		twice that far they do nothing at all, which is a stall: the nose drops until the air is over them
		again
	*/
	float angleLift = 2.5f;
	float stallAngle = 0.35f;

	/*
		Per second, how quickly the wings and the tail fin bleed off the part of its velocity that isn't
		along its nose: the wing catches it moving up or down through the air and the fin catches it
		sliding sideways, which together are what make a banked turn carry it around rather than skid
	*/
	float wingDamping = 1.2f;
	float finDamping = 0.9f;

	//Airspeed where drag holds up exactly its own weight, which is how fast it ends up falling in a dive
	float dragSpeed = 150.0f;

	//Radians a second the controls ask for, and seconds it takes to reach them
	float pitchRate = 1.4f;
	float yawRate = 0.7f;
	float rollRate = 2.2f;
	float response = 0.25f;

	/*
		Radians a second of roll toward the bank it wants while no roll key is held, 0 to leave it rolled
		wherever it is. What it wants is turnBank radians of bank into however hard it's turning, and
		wings level when it isn't turning, which is what makes a turn a banked one rather than a flat
		skid. Upside down it wants nothing: a plane held inverted stays inverted until its pilot rolls it
		back, which is the whole point of a stunt plane
	*/
	float levelRate = 0.8f;
	float turnBank = 0.7f;

	void clampValues();
};

//Server: what its driver was last holding down, kept for the flight forces each physics substep, see Vehicle::flyStep
struct VehicleInput
{
	bool forward = false, backward = false, left = false, right = false, brake = false;

	//Where the driver is looking, which a plane turns its nose toward, and whether anyone has said
	glm::vec3 look = glm::vec3(0);
	bool hasLook = false;

	//Whether anybody (a client, or Lua through vehicle:drive) is at the controls at all
	bool driven = false;
};

/*
	Bricks sliced out of the world into one body that drives on wheels, like the old game's brick cars
	The server simulates it with Bullet's raycast vehicle, a player who right clicks it drives it from behind its steering wheel,
	and clients draw its bricks where the server says it is, with a body that follows along for bumping into and clicking
	Its bricks go out in VehicleBricks packets after its creation packet, see makeBrickPackets

	A vehicle can be one model instead, see Lua's spawnModelVehicle and isModelVehicle: it has no bricks at all, its
	body is a single box, and clients draw a dynamic type's model in place of the bricks. Everything else about it -
	the raycast vehicle, driving, seats, getting in and out, the wrench dialog - is the same either way
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

	//Server: something in the update packet besides where it is changed, like its headlight, so one goes out even while it sits still
	bool stateChanged = false;

	//Server: its driver is turning or braking, so fast wheels throw dirt
	bool throwsDirt = false;

	//Client: the renderer drawing its bricks, and their group there, see finishClient
	InstancedBrickRenderer* renderer = nullptr;
	int brickGroup = -1;

	//Makes shape out of its colliding bricks, or one box for a model vehicle, false if it comes out with nothing in it
	bool buildShape(const BrickTypes* types);

	//Where its body is, from the physics on the server or where it's drawn on a client
	void getBodyTransform(bool drawn, glm::vec3& origin, glm::quat& rotation) const;

	protected:

	Vehicle();

	virtual void onCreation() override {}

	virtual void requestDestruction() override;

	public:

	/*
		Server: what makes it a plane rather than a car, see Lua's spawnModelVehicle. flyStep runs before
		every physics substep while this is switched on, from the keys and look left in lastInput
	*/
	VehicleFlight flight;
	VehicleInput lastInput;

	/*
		Server: one physics substep of flight, nothing at all unless flight.enabled. Its wings, drag and
		thrust are impulses on its body and its controls turn it by steering its spin, see the comments there
	*/
	void flyStep(float deltaT);

	/*
		Server: Lua is driving it with vehicle:drive, with nobody in the seat. The keys are held until it's
		called again, the way a driver's are between their movement packets, and a real driver getting in
		takes it back over. See LoopServer::updateVehicles
	*/
	bool luaDriving = false;
	bool luaForward = false, luaBackward = false, luaLeft = false, luaRight = false, luaBrake = false;

	/*
		Server: the dynamic in the driver's seat was put there by vehicle:setDriver rather than by a client
		getting in, so it has no ClientData behind it. Everything that draws a driver already goes by
		driverID, which is a dynamic's net ID; this is only so that the check throwing out a driver whose
		client has gone leaves it alone. See LoopServer::updateVehicles
	*/
	bool luaSeatedDriver = false;

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

	//Update packet bytes for its animations: the one shot one and its count, then how many loop
	static constexpr unsigned int animationUpdateBytes = 3;

	//The flags byte in creation and update packets
	static constexpr unsigned char flagHasHeadlight = 1;
	static constexpr unsigned char flagHeadlightOn = 2;
	//Whether it's a plane, which clients only use to tell its driver what its controls do
	static constexpr unsigned char flagFlies = 4;

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

	/*
		The dynamic type whose model is this vehicle's body, drawn where a brick vehicle's bricks would be,
		NO_ID for a vehicle sliced out of bricks. Its model's own origin is this vehicle's body origin, so
		everything below in the body's space is also in the model's space, see Lua's spawnModelVehicle
	*/
	netIDType bodyTypeID = NO_ID;

	//The dynamic type its wheels are drawn with, NO_ID for whatever tire model the client has
	netIDType wheelTypeID = NO_ID;

	//A model vehicle's one collision box, half its size and where its middle sits in the body's space, world units
	glm::vec3 bodyHalfExtents = glm::vec3(1);
	glm::vec3 bodyOffset = glm::vec3(0);

	//Server: what a model vehicle weighs, the way a brick vehicle weighs one per colliding brick
	float bodyMass = 40.0f;

	/*
		Server: what Lua's radiusImpulse pushes it as if it weighed, 0 for its real mass. A model vehicle's mass is picked for how it
		drives (the jeep's 150 is what keeps it planted), which would leave it barely moving where a brick car of a few dozen bricks
		flies, so spawnModelVehicle gives it a weight like a brick car's here instead, see its impulseMass
	*/
	float impulseMass = 0.0f;

	//Whether it's a model rather than bricks
	bool isModelVehicle() const { return bodyTypeID != NO_ID; }

	//Client: the model drawn as a model vehicle's body, moved to getDrawnTransform every frame
	ModelInstance* bodyInstance = nullptr;

	//Client: half the size of a box around everything that model draws, from finishClient, see getCameraDistance
	glm::vec3 bodyDrawnHalfExtents = glm::vec3(0);

	/*
		Client: how far behind its driver the camera wants to sit to keep the whole of it on screen, which
		a plane asks for instead of the follow distance the server picked for someone on foot, see
		PlayerCamera::vehicleDistance. Worked out from how wide and long it is, as it's drawn rather than
		as it collides, since a Blockland shape's collision detail often leaves the wings out
	*/
	float getCameraDistance() const;

	//How many times its own size back that camera sits
	static constexpr float cameraDistanceScale = 2.2f;

	/*
		Animations of a model vehicle's own shape playing on its body, like a plane's propeller: on the
		server what vehicle:playAnimation has looping, in the order they started, and on a client what the
		server last said was looping, played on bodyInstance. The IDs are the body type's model's, which
		both sides read out of the same file, the way a dynamic's are. A brick vehicle has no model to
		play anything on, so nothing here does anything to one
	*/
	std::vector<unsigned char> loopingAnimations;

	//Server: how many more updates repeat the list after a change, since updates go out unreliably
	int loopResends = 0;

	/*
		Server: the last animation vehicle:playAnimation started without a loop, and a count that changes
		each time so playing the same one again plays it again. Client: the last count it acted on
	*/
	int oneShotAnimation = -1;
	unsigned char oneShotCount = 0;
	int oneShotResends = 0;

	//The animation ID meaning none, in a packet where every ID is one byte
	static constexpr unsigned char noAnimation = 255;

	//How many more updates carry an animation after it changes, see Dynamic, which does all of this the same way
	static constexpr int animationResends = 4;

	//Server: starts one looping for everyone, stops one, or stops every one of them for -1
	void startLoop(int id);
	void stopLoop(int id);

	//Server: plays one once for everyone, from its start
	void playOneShot(int id);

	//Client: plays whichever of these its body isn't already, and stops the ones the server no longer has looping
	void syncLoops(const std::vector<unsigned char>& loops);

	//Client: plays or stops one on its body model right now
	void play(int id, bool loop) { if (bodyInstance) bodyInstance->playAnimation(id, loop); }
	void stop(int id) { if (bodyInstance) bodyInstance->stopAnimation(id); }

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

	//Server: the Vehicle Spawn brick that spawned it and spawns another once it's gone, NO_ID for a vehicle that didn't come from one, see BrickAttachments::vehicleSpawnName
	netIDType spawnBrickID = NO_ID;

	//Server: whether Lua's radiusImpulse breaks its bricks off, see vehicle:setDestructable
	bool destructable = false;

	//Server: music from its wrench dialog, and the loop playing it, NO_ID for none
	std::string musicName = "";
	float musicVolume = 1.0f;
	float musicPitch = 1.0f;
	unsigned int musicLoopID = NO_ID;

	//Server: the sound its driver honks with left click, from its wrench dialog, "" for none. A new vehicle gets Honk if there's a sound by that name
	std::string hornName = "";

	/*
		Server: its headlight from its wrench dialog, only the light fields are used, hasLight for whether it has one at all
		The driver switches it on and off with their flashlight key, see setVehicleHeadlightOn in LuaFunctions/VehicleLua.h
		headlightID is the Light shining while it's on, NO_ID while it's off or it has none
	*/
	BrickAttachments headlight;
	bool headlightOn = false;
	netIDType headlightID = NO_ID;

	//Where its headlight sits before its offset, in its body's space: the middle of its front, see buildServer
	glm::vec3 headlightMount = glm::vec3(0);

	//Client: whether it has a headlight and whether that's lit, and whether it flies, from the flags byte of the server's packets
	bool hasHeadlight = false;
	bool headlightLit = false;
	bool fliesForDriver = false;

	//Server: has the next update packet go out even while it sits still, for a change to its flags byte
	void markStateChanged() { stateChanged = true; }

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

	//Client: the model every one of its wheels has an instance of, from finishClient, nullptr without one
	Model* wheelModel = nullptr;

	//Server: makes its body and wheels with its body at origin. bricks, wheels, forward, seat, and steering have to be set first. False if none of its bricks collide
	bool buildServer(const BrickTypes* types, const btVector3& origin);

	//Server: engine, brakes, and steering for the next step from its driver's keys, true if it's going too fast for the engine to push it any faster
	bool drive(bool forwardHeld, bool backwardHeld, bool leftHeld, bool rightHeld, bool brakeHeld);

	//Server: no engine or steering, and every wheel's brakes on, for a vehicle nobody is driving
	void park();

	//Server: no engine, steering, or brakes either, for a vehicle nobody drives that a player is pushing along, see LoopServer::pushVehicle
	void coast();

	/*
		Server: stands it back on its wheels facing the way it was, still, and lifts it clear of whatever it was stuck in,
		for a player left clicking a vehicle nobody is in, see Networking/PacketsFromClient/ClickDetails.cpp
	*/
	void flipUpright();

	//How far above half its height flipUpright lifts it, and the most it lifts it at all, world units
	static constexpr float flipLift = 1.0f;
	static constexpr float maxFlipLift = 10.0f;

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

	/*
		Client: once every brick has arrived, makes its body and has renderer draw its bricks, or for a model vehicle
		makes an instance of its body's model out of dynamicTypes. Each wheel gets an instance of its wheel type's model,
		or of tireModel, either of which can be missing, leaving it without one
	*/
	void finishClient(const BrickTypes* types, InstancedBrickRenderer* _renderer, Model* tireModel, const std::vector<std::shared_ptr<DynamicType>>& dynamicTypes);
	bool hasAllBricks() const { return bricks.size() >= expectedBricks; }

	//The model one of its wheels is drawn with, nullptr without one, see finishClient
	Model* getWheelModel() const { return wheelModel; }

	/*
		Takes some of its bricks out by index (in any order, repeats and ones past the end ignored), rebuilding its body's shape around the rest and on the server its weight,
		and on a client drawing what's left. Its body keeps its old shape if none of the rest collide. A model vehicle has no bricks to take out
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

	//Client: how long the update packet starting at src is, 0 if there aren't that many bytes left in it
	unsigned int readUpdateBytes(const enet_uint8* src, size_t available) const;

	//Client: readUpdateBytes of an update packet
	void readUpdate(const enet_uint8* src, float idealBufferSize);

	//Server: VehicleBricks packets with all of its bricks
	std::vector<ENetPacket*> makeBrickPackets() const;

	//Server: a VehicleBricksBroken packet for bricks an impulse broke off, with how many it had before removeBricks took them out
	ENetPacket* makeBricksBrokenPacket(uint16_t bricksBefore, const std::vector<uint16_t>& indices, const glm::vec3& center, float strength) const;

	//Server: a VehicleDriver packet saying who's driving it and riding on each of its seats
	ENetPacket* makeDriverPacket() const;

	virtual bool requiresNetUpdate() override;

	//The flags byte of creation and update packets: server side from its headlight, client side into hasHeadlight and headlightLit
	unsigned char getFlags() const;
	void readFlags(unsigned char flags);

	virtual unsigned int getCreationPacketBytes() const override;

	virtual unsigned int getUpdatePacketBytes() const override;

	//The first byte of an update is how long to play it back over, see SimObject::scaleUpdateInterval
	//A byte can't say more than 255, and anything that long is treated as "no idea" by the interpolator anyway
	virtual void scaleUpdateInterval(enet_uint8* update, unsigned int multiplier) const override
	{
		update[0] = (enet_uint8)std::min<unsigned int>(update[0] * multiplier, 255u);
	}

	//See SimObject::getNetRelevancePosition
	virtual bool getNetRelevancePosition(glm::vec3& position) const override
	{
		if (!body)
			return false;
		position = b2g3(body->getWorldTransform().getOrigin());
		return true;
	}

	virtual void addToCreationPacket(enet_uint8* dest) const override;

	virtual void addToUpdatePacket(enet_uint8* dest) override;

	~Vehicle();
};

//The vehicle a body belongs to, nullptr if it isn't a vehicle's
std::shared_ptr<Vehicle> vehicleFromBody(const btCollisionObject* body);
