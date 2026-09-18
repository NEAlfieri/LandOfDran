#pragma once

#include "../Networking/ObjHolder.h"
#include "../SimObjects/SimObject.h"
#include "../NetTypes/DynamicType.h"
#include "../Graphics/Interpolator.h"
#include "../Networking/Quantization.h"
#include "../Networking/JoinedClient.h"
#include "../Utility/GlobalStartup.h" //getTicksMS

class Vehicle;

/*
	Dynamics are basically any object that can move around with physics
	Projectiles are dynamics and items and players are child classes of dynamics
*/
class Dynamic : public SimObject
{
	friend ObjHolder<Dynamic>;
	 
	protected:

	/*
		The last transform we sent with addToUpdatePacket
	*/
	btTransform lastSentTransform = btTransform::getIdentity();
	btVector3 lastSentAngVel = btVector3(0, 0, 0);
	btVector3 lastSentVel = btVector3(0, 0, 0);
	
	//Time with SDL_GetTicks that we sent lastSentTransform
	unsigned int lastSentTime = 0;

	/*
		Sender side, see addToUpdatePacket: the last full position we wrote out, as the far end will have decoded it,
		and which generation it was. Positions after it go out as PositionDeltaBytes measured from here for as long as
		they stay in range, see addPositionDelta
	*/
	glm::vec3 keyframePosition = glm::vec3(0, 0, 0);
	unsigned char keyframeGeneration = 0;
	bool hasKeyframe = false;
	//Time with SDL_GetTicks of the keyframe above, so one goes out regularly and a receiver that missed one recovers quickly
	unsigned int lastKeyframeTime = 0;

	/*
		Everything requiresNetUpdate worked out about the update it just asked for, so getUpdatePacketBytes and
		addToUpdatePacket don't each redo the same distance and angle comparisons - ObjHolder calls all three
		once per object per tick, and physics doesn't step in between, see LoopServer::run
	*/
	struct PendingUpdate
	{
		bool posRot = false;
		bool vel = false;
		bool angVel = false;
		bool look = false;
		bool oneShot = false;
		bool loops = false;
		//Whether the position goes out as a delta off keyframePosition rather than a full one, and what it is
		bool positionIsDelta = false;
		glm::vec3 positionDelta = glm::vec3(0, 0, 0);
	};

	PendingUpdate pendingUpdate;

	//Works out pendingUpdate from where the body is now, called by requiresNetUpdate for an update it is about to ask for
	void measurePendingUpdate();

	/*
		Determines its physical appearance and physics properties
	*/
	std::shared_ptr<DynamicType> type = nullptr;

	
	explicit Dynamic(std::shared_ptr<DynamicType> _type, const btVector3& initialPos, const btQuaternion& initialRot);

	//Called after this object has an id, type, me pointer, and vector index assigned by ObjHolder
	virtual void onCreation() override;

	//Client: holds buffer data for instanced mesh rendering
	//Server: Only uses the vectors to store per-node color/hidden data for now
	ModelInstance* modelInstance = nullptr;

	//Called by objHolder when destroy is first called, gives object an oppertunity to reset smart pointers it might have
	virtual void requestDestruction() override;

	//See isInWorld, and the gravity it had before leaving, which rejoining the world would replace with the world's
	bool inWorld = true;
	btVector3 outOfWorldGravity = btVector3(0, 0, 0);

	public:

	//TODO: Just make updating these values on body require going through a setter method that sets these as well
	bool frictionUpdated = false;
	bool gravityUpdated = false;
	bool restitutionUpdated = false;
	bool playWalkingAnimation = false;
	bool forceUpdateAll = false; //Set in requiresNetUpdate, unset in addToUpdatePacket, should be done once per second or so

	//If lua changed the position/velocity/etc of a player controlled object
	bool forcePlayerUpdate = false;

	/*
		If lua changed only the velocity of a player controlled object. Sent as a forced update too, but marked
		DynamicExtra_VelocityOnly so the controlling client takes the velocity without being moved to the position
		the server has for them, which is a round trip old: something setting a player's velocity every tick, like
		a rope swinging them, would otherwise drag them back that far each time
	*/
	bool forcePlayerVelocity = false;

	//What kind of dynamic this is, and what its creation packet carries after a plain dynamic's, see DynamicKind
	virtual DynamicKind getKind() const { return isProjectile ? DynamicKind_Projectile : DynamicKind_Plain; }
	virtual unsigned int getKindCreationBytes() const { return 0; }
	virtual void addKindCreationData(enet_uint8* dest) const {}

	//False while its body is out of the physics world, like an item in someone's inventory: it doesn't collide, fall, or float, and the server sends no updates for it
	bool isInWorld() const { return inWorld; }

	//Takes its body out of the physics world, unsnapping it from any cursor
	void removeFromWorld();

	//Puts its body back into the physics world at transform, not moving, with the gravity it had before
	void returnToWorld(const btTransform& transform);

	//Client: draws it here rather than where physics or the interpolator have it, like an item in someone's hand, for everything that follows where it's drawn too
	void setDrawnTransform(const glm::vec3& position, const glm::quat& rotation);

	//Client: what holds its meshes' transforms and per instance render data, nullptr server side
	ModelInstance* getModelInstance() const { return modelInstance; }

	//Client: plays and stops an animation on its own instance, the walk and a vehicle's sit are done this way on each client
	void play(int id, bool loop) { if (!modelInstance) return; modelInstance->playAnimation(id, loop); }

	void stop(int id) { if (!modelInstance) return;  modelInstance->stopAnimation(id); }

	/*
		Server: starts an animation looping on it for everyone, like Lua's dynamic:playAnimation with loop, or stops one,
		or every one of them for -1. They go into loopingAnimations, which its creation packet carries so a client
		joining later sees them, and which the next few updates repeat after a change, see loopResends
	*/
	void startLoop(int id);
	void stopLoop(int id);

	//Client: plays what the server has looping that isn't yet and stops what it no longer does, leaving what this client plays itself alone
	void syncLoops(const std::vector<unsigned char>& loops);

	//Server: the animations startLoop has looping, in the order they started. Client: the ones the server last said were, see syncLoops
	std::vector<unsigned char> loopingAnimations;

	//Server only, how many more updates carry loopingAnimations, updates are unreliable so a change goes out a few times
	int loopResends = 0;

	/*
		Plays an animation once from the start, like a player's grab
		Client: right away. Server: sends it to clients in the next few updates, see oneShotResends
	*/
	void playOneShot(int id);

	//Server: the last animation playOneShot sent, and a count that changes each time so a repeat plays again
	//Client: the last count we got from the server, so resent updates don't play it again
	int oneShotAnimation = -1;
	unsigned char oneShotCount = 0;

	//Server only, how many more updates carry the one shot animation, updates are unreliable so it goes out a few times
	int oneShotResends = 0;

	/*
		Unit world direction a player looks, which turns the model's Head node, see updateSnapshot
		Set by PlayerController from the camera on the server and for our own player, from updates for everyone else's
	*/
	glm::vec3 lookDirection = glm::vec3(0, 0, -1);
	//Whether anything has set lookDirection, dynamics nobody controls keep their heads still
	bool hasLook = false;

	//Server only, the look direction in the last update we sent
	glm::vec3 lastSentLook = glm::vec3(0, 0, 0);

	/*
		Receiver side: the last full position an update carried for this object and which generation it was, so a
		position delta can be measured back off of it. 255 until we have been sent one, which makes us drop deltas
		rather than apply them to a position we never got, see UpdateSimObjectsPacket::applyPacket
	*/
	glm::vec3 receivedKeyframePosition = glm::vec3(0, 0, 0);
	unsigned char receivedKeyframeGeneration = 255;

	//Server only: makes the next position we send a full one rather than a delta, see requireFullNetUpdate
	bool needsKeyframe = true;

	/*
		Writes the position of an update as either a full keyframe or a delta off the last one, per pendingUpdate,
		stamping which of the two it is and the generation it belongs to into extraFlags
	*/
	void writeUpdatePosition(enet_uint8* dest, const glm::vec3& pos, unsigned char& extraFlags);

	/*
		Receiver side: reads the position out of an update, whichever of the two forms it took, and remembers a
		keyframe for the deltas that follow it
		Returns false for a delta measured from a keyframe we never got, which means we have no idea where this
		object actually is and the caller should leave it where it is rather than put it somewhere wrong
		The caller advances by updatePositionBytes(extraFlags) either way
	*/
	bool readUpdatePosition(enet_uint8 const* src, unsigned char extraFlags, glm::vec3& pos);

	//Client only, the look direction the head is drawn with, following lookDirection smoothly for other players
	glm::vec3 renderedLook = glm::vec3(0, 0, -1);
	bool renderedLookInitialized = false;

	bool getHidden() const { return modelInstance->getHidden(); };

	//See ModelInstance::setHidden
	void setHidden(bool hidden, bool castShadow = false) { modelInstance->setHidden(hidden, castShadow); };

	//Client: world space middle of one of its model's meshes as it's drawn, or where it's drawn for -1
	glm::vec3 getMeshCenter(int meshIndex) const;

	//Client: how one of its model's meshes is turned as it's drawn, or how the whole dynamic is for -1, swimming tilt included
	glm::quat getMeshRotation(int meshIndex) const;

	const std::shared_ptr<DynamicType>& getType() const { return type; }

	//Physics object
	btRigidBody* body = nullptr;

	//Client only, true if object is in Simulation::controlledDynamics
	bool clientControlled = false;

	//Client only, the vehicle it's riding in as of the last LoopClient::placeVehicleDrivers, empty when it isn't in one
	//Riding takes it out of the physics world, which leaves its body's velocity at zero, so its vehicle's is how fast it really moves
	std::weak_ptr<Vehicle> ridingVehicle;

	//Client only, ms timestamp (getTicksMS) until which we should render off the live physics transform instead of the
	//interpolated one - set briefly after locally touching the player's controlled body, so bumping into something feels
	//immediate instead of waiting for the server to notice and broadcast the reaction. Purely a visual prediction; the
	//server remains authoritative and the next real snapshot will correct us if we predicted wrong. See LoopClient::predictLocalCollisions
	unsigned int predictLocallyUntil = 0;

	//Client only, ms timestamp (getTicksMS) of the last time the player's own body was actually touching this object -
	//refreshed every frame contact continues, so it only starts "aging" once contact ends. Used to cap how long
	//continued motion can keep extending predictLocallyUntil *after* losing contact (a chaotic multi-body event can
	//otherwise diverge client vs. server unboundedly), without cutting off a long sustained push while it's still
	//ongoing - see LoopClient::predictLocalCollisions
	unsigned int predictLocallyStartedAt = 0;

	//Client only, whether we rendered off the live physics transform last frame because of predictLocallyUntil -
	//used to notice the exact frame prediction ends so we can hand off smoothly, see handOffFromPrediction
	bool wasPredictingLocally = false;

	//Client only: called the moment local collision prediction ends. Injects our current live transform as a fresh
	//interpolator snapshot so playback continues smoothly from here instead of jumping to whatever (stale, pre-collision)
	//position the interpolator still had buffered from before the server noticed anything
	void handOffFromPrediction(float idealBufferSize);

	//waterLevel is PlayerController::noWater if there's no water, it's for tilting swimming players
	void updateSnapshot(float deltaT, bool forceUsePhysicsTransform, float waterLevel);

	//Client only, turns the model's Head node toward lookDirection, called by updateSnapshot
	void turnHead(float deltaT);

	//Client only
	Interpolator interpolator;

	//Client only: the transform actually drawn, kept separate from whatever updateSnapshot's raw target (interpolated
	//or live physics) says. Rather than assigning the target straight to the model each frame, we smoothly correct
	//toward it - negligible lag for the normal case (the target is already moving smoothly frame to frame, so the
	//correction closes almost the entire gap every frame), but a sudden large jump in the target (e.g. handing off
	//from local prediction to a server position that resolved a collision differently) becomes a brief, smooth
	//glide instead of an instant teleport. See updateSnapshot
	glm::vec3 renderedPosition = glm::vec3(0, 0, 0);
	glm::quat renderedRotation = glm::quat(1, 0, 0, 0);
	bool renderedTransformInitialized = false;

	//Client only: extra rotation on top of renderedRotation that lays a swimming player along the way they're swimming
	//Only the model turns, the collision box stays upright
	glm::quat renderedTilt = glm::quat(1, 0, 0, 0);

	//Client only: which way the tilt points, the body's velocity for our own dynamics, smoothed rendered movement for everything else
	glm::vec3 tiltVelocity = glm::vec3(0, 0, 0);

	//Server only, used to set physics body position
	void setPosition(const btVector3& pos);

	//Server only, used to set physics body linear veclotiy
	void setVelocity(const btVector3& vel);

	//Server only, used to set physics body angular veclotiy
	void setAngularVelocity(const btVector3& vel);

	void activate() const;

	btVector3 getVelocity() const;

	btVector3 getPosition() const;

	btVector3 getAngularVelocity() const;

	virtual bool requiresNetUpdate() override;//const override;

	//How many bytes would this add to a packet creating objects if it was added to it
	virtual unsigned int getCreationPacketBytes() const override;

	//How many bytes would this add to a packet updating objects if it was added to it
	virtual unsigned int getUpdatePacketBytes() const override;

	//The first byte of an update is how long to play it back over, see SimObject::scaleUpdateInterval
	//A byte can't say more than 255, and anything that long is treated as "no idea" by the interpolator anyway
	virtual void scaleUpdateInterval(enet_uint8* update, unsigned int multiplier) const override
	{
		update[0] = (enet_uint8)std::min<unsigned int>(update[0] * multiplier, 255u);
	}

	//A position keyframe is what the deltas after it are measured from, so throttled clients need it even on a tick
	//they would otherwise skip, see SimObject::lastUpdateAnchorsLaterOnes
	virtual bool lastUpdateAnchorsLaterOnes() const override
	{
		return pendingUpdate.posRot && !pendingUpdate.positionIsDelta;
	}

	//A client that has only just been sent this object has no keyframe to measure a position delta from, and none
	//of our lastSent state describes what it holds, so start it over from a full one
	virtual void requireFullNetUpdate() override
	{
		needsKeyframe = true;
		forceUpdateAll = true;
	}

	//An object out of the physics world - a carried item - has a stale body position, and the little it still sends
	//(where its holder looks, grabs) has to reach everyone, so it stays unthrottled
	virtual bool getNetRelevancePosition(glm::vec3& position) const override
	{
		if (!inWorld || !body)
			return false;
		position = b2g3(body->getWorldTransform().getOrigin());
		return true;
	}

	//Add getCreationPacketBytes() worth of data to the given packet with all the data needed for the client to create it
	virtual void addToCreationPacket(enet_uint8 * dest) const override;

	//Add getUpdatePacketBytes() worth of data to the given packet with all the data needed for the client to update it
	virtual void addToUpdatePacket(enet_uint8 * dest) override;

	//Client side: 
	void setMeshColor(int idx, const glm::vec4& color);

	//Server side: returns a fully created packet ready to broadcast to relay the mesh color update
	ENetPacket* setMeshColor(const std::string& meshName, const glm::vec4& color);

	//Client side: shows a decal (a layer of the decal array, see ClientProgramData::getDecal) on a mesh, -1 for none
	void setMeshDecal(int meshIdx, int decalId);

	//Server side: puts the face or shirt with that file name in Assets/faces or Assets/shirts on a mesh, empty for none
	//Returns a fully created packet ready to broadcast, or nullptr if the model has no mesh by that name
	ENetPacket* setMeshDecal(const std::string& meshName, const std::string& decalName);

	//Server side: decal file names by mesh index, for creation packets
	std::map<int, std::string> meshDecals;

	/*
		Models worn on this one, by slot: a player's hat from their appearance editor goes in the "hat" slot, see
		PlayerAppearance::hatSlot. Each is a model with attach lines saying which mesh of this one it sits on and
		how, see Model::attachMesh, is painted one color like a mesh, alpha 0 for its own look, and is sized by
		a multiple of the size its descriptor gives it
		Server side: the descriptor's file name in PlayerAppearance::partsFolder, its color, and its size, for creation packets
	*/
	struct PartChoice
	{
		std::string name = "";
		glm::vec4 color = glm::vec4(0);
		float scale = 1.0f;
	};
	std::map<std::string, PartChoice> parts;

	//Longer slot names are cut down to this, so one packet always holds a whole one
	static constexpr size_t maxSlotLength = 32;

	//Server side: puts the part with that file name in a slot, or takes the slot's part off for an empty name
	//Returns a fully created packet ready to broadcast, see DynamicPartPacket, or nullptr for an empty slot name
	ENetPacket* setPart(const std::string& slot, const std::string& partName, const glm::vec4& color, float scale = 1.0f);

	//Client side: an instance of each worn model by slot, put on this one's model each frame by placeParts
	struct MountedPart
	{
		Model* model = nullptr;
		ModelInstance* instance = nullptr;
		float scale = 1.0f;
	};
	std::map<std::string, MountedPart> mountedParts;

	//Client side: wears an instance of model in a slot, painted color and sized scale, or takes the slot's part off for nullptr, like a hat this game doesn't have
	void setPart(const std::string& slot, Model* model, const glm::vec4& color, float scale = 1.0f);

	//Client side: moves each worn model to where it sits on this one's model as drawn, call each frame after this one's model is updated and before theirs are
	void placeParts();

	//Applies (or, if color.a <= 0, clears) an outline/highlight effect on this object. Used both client-side when
	//applying a packet and server-side for bookkeeping so late-joining clients get it baked into their creation packet
	void setHighlight(const glm::vec4& color, float thickness);

	/*
		Text drawn floating over it on clients, empty for none, and the color it's drawn in, see dynamic:setNameTag in Lua
		Players get their name put here as they join, see serverstart.lua, and LoopClient::updateNameTags draws them
	*/
	std::string nameTag = "";
	glm::vec3 nameTagColor = glm::vec3(1, 1, 1);

	//Longer text is cut down to this, so one packet always holds a whole tag
	static constexpr size_t maxNameTagLength = 64;

	//Sets nameTag (cut to maxNameTagLength) and its color, both server and client side
	void setNameTag(const std::string& text, const glm::vec3& color);

	//Server side: returns a fully created packet ready to broadcast with the current name tag, see NameTagPacket
	ENetPacket* makeNameTagPacket() const;

	//Server side: returns a fully created packet ready to broadcast to relay the highlight update, does not apply it locally
	ENetPacket* makeHighlightPacket(const glm::vec4& color, float thickness) const;

	/*
		Server only: cursor-snapping (see dynamic:snapToCursor in Lua). While snapped, LoopServer drives this
		object's position every tick from the owning client's cached camera position/direction instead of physics.
	*/

	//Non-owning: does not keep the client alive. Gravity is disabled while snapped, so this also flags "is snapped"
	std::weak_ptr<JoinedClient> snappedToClient;
	//View-space offset used while snapped: x = right, y = up, z = forward (away from the camera)
	glm::vec3 snapOffset = glm::vec3(0, 0, 0);
	//Gravity from just before snapping, restored on unsnap
	btVector3 preSnapGravity = btVector3(0, 0, 0);

	bool isSnappedToCursor() const { return !snappedToClient.expired(); }

	//Attach to a client's cursor. Disables gravity until unsnapFromCursor() is called (or the client disconnects)
	void snapToCursor(std::shared_ptr<JoinedClient> client, const glm::vec3& offset);

	//Detach from the cursor if snapped, restoring the gravity it had before snapping. No-op if not snapped
	void unsnapFromCursor();

	//Server only, called once per tick by LoopServer for each snapped dynamic with its owner's current camera state
	void updateCursorSnapPosition(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection);

	/*
		Set by Lua's addProjectile. The first time it touches anything that collides, LoopServer::updateProjectiles
		fires ProjectileHit with its tag and removes it. Until then it's turned every tick to point its model's +Y the way it's going
		Clients learn it from the creation packet, and their copy never pushes or gets pushed by anything, see AddSimObjects
	*/
	bool isProjectile = false;
	std::string projectileTag = "";

	//Server only: the dynamic that fired a projectile, which it passes through, and that dynamic's body, so passing through it can be undone once it's gone
	std::weak_ptr<Dynamic> projectileShooter;
	btRigidBody* ignoredShooterBody = nullptr;

	//Server only: a touch of this projectile was already noted this frame, see LoopServer::recordProjectileHits
	bool projectileHitRecorded = false;

	//Server only: turns it so its model's forward (+Y, or -Z for a DTS shape, see Model::projectileForward) points along its velocity, with no spin, unless it's too slow to have a steady heading
	void faceVelocity();

	/*
		How hard water pushes this up, as a multiple of its weight when it's all the way under: 0 sinks, 1 hangs in place,
		more floats with less of it under. Sent to clients so the ones simulating it in water match the server
	*/
	float buoyancy = 1.3f;

	//Once this much of a player's height is under water they swim, see PlayerController
	static constexpr float swimDepth = 0.5f;

	//Server side: returns a fully created packet ready to broadcast to relay the current buoyancy, see DynamicBuoyancyPacket
	ENetPacket* makeBuoyancyPacket() const;

	//How much of its height is below waterLevel, 0 to 1
	btScalar getSubmergedFraction(float waterLevel) const;

	//Buoyancy and water drag for the next physics step, does nothing if no part of it is below waterLevel
	void applyWaterForces(float waterLevel, float deltaT);

	//Server only, for splash sounds, see LoopServer::playWaterSounds
	bool inWater = false;
	unsigned int lastWaterSoundMS = 0;

	//Client only, for water ripples, see LoopClient::makeWaterRipples
	bool rippleInWater = false;
	bool rippleStateKnown = false;
	//How far it's moved along the surface since the last wake ripple
	float rippleWakeDistance = 0;

	~Dynamic();
};

//Helper function to get a normal dynamic shared_ptr from its btRigidBody with a ton of error checking, returns nullptr on error
std::shared_ptr<Dynamic> dynamicFromBody(const btRigidBody* in);

