#include "Vehicle.h"
#include "../Graphics/InstancedBrickRenderer.h"
#include "../Bricks/BrickHolder.h"
#include <algorithm>

#include <glm/gtc/constants.hpp>

//The engine stops pushing past this speed, world units per second, like the old game
static constexpr float maxDriveSpeed = 200.0f;

//Wheels on the ground throw dirt going faster than this while turning or braking, like the old game
static constexpr float dirtSpeedKmHour = 50.0f;

//How hard a wheel's suspension can push, Bullet's default of 6000 left anything over a few hundred bricks sitting on the ground
static constexpr float maxSuspensionForce = 10000000.0f;

//The old game popped new cars upward so bricks sitting on the ground don't start stuck in it
static constexpr float spawnUpwardSpeed = 20.0f;

//Net ID, position, rotation, brick offset, forward, seat, exit height, brick count, driver, dirt emitter type,
//body and wheel type, body box, headlight mount, flags, wheel count
static constexpr unsigned int creationHeaderBytes = sizeof(netIDType) + PositionBytes + QuaternionBytes + sizeof(float) * 10 + sizeof(uint16_t) + sizeof(netIDType) + sizeof(uint16_t) + sizeof(netIDType) * 2 + sizeof(float) * 9 + 1 + 1;

//Milliseconds since the last update, position, rotation, velocity as full floats since cars go faster than dynamics' quantized velocity reaches, flags
static constexpr unsigned int updateHeaderBytes = 1 + PositionBytes + QuaternionBytes + sizeof(float) * 3 + 1;

//Packet type, vehicle net ID, u16 index of the first brick, u16 count
static constexpr unsigned int brickPacketHeaderBytes = 1 + sizeof(netIDType) + sizeof(uint16_t) * 2;

//Per second, how quickly where it's drawn catches up to the interpolator, same as dynamics
static constexpr float correctionRate = 15.0f;

/*
	How hard a plane turns toward where its pilot looks: the command is the sine of the angle its nose is
	off by times this, so anything more than about thirty degrees off asks for everything it has
*/
static constexpr float aimGain = 2.0f;

//And how hard it rolls toward the bank it wants, the same way around how many radians off it is
static constexpr float levelGain = 2.0f;

namespace
{
	//Closest hit that isn't the vehicle itself, something that doesn't collide, or debris
	struct VehicleRayCallback : public btCollisionWorld::ClosestRayResultCallback
	{
		const btCollisionObject* chassis;

		VehicleRayCallback(const btVector3& from, const btVector3& to, const btCollisionObject* _chassis) : ClosestRayResultCallback(from, to), chassis(_chassis)
		{
			m_collisionFilterMask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::DebrisFilter;
		}

		bool needsCollision(btBroadphaseProxy* proxy) const override
		{
			const btCollisionObject* object = (const btCollisionObject*)proxy->m_clientObject;
			if (object == chassis || !object->hasContactResponse())
				return false;
			return ClosestRayResultCallback::needsCollision(proxy);
		}
	};

	//Bullet's default raycaster casts from inside the vehicle's own bricks, where wheels would land on the vehicle itself
	class VehicleRaycaster : public btVehicleRaycaster
	{
		btDynamicsWorld* world;
		const btCollisionObject* chassis;

		public:

		VehicleRaycaster(btDynamicsWorld* _world, const btCollisionObject* _chassis) : world(_world), chassis(_chassis)
		{
		}

		void* castRay(const btVector3& from, const btVector3& to, btVehicleRaycasterResult& result) override
		{
			VehicleRayCallback callback(from, to, chassis);
			world->rayTest(from, to, callback);

			if (!callback.hasHit())
				return nullptr;

			const btRigidBody* hit = btRigidBody::upcast(callback.m_collisionObject);
			if (!hit)
				return nullptr;

			result.m_hitPointInWorld = callback.m_hitPointWorld;
			result.m_hitNormalInWorld = callback.m_hitNormalWorld.normalized();
			result.m_distFraction = callback.m_closestHitFraction;
			return (void*)hit;
		}
	};
}

Vehicle::Vehicle()
{
	//See the same line in Dynamic's constructor
	lastSentTime = getTicksMS();
}

bool Vehicle::buildShape(const BrickTypes* types)
{
	shape = new btCompoundShape();

	//A model vehicle is one box around its model instead, which is the whole of its collision
	if (isModelVehicle())
	{
		btBoxShape* box = new btBoxShape(g2b3(glm::max(bodyHalfExtents, glm::vec3(0.01f))));
		ownedShapes.push_back(box);

		btTransform child = btTransform::getIdentity();
		child.setOrigin(g2b3(bodyOffset));
		shape->addChildShape(child, box);
		return true;
	}

	std::unordered_map<unsigned int, btBoxShape*> boxes;

	for (const Brick& brick : bricks)
	{
		if (!brick.collides)
			continue;

		btTransform child = btTransform::getIdentity();
		child.setOrigin(g2b3(brickOffset + brick.getWorldCenter()));

		//Like BrickHolder::createBody: special shapes are built unturned, boxes already have their footprint swapped
		const SpecialBrickType* special = brick.isSpecial() && types ? types->getSpecial(brick.typeID - 1) : nullptr;
		btCollisionShape* part = nullptr;
		if (special && special->shape)
		{
			part = special->shape;
			child.setRotation(btQuaternion(btVector3(0, 1, 0), brick.getAngle()));
		}
		else
		{
			btBoxShape*& box = boxes[brick.footprintWidth() | (brick.height << 8) | (brick.footprintLength() << 16)];
			if (!box)
			{
				box = new btBoxShape(btVector3(brick.footprintWidth() * STUD_SIZE, brick.height * PLATE_SIZE, brick.footprintLength() * STUD_SIZE) * 0.5f);
				ownedShapes.push_back(box);
			}
			part = box;
		}

		shape->addChildShape(child, part);
	}

	if (shape->getNumChildShapes() > 0)
		return true;

	delete shape;
	shape = nullptr;
	return false;
}

bool Vehicle::buildServer(const BrickTypes* types, const btVector3& origin)
{
	if (body || !buildShape(types))
		return false;

	//Like the old game, the body weighs one per brick while each brick adds its steering mass setting to how hard it is to turn
	int children = shape->getNumChildShapes();
	btScalar mass = (btScalar)children;
	btVector3 inertia;

	if (isModelVehicle())
	{
		//One box, so there is nothing to spread a per brick weight over
		mass = std::max(bodyMass, 0.01f);
		shape->calculateLocalInertia(mass, inertia);
	}
	else
	{
		std::vector<btScalar> masses(children, steering.mass);
		btTransform principal;
		shape->calculatePrincipalAxisTransform(masses.data(), principal, inertia);
	}

	btRigidBody::btRigidBodyConstructionInfo info(mass, nullptr, shape, inertia);
	info.m_startWorldTransform.setIdentity();
	info.m_startWorldTransform.setOrigin(origin);
	info.m_friction = 0.9f;

	body = new btRigidBody(info);
	body->setUserIndex(vehicleBody);
	body->setUserPointer((void*)new std::shared_ptr<SimObject>(getMe()));
	body->setDamping(0, steering.angularDamping);
	world->addBody(body);

	btVector3 aabbMin, aabbMax;
	shape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);
	exitHeight = (aabbMax.y() - aabbMin.y()) * 0.5f + 0.1f;

	//Its headlight hangs off the middle of its front, where it isn't buried in its own body
	glm::vec3 low = b2g3(aabbMin);
	glm::vec3 high = b2g3(aabbMax);
	headlightMount = (low + high) * 0.5f;
	for (int axis = 0; axis < 3; axis++)
	{
		if (forward[axis] > 0.5f)
			headlightMount[axis] = high[axis];
		else if (forward[axis] < -0.5f)
			headlightMount[axis] = low[axis];
	}

	raycaster = new VehicleRaycaster(world->getDynamicsWorld(), body);

	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_maxSuspensionForce = maxSuspensionForce;
	raycastVehicle = new btRaycastVehicle(tuning, body, raycaster);

	bool alongX = std::abs(forward.x) > 0.5f;
	raycastVehicle->setCoordinateSystem(alongX ? 2 : 0, 1, alongX ? 0 : 2);

	//Bullet rolls a wheel toward its axle crossed with down, so this axle has positive engine force drive forward
	btVector3 down(0, -1, 0);
	btVector3 axle = down.cross(g2b3(forward));

	for (VehicleWheel& wheel : wheels)
	{
		const WheelSettings& settings = wheel.settings;
		btWheelInfo& added = raycastVehicle->addWheel(g2b3(wheel.connection), down, axle, settings.suspensionLength, wheel.radius, tuning, std::abs(settings.steerAngle) > 0.01f);
		added.m_suspensionStiffness = settings.suspensionStiffness;
		added.m_wheelsDampingCompression = settings.dampingCompression;
		added.m_wheelsDampingRelaxation = settings.dampingRelaxation;
		added.m_frictionSlip = settings.frictionSlip;
		added.m_rollInfluence = settings.rollInfluence;
		wheel.suspension = settings.suspensionLength;
	}

	world->addAction(raycastVehicle);

	wheelInWater.assign(wheels.size(), false);
	lastSplashMS.assign(wheels.size(), 0);

	//A model vehicle is put exactly where a script asked for, so there is nothing for it to start stuck in
	if (!isModelVehicle())
		body->setLinearVelocity(btVector3(0, spawnUpwardSpeed, 0));

	return true;
}

void VehicleFlight::clampValues()
{
	thrust = std::clamp(thrust, 0.0f, 10000.0f);
	reverseThrust = std::clamp(reverseThrust, 0.0f, 10000.0f);
	maxSpeed = std::clamp(maxSpeed, 1.0f, 1000.0f);
	maxReverseSpeed = std::clamp(maxReverseSpeed, 0.0f, 1000.0f);
	liftSpeed = std::clamp(liftSpeed, 1.0f, 1000.0f);
	maxLift = std::clamp(maxLift, 0.0f, 100.0f);
	stallSpeed = std::clamp(stallSpeed, 0.1f, 1000.0f);
	angleLift = std::clamp(angleLift, 0.0f, 50.0f);
	stallAngle = std::clamp(stallAngle, 0.01f, 1.5f);

	//A damping rate past 50 would take more than all of the slip away in one substep and bounce it back
	wingDamping = std::clamp(wingDamping, 0.0f, 50.0f);
	finDamping = std::clamp(finDamping, 0.0f, 50.0f);

	dragSpeed = std::clamp(dragSpeed, 1.0f, 10000.0f);
	pitchRate = std::clamp(pitchRate, 0.0f, 20.0f);
	yawRate = std::clamp(yawRate, 0.0f, 20.0f);
	rollRate = std::clamp(rollRate, 0.0f, 20.0f);
	response = std::clamp(response, 0.01f, 10.0f);
	levelRate = std::clamp(levelRate, 0.0f, 20.0f);
	turnBank = std::clamp(turnBank, 0.0f, 1.4f);
}

void Vehicle::flyStep(float deltaT)
{
	if (!flight.enabled || !body || deltaT <= 0.0f || body->getInvMass() <= 0.0f)
		return;

	const btMatrix3x3& basis = body->getWorldTransform().getBasis();

	//Its nose, its own up, and its right, which is the way the two of those leave over: see LuaAPI.md on flying
	btVector3 nose = (basis * g2b3(glm::normalize(forward))).normalized();
	btVector3 up = (basis * btVector3(0, 1, 0)).normalized();
	btVector3 right = nose.cross(up);

	btVector3 velocity = body->getLinearVelocity();
	float airspeed = velocity.dot(nose);
	float speed = velocity.length();

	float mass = 1.0f / body->getInvMass();
	float weight = mass * body->getGravity().length();

	/*
		Everything the air does to it fades away as it slows down, so a plane sitting on a runway is an
		ordinary vehicle on wheels: no lift, no wings biting, and nothing steering how it is turned
	*/
	float authority = std::clamp(std::abs(airspeed) / flight.stallSpeed, 0.0f, 1.0f);
	bool driven = lastInput.driven;

	//The throttle, along its nose, which stops pushing once it is going as fast as it goes
	if (driven)
	{
		float accel = 0.0f;
		if (lastInput.forward && airspeed < flight.maxSpeed)
			accel = flight.thrust;
		else if (lastInput.backward && airspeed > -flight.maxReverseSpeed)
			accel = -flight.reverseThrust;

		if (accel != 0.0f)
			body->applyCentralImpulse(nose * (accel * mass * deltaT));
	}

	/*
		The sine of its angle of attack: how far its nose is above the way it's actually going, which is
		what its wings care about, worked out here because its controls want it too
	*/
	float attack = speed > 0.01f ? std::clamp(-up.dot(velocity) / speed, -1.0f, 1.0f) : 0.0f;

	/*
		Lift, out of the top of its wings rather than along the world's up, so a banked plane's wings
		carry it around the turn. Square to the way it's actually going rather than exactly along its own
		up: lift is a turn of the air going past, so it can't add to how fast the plane is going, and a
		wing pointed straight up while the plane climbs would do exactly that - it fed a level nosed
		plane into a climb that never ran out of speed, since every stud it rose was lift doing work
	*/
	if (weight > 0.0f)
	{
		btVector3 liftDirection = up;

		if (speed > 0.01f)
		{
			liftDirection = up - velocity * (up.dot(velocity) / (speed * speed));
			if (liftDirection.length2() < 0.0001f)
				liftDirection = up;
			else
				liftDirection.normalize();
		}

		/*
			The wings make more of it the further the air comes from under them, up to the stall angle,
			and less and less past that until there's none of it left, see angleLift
		*/
		float stall = std::sin(flight.stallAngle);
		float reach = std::abs(attack) <= stall ? std::abs(attack) : std::max(0.0f, stall * 2.0f - std::abs(attack));
		float shape = std::max(0.0f, 1.0f + flight.angleLift * (attack < 0.0f ? -reach : reach));

		float ratio = airspeed / flight.liftSpeed;
		float lift = std::min(ratio * ratio * shape, flight.maxLift) * weight * authority;
		body->applyCentralImpulse(liftDirection * (lift * deltaT));
	}

	//The wings and the tail fin taking away whatever it is doing that isn't flying along its nose
	btVector3 surface = up * (-velocity.dot(up) * flight.wingDamping) + right * (-velocity.dot(right) * flight.finDamping);
	body->applyCentralImpulse(surface * (mass * authority * deltaT));

	//Drag, along the way it is actually going, which is what stops a dive running away
	if (speed > 0.01f && weight > 0.0f)
	{
		float ratio = speed / flight.dragSpeed;
		body->applyCentralImpulse(velocity * (-ratio * ratio * weight * deltaT / speed));
	}

	/*
		Turning. A torque would have to go through the body's inertia tensor before it meant anything in
		radians a second, and that tensor is whatever the shape of the plane happens to be, so instead its
		controls steer its spin itself: each of its own three axes is moved toward the rate the controls
		are asking for, getting no further than response seconds' worth of the way there each substep. The
		same line of code is its rotational drag, since letting go asks for a rate of zero
	*/
	float control = driven ? authority : authority * 0.5f;
	if (control <= 0.001f)
	{
		if (driven)
			body->activate();
		return;
	}

	//Where the nose is being asked to point: where the pilot looks, or, with nobody flying it, wherever it's already going
	btVector3 aim(0, 0, 0);
	bool haveAim = false;
	if (driven && lastInput.hasLook)
	{
		aim = g2b3(lastInput.look);
		haveAim = aim.length2() > 0.0001f;
		if (haveAim)
			aim.normalize();
	}
	else if (!driven && speed > 1.0f)
	{
		aim = velocity / speed;
		haveAim = true;
	}

	float pitchTarget = 0.0f;
	float yawTarget = 0.0f;
	float yawAsked = 0.0f;

	if (haveAim)
	{
		//One turn taking its nose onto the aim, right hand rule, which pitch and yaw are just the two halves of
		btVector3 toAim = nose.cross(aim);
		pitchTarget = std::clamp(toAim.dot(right) * aimGain, -1.0f, 1.0f) * flight.pitchRate;
		yawAsked = std::clamp(toAim.dot(up) * aimGain, -1.0f, 1.0f);
		yawTarget = yawAsked * flight.yawRate;
	}

	/*
		The wings won't be pulled harder than they can hold: as the angle of attack comes up on the stall
		angle the elevator stops answering, so yanking the view around at speed makes the plane groan
		around the corner rather than snap into a stall it can't see coming. Pushing the other way is
		still free, which is how a pilot gets out of one
	*/
	float limit = std::sin(flight.stallAngle);
	if (limit > 0.0001f)
	{
		float over = (std::abs(attack) - limit * 0.8f) / (limit * 0.2f);
		if (over > 0.0f && (attack > 0.0f) == (pitchTarget > 0.0f))
			pitchTarget *= std::clamp(1.0f - over, 0.0f, 1.0f);
	}

	float rollTarget = 0.0f;
	if (driven && (lastInput.left || lastInput.right))
		rollTarget = (lastInput.right ? 1.0f : -1.0f) * flight.rollRate;
	else if (flight.levelRate > 0.0f)
	{
		/*
			How far it's banked over, and how far it wants to be: into the turn it's being asked for, so
			its wings pull it around rather than its tail dragging it around sideways, and level when it
			isn't turning. Turning left is a positive yaw, which wants the left wing down, hence the sign

			The bank is a real angle rather than its sine, because the sine comes back down again past
			ninety degrees: measured that way a hard turn's roll never reaches the bank it was asked for
			and carries on over onto its back
		*/
		btVector3 level = btVector3(0, 1, 0) - nose * nose.dot(btVector3(0, 1, 0));
		if (level.length2() > 0.0001f)
		{
			level.normalize();
			float upright = up.dot(level);
			float banked = std::atan2(level.cross(up).dot(nose), upright);

			//Past its back it wants nothing: a plane rolled inverted stays there until its pilot rolls it out
			if (upright >= 0.0f)
			{
				float wanted = -yawAsked * flight.turnBank;
				rollTarget = std::clamp((wanted - banked) * levelGain, -1.0f, 1.0f) * flight.levelRate;
			}
		}
	}

	btVector3 spin = body->getAngularVelocity();
	auto steer = [&](const btVector3& axis, float target, float rate)
	{
		float step = std::max(rate, 0.1f) / flight.response * deltaT * control;
		float change = std::clamp(target * control - spin.dot(axis), -step, step);
		spin += axis * change;
	};

	steer(right, pitchTarget, flight.pitchRate);
	steer(up, yawTarget, flight.yawRate);
	steer(nose, rollTarget, flight.rollRate);

	body->setAngularVelocity(spin);
	body->activate();
}

bool Vehicle::drive(bool forwardHeld, bool backwardHeld, bool leftHeld, bool rightHeld, bool brakeHeld)
{
	if (!raycastVehicle)
		return false;

	throwsDirt = brakeHeld || leftHeld || rightHeld;

	float steer = leftHeld ? -1.0f : (rightHeld ? 1.0f : 0.0f);
	float engine = forwardHeld ? 1.0f : (backwardHeld ? -1.0f : 0.0f);
	bool speeding = body->getLinearVelocity().length() > maxDriveSpeed;

	for (int a = 0; a < (int)wheels.size(); a++)
	{
		const WheelSettings& settings = wheels[a].settings;
		raycastVehicle->setBrake(brakeHeld ? settings.brakeForce : 0.0f, a);

		/*
			Negated because a positive steering value turns a wheel toward the negative side of its axle,
			and our axles point the other way: down crossed with the way it drives. Measured the same for
			a vehicle driving along +x, -x, +z, or -z, so it isn't about which way one faces. Without this
			a wheel set to the dialog's default steering turns left when its driver holds right, which is
			why vehicles saved before this had to be given a negative steering angle to drive properly
		*/
		raycastVehicle->setSteeringValue(-steer * settings.steerAngle, a);
		raycastVehicle->applyEngineForce(speeding ? 0.0f : settings.engineForce * engine, a);
	}

	if (forwardHeld || backwardHeld || leftHeld || rightHeld || brakeHeld)
		body->activate();

	return speeding;
}

void Vehicle::coast()
{
	if (!raycastVehicle)
		return;

	throwsDirt = false;

	for (int a = 0; a < (int)wheels.size(); a++)
	{
		raycastVehicle->setBrake(0.0f, a);
		raycastVehicle->setSteeringValue(0.0f, a);
		raycastVehicle->applyEngineForce(0.0f, a);
	}
}

void Vehicle::park()
{
	if (!raycastVehicle)
		return;

	throwsDirt = false;

	//The old game let empty cars roll on forever, with nothing to stop them on flat ground
	for (int a = 0; a < (int)wheels.size(); a++)
	{
		raycastVehicle->setBrake(wheels[a].settings.brakeForce, a);
		raycastVehicle->setSteeringValue(0.0f, a);
		raycastVehicle->applyEngineForce(0.0f, a);
	}
}

void Vehicle::flipUpright()
{
	if (!body)
		return;

	btTransform transform = body->getWorldTransform();

	//The way it drives, flattened, so a car rolled onto its roof comes back up still pointing where it was
	btVector3 facing = transform.getBasis() * g2b3(glm::normalize(forward));
	facing.setY(0);
	if (facing.length2() < 0.0001f)
		facing = btVector3(0, 0, 1);
	facing.normalize();

	//Maps the way it drives and its own up onto that flattened direction and world up. forward is along one of its body's axes, so the two are square to each other
	btVector3 localForward = g2b3(glm::normalize(forward));
	btVector3 localRight = localForward.cross(btVector3(0, 1, 0));
	btVector3 right = facing.cross(btVector3(0, 1, 0));

	btMatrix3x3 fromLocal(localForward.x(), 0, localRight.x(),
						  localForward.y(), 1, localRight.y(),
						  localForward.z(), 0, localRight.z());
	btMatrix3x3 toWorld(facing.x(), 0, right.x(),
						facing.y(), 1, right.y(),
						facing.z(), 0, right.z());
	transform.setBasis(toWorld * fromLocal.transpose());

	//Upright it is as tall as its body's y, so half of that plus a little puts whatever was underground back above it
	float lift = flipLift;
	if (shape)
	{
		btVector3 low, high;
		shape->getAabb(btTransform::getIdentity(), low, high);
		lift = std::clamp((float)(high.y() - low.y()) * 0.5f + flipLift, flipLift, maxFlipLift);
	}
	transform.setOrigin(transform.getOrigin() + btVector3(0, lift, 0));

	body->setWorldTransform(transform);
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	if (raycastVehicle)
		raycastVehicle->resetSuspension();
	body->activate();
}

void Vehicle::startLoop(int id)
{
	//A packet says how many loop at all in one byte, and nothing needs anywhere near this many
	if (id < 0 || id >= noAnimation || loopingAnimations.size() >= 32)
		return;

	if (std::find(loopingAnimations.begin(), loopingAnimations.end(), (unsigned char)id) != loopingAnimations.end())
		return;

	loopingAnimations.push_back((unsigned char)id);
	loopResends = animationResends;
	markStateChanged();
}

void Vehicle::stopLoop(int id)
{
	size_t before = loopingAnimations.size();
	if (id < 0)
		loopingAnimations.clear();
	else
		loopingAnimations.erase(std::remove(loopingAnimations.begin(), loopingAnimations.end(), (unsigned char)id), loopingAnimations.end());

	if (loopingAnimations.size() == before)
		return;

	loopResends = animationResends;
	markStateChanged();
}

void Vehicle::playOneShot(int id)
{
	if (id < 0 || id >= noAnimation)
		return;

	oneShotAnimation = id;
	oneShotCount++;
	oneShotResends = animationResends;
	markStateChanged();
}

void Vehicle::syncLoops(const std::vector<unsigned char>& loops)
{
	for (unsigned char id : loops)
	{
		if (std::find(loopingAnimations.begin(), loopingAnimations.end(), id) == loopingAnimations.end())
			play(id, true);
	}

	for (unsigned char id : loopingAnimations)
	{
		if (std::find(loops.begin(), loops.end(), id) == loops.end())
			stop(id);
	}

	loopingAnimations = loops;
}

void Vehicle::updateWheelStates()
{
	if (!raycastVehicle)
		return;

	bool fast = std::abs(raycastVehicle->getCurrentSpeedKmHour()) > dirtSpeedKmHour;

	for (int a = 0; a < (int)wheels.size(); a++)
	{
		const btWheelInfo& info = raycastVehicle->getWheelInfo(a);
		VehicleWheel& wheel = wheels[a];
		wheel.steering = info.m_steering;
		wheel.suspension = info.m_raycastInfo.m_suspensionLength;
		wheel.contact = info.m_raycastInfo.m_isInContact;
		wheel.dirt = fast && throwsDirt && wheel.contact;
	}
}

btTransform Vehicle::getWheelTransform(int wheel) const
{
	if (!raycastVehicle || wheel < 0 || wheel >= raycastVehicle->getNumWheels())
		return body ? body->getWorldTransform() : btTransform::getIdentity();

	return raycastVehicle->getWheelInfo(wheel).m_worldTransform;
}

void Vehicle::getBodyTransform(bool drawn, glm::vec3& origin, glm::quat& rotation) const
{
	origin = renderedPosition;
	rotation = renderedRotation;
	if (!drawn && body)
	{
		const btTransform& transform = body->getWorldTransform();
		btQuaternion turn = transform.getRotation();
		origin = b2g3(transform.getOrigin());
		rotation = glm::quat(turn.w(), turn.x(), turn.y(), turn.z());
	}
}

btTransform Vehicle::getSeatTransform(bool drawn) const
{
	glm::vec3 origin;
	glm::quat rotation;
	getBodyTransform(drawn, origin, rotation);

	//Player models face -Z
	glm::quat facing = rotation * glm::angleAxis(std::atan2(-forward.x, -forward.z), glm::vec3(0, 1, 0));
	glm::vec3 position = origin + rotation * seat;
	return btTransform(btQuaternion(facing.x, facing.y, facing.z, facing.w), g2b3(position));
}

btTransform Vehicle::getPassengerTransform(int seatIndex, const Dynamic& rider, const glm::vec3& look, bool drawn) const
{
	if (seatIndex < 0 || seatIndex >= (int)passengerSeats.size())
		return getSeatTransform(drawn);

	glm::vec3 origin;
	glm::quat rotation;
	getBodyTransform(drawn, origin, rotation);

	//Looking straight up or down, they face the way it drives, and sitting in a model vehicle they always do, only their head turns
	glm::vec3 local = glm::inverse(rotation) * look;
	local.y = 0.0f;
	if (isModelVehicle() || glm::length(local) < 0.001f || glm::any(glm::isnan(local)))
		local = forward;

	//Player models face -Z
	glm::quat facing = rotation * glm::angleAxis(std::atan2(-local.x, -local.z), glm::vec3(0, 1, 0));

	//The bottom of its collision box on the seat's top
	std::shared_ptr<Model> model = rider.getType()->getModel();
	float feetToOrigin = model->getColHalfExtents().y - model->getColOffset().y;
	glm::vec3 position = origin + rotation * (passengerSeats[seatIndex].top + glm::vec3(0, feetToOrigin, 0));

	return btTransform(btQuaternion(facing.x, facing.y, facing.z, facing.w), g2b3(position));
}

int Vehicle::findFreeSeat(const glm::vec3& targetPos) const
{
	glm::vec3 origin;
	glm::quat rotation;
	getBodyTransform(false, origin, rotation);

	int best = -1;
	float bestDistance = 0.0f;
	for (int a = 0; a < (int)passengerSeats.size(); a++)
	{
		if (passengerSeats[a].riderID != NO_ID || passengerSeats[a].broken)
			continue;

		float distance = glm::distance2(origin + rotation * passengerSeats[a].top, targetPos);
		if (best == -1 || distance < bestDistance)
		{
			best = a;
			bestDistance = distance;
		}
	}
	return best;
}

void Vehicle::finishClient(const BrickTypes* types, InstancedBrickRenderer* _renderer, Model* tireModel, const std::vector<std::shared_ptr<DynamicType>>& dynamicTypes)
{
	if (brickGroup != -1 || body || bodyInstance)
		return;

	//Whichever of the types this vehicle asked for the server has actually sent us
	auto modelOfType = [&dynamicTypes](netIDType typeID) -> Model*
	{
		if (typeID == NO_ID)
			return nullptr;

		for (const std::shared_ptr<DynamicType>& type : dynamicTypes)
		{
			if (type->getID() == typeID)
				return type->getModel().get();
		}
		return nullptr;
	};

	if (buildShape(types))
	{
		btRigidBody::btRigidBodyConstructionInfo info(0, nullptr, shape);
		info.m_startWorldTransform = btTransform(btQuaternion(renderedRotation.x, renderedRotation.y, renderedRotation.z, renderedRotation.w), g2b3(renderedPosition));
		info.m_friction = 0.9f;

		//Moved to where it's drawn every frame, which pushes players around like the server's does
		body = new btRigidBody(info);
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		body->setActivationState(DISABLE_DEACTIVATION);
		body->setUserIndex(vehicleBody);
		body->setUserPointer((void*)new std::shared_ptr<SimObject>(getMe()));
		world->addBody(body);
	}

	if (isModelVehicle())
	{
		if (Model* bodyModel = modelOfType(bodyTypeID))
		{
			bodyInstance = new ModelInstance(bodyModel);
			bodyDrawnHalfExtents = bodyModel->getDrawnHalfExtents();

			//Whatever the server had looping on it before this client ever heard of it, like a plane's propeller
			for (unsigned char id : loopingAnimations)
				play(id, true);
		}
	}
	else
	{
		renderer = _renderer;
		if (renderer)
			brickGroup = renderer->addBrickGroup(bricks);
	}

	//A vehicle that names its own wheel type uses that, everything else the one tire model the client loaded
	wheelModel = modelOfType(wheelTypeID);
	if (!wheelModel)
		wheelModel = tireModel;

	for (VehicleWheel& wheel : wheels)
	{
		if (wheelModel && !wheel.tire)
			wheel.tire = new ModelInstance(wheelModel);
	}
}

void Vehicle::removeBricks(std::vector<uint16_t> indices, const BrickTypes* types)
{
	//A model vehicle's body is its model, there are no bricks of it to take out
	if (isModelVehicle())
		return;

	//Back to front, so taking one out doesn't move the ones still to go
	std::sort(indices.begin(), indices.end(), [](uint16_t a, uint16_t b) { return a > b; });
	indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

	bool removed = false;
	for (uint16_t index : indices)
	{
		if (index >= bricks.size())
			continue;

		bricks.erase(bricks.begin() + index);
		removed = true;
	}

	//A client's count to wait for before drawing it
	expectedBricks = (unsigned int)bricks.size();

	if (!removed)
		return;

	btCompoundShape* oldShape = shape;
	std::vector<btCollisionShape*> oldOwned;
	oldOwned.swap(ownedShapes);
	shape = nullptr;

	if (!buildShape(types))
	{
		shape = oldShape;
		ownedShapes.swap(oldOwned);
	}
	else
	{
		if (body)
		{
			//Out and back into the world for the new shape, which would also replace its gravity with the world's
			btVector3 gravity = body->getGravity();
			world->removeBody(body);
			body->setCollisionShape(shape);

			//Like buildServer, one per brick
			if (raycastVehicle)
			{
				int children = shape->getNumChildShapes();
				std::vector<btScalar> masses(children, steering.mass);
				btTransform principal;
				btVector3 inertia;
				shape->calculatePrincipalAxisTransform(masses.data(), principal, inertia);
				body->setMassProps((btScalar)children, inertia);
				body->updateInertiaTensor();
			}

			world->addBody(body);
			body->setGravity(gravity);
			body->activate();
		}

		delete oldShape;
		for (btCollisionShape* owned : oldOwned)
			delete owned;
	}

	if (renderer && brickGroup != -1)
	{
		renderer->removeBrickGroup(brickGroup);
		brickGroup = renderer->addBrickGroup(bricks);
	}
}

ENetPacket* Vehicle::makeBricksBrokenPacket(uint16_t bricksBefore, const std::vector<uint16_t>& indices, const glm::vec3& center, float strength) const
{
	//See VehicleBricksBrokenPacket
	static constexpr size_t headerBytes = 1 + sizeof(netIDType) + sizeof(uint16_t) * 2 + sizeof(float) * 4;

	ENetPacket* packet = enet_packet_create(NULL, headerBytes + indices.size() * sizeof(uint16_t), getFlagsFromChannel(OtherReliable));
	netIDType id = getID();
	uint16_t count = (uint16_t)indices.size();

	size_t at = 0;
	packet->data[at++] = VehicleBricksBroken;
	memcpy(packet->data + at, &id, sizeof(netIDType));
	at += sizeof(netIDType);
	memcpy(packet->data + at, &bricksBefore, sizeof(uint16_t));
	at += sizeof(uint16_t);
	memcpy(packet->data + at, &count, sizeof(uint16_t));
	at += sizeof(uint16_t);
	memcpy(packet->data + at, &center[0], sizeof(float) * 3);
	at += sizeof(float) * 3;
	memcpy(packet->data + at, &strength, sizeof(float));
	at += sizeof(float);
	memcpy(packet->data + at, indices.data(), indices.size() * sizeof(uint16_t));
	return packet;
}

void Vehicle::updateSnapshot(float deltaT)
{
	glm::vec3 targetPosition = interpolator.getPosition();
	glm::quat targetRotation = interpolator.getRotation();

	if (!renderedTransformInitialized || glm::any(glm::isnan(renderedPosition)) || glm::any(glm::isnan(renderedRotation)))
	{
		renderedPosition = targetPosition;
		renderedRotation = targetRotation;
		renderedTransformInitialized = true;
	}
	else
	{
		float t = 1.0f - std::exp(-correctionRate * (deltaT / 1000.0f));
		renderedPosition = glm::mix(renderedPosition, targetPosition, t);
		renderedRotation = glm::slerp(renderedRotation, targetRotation, t);
	}

	if (body)
		body->setWorldTransform(btTransform(btQuaternion(renderedRotation.x, renderedRotation.y, renderedRotation.z, renderedRotation.w), g2b3(renderedPosition)));

	//Rolling along the ground turns a wheel back around its axle
	float speed = glm::dot(serverVelocity, renderedRotation * forward);
	float seconds = deltaT / 1000.0f;
	for (VehicleWheel& wheel : wheels)
		wheel.spin = std::fmod(wheel.spin - speed / std::max(wheel.radius, 0.01f) * seconds, glm::two_pi<float>());
}

float Vehicle::getCameraDistance() const
{
	//What it's drawn as for a model vehicle, and the box its bricks fill for one made of those
	glm::vec3 half(1.0f);
	if (bodyInstance && bodyDrawnHalfExtents.x > 0.0f)
		half = bodyDrawnHalfExtents;
	else if (shape)
	{
		btVector3 low, high;
		shape->getAabb(btTransform::getIdentity(), low, high);
		half = (b2g3(high) - b2g3(low)) * 0.5f;
	}

	//Only how wide and long it is: how tall it is has nothing to do with how far back it has to be seen from
	return glm::length(glm::vec2(half.x, half.z)) * cameraDistanceScale;
}

glm::mat4 Vehicle::getDrawnTransform() const
{
	return glm::translate(renderedPosition) * glm::toMat4(renderedRotation);
}

glm::mat4 Vehicle::getBrickTransform() const
{
	return getDrawnTransform() * glm::translate(brickOffset);
}

glm::mat4 Vehicle::getDrawnWheelTransform(int wheel) const
{
	const VehicleWheel& drawn = wheels[wheel];
	glm::vec3 up(0, 1, 0);
	glm::vec3 axle = glm::cross(-up, forward);
	glm::vec3 center = drawn.connection - up * drawn.suspension;
	glm::quat turn = glm::angleAxis(drawn.steering, up) * glm::angleAxis(drawn.spin, axle);
	return glm::translate(renderedPosition + renderedRotation * center) * glm::toMat4(renderedRotation * turn);
}

void Vehicle::releaseSeated(int seatIndex, float idealBufferSize)
{
	bool isDriver = seatIndex == driverSeat;
	if (!isDriver && (seatIndex < 0 || seatIndex >= (int)passengerSeats.size()))
		return;

	std::weak_ptr<Dynamic>& slot = isDriver ? seated : passengerSeats[seatIndex].seated;
	std::shared_ptr<Dynamic> rider = slot.lock();
	slot.reset();

	if (!rider)
		return;

	rider->ridingVehicle.reset();

	//Stands back up out of a model vehicle, unless the server has it sitting on purpose, see LoopClient::placeVehicleDrivers
	if (isModelVehicle())
	{
		int sit = rider->getType()->getModel()->getAnimationID("sit");
		if (sit != -1 && std::find(rider->loopingAnimations.begin(), rider->loopingAnimations.end(), (unsigned char)sit) == rider->loopingAnimations.end())
			rider->stop(sit);
	}

	if (rider->isInWorld() || rider->getKind() != DynamicKind_Plain)
		return;

	//A driver comes out on top of the vehicle, a passenger just off the seat they stood on
	btTransform transform = rider->body->getWorldTransform();
	transform.setOrigin(transform.getOrigin() + btVector3(0, isDriver ? exitHeight : passengerExitLift, 0));
	rider->returnToWorld(transform);

	//Its snapshots are from wherever it got in, so it doesn't glide back from there
	btQuaternion turn = transform.getRotation();
	rider->interpolator.reset();
	rider->interpolator.addSnapshot(b2g3(transform.getOrigin()), glm::quat(turn.w(), turn.x(), turn.y(), turn.z()), idealBufferSize, 0);
}

void Vehicle::releaseEveryone(float idealBufferSize)
{
	releaseSeated(driverSeat, idealBufferSize);
	for (int a = 0; a < (int)passengerSeats.size(); a++)
		releaseSeated(a, idealBufferSize);
}

unsigned int Vehicle::readCreationBytes(const enet_uint8* src, size_t available)
{
	if (available < creationHeaderBytes)
		return 0;

	//Then how many passenger seats it has, and each of them
	unsigned int size = creationHeaderBytes + src[creationHeaderBytes - 1] * wheelCreationBytes + 1;
	if (available < size)
		return 0;

	size += src[size - 1] * seatCreationBytes + 1;
	if (available < size)
		return 0;

	//And the animations looping on its body, see syncLoops
	size += src[size - 1];
	return available < size ? 0 : size;
}

unsigned int Vehicle::readUpdateBytes(const enet_uint8* src, size_t available) const
{
	/*
		How long an update is depends on the vehicle, so a client works it out from the packet rather than
		from what it knows: its wheels it does know about, but how many animations loop on it is whatever
		the server says in this very packet, see readUpdate
	*/
	unsigned int size = updateHeaderBytes + (unsigned int)wheels.size() * wheelUpdateBytes + animationUpdateBytes;
	if (available < size)
		return 0;

	size += src[size - 1];
	return available < size ? 0 : size;
}

void Vehicle::readCreation(const enet_uint8* src)
{
	size_t at = sizeof(netIDType);
	auto take = [&](void* out, size_t count)
	{
		memcpy(out, src + at, count);
		at += count;
	};

	glm::vec3 position;
	glm::quat rotation;
	getPosition(src + at, position);
	at += PositionBytes;
	getQuaternion(src + at, rotation);
	at += QuaternionBytes;

	take(&brickOffset[0], sizeof(float) * 3);
	take(&forward[0], sizeof(float) * 3);
	take(&seat[0], sizeof(float) * 3);
	take(&exitHeight, sizeof(float));

	uint16_t brickCount;
	take(&brickCount, sizeof(uint16_t));
	expectedBricks = brickCount;

	take(&driverID, sizeof(netIDType));
	take(&dirtEmitterType, sizeof(uint16_t));
	take(&bodyTypeID, sizeof(netIDType));
	take(&wheelTypeID, sizeof(netIDType));
	take(&bodyHalfExtents[0], sizeof(float) * 3);
	take(&bodyOffset[0], sizeof(float) * 3);
	take(&headlightMount[0], sizeof(float) * 3);
	readFlags(src[at]);
	at++;

	wheels.resize(src[at]);
	at++;

	for (VehicleWheel& wheel : wheels)
	{
		take(&wheel.connection[0], sizeof(float) * 3);
		take(&wheel.radius, sizeof(float));
		take(&wheel.width, sizeof(float));
		take(&wheel.settings.suspensionLength, sizeof(float));
		wheel.suspension = wheel.settings.suspensionLength;
	}

	passengerSeats.resize(src[at]);
	at++;

	for (PassengerSeat& passengerSeat : passengerSeats)
	{
		take(&passengerSeat.top[0], sizeof(float) * 3);
		take(&passengerSeat.riderID, sizeof(netIDType));
	}

	//Played on its body once finishClient has made one, since the model isn't here yet
	loopingAnimations.assign(src + at + 1, src + at + 1 + src[at]);
	at += 1 + src[at];

	interpolator.addSnapshot(position, rotation, 4, 0);
	renderedPosition = position;
	renderedRotation = rotation;
	renderedTransformInitialized = true;
}

void Vehicle::readUpdate(const enet_uint8* src, float idealBufferSize)
{
	glm::vec3 position;
	glm::quat rotation;
	getPosition(src + 1, position);
	getQuaternion(src + 1 + PositionBytes, rotation);
	//src[0] already accounts for how often this client is sent one, see SimObject::scaleUpdateInterval
	interpolator.addSnapshot(position, rotation, idealBufferSize, src[0]);

	memcpy(&serverVelocity[0], src + 1 + PositionBytes + QuaternionBytes, sizeof(float) * 3);
	readFlags(src[1 + PositionBytes + QuaternionBytes + sizeof(float) * 3]);

	unsigned int at = updateHeaderBytes;
	for (VehicleWheel& wheel : wheels)
	{
		wheel.steering = (int8_t)src[at] / 40.0f;
		wheel.suspension = src[at + 1] / 25.0f;
		wheel.contact = src[at + 2] & 1;
		wheel.dirt = src[at + 2] & 2;
		at += wheelUpdateBytes;
	}

	//An animation played once, which the same update is sent a few times over, so the count is what says it's a new one
	if (src[at] != noAnimation && src[at + 1] != oneShotCount)
	{
		oneShotCount = src[at + 1];
		oneShotAnimation = src[at];
		play(oneShotAnimation, false);
	}
	at += 2;

	syncLoops(std::vector<unsigned char>(src + at + 1, src + at + 1 + src[at]));
}

std::vector<ENetPacket*> Vehicle::makeBrickPackets() const
{
	static constexpr unsigned int recordsPerPacket = (ENET_HOST_DEFAULT_MTU - 20 - brickPacketHeaderBytes) / BrickHolder::recordBytes;

	std::vector<ENetPacket*> packets;
	netIDType id = getID();

	for (size_t start = 0; start < bricks.size(); start += recordsPerPacket)
	{
		uint16_t count = (uint16_t)std::min<size_t>(recordsPerPacket, bricks.size() - start);

		ENetPacket* packet = enet_packet_create(NULL, brickPacketHeaderBytes + count * BrickHolder::recordBytes, getFlagsFromChannel(OtherReliable));
		uint16_t first = (uint16_t)start;
		packet->data[0] = VehicleBricks;
		memcpy(packet->data + 1, &id, sizeof(netIDType));
		memcpy(packet->data + 1 + sizeof(netIDType), &first, sizeof(uint16_t));
		memcpy(packet->data + 1 + sizeof(netIDType) + sizeof(uint16_t), &count, sizeof(uint16_t));

		for (unsigned int a = 0; a < count; a++)
			BrickHolder::writeRecord(&bricks[start + a], packet->data + brickPacketHeaderBytes + a * BrickHolder::recordBytes);

		packets.push_back(packet);
	}

	return packets;
}

ENetPacket* Vehicle::makeDriverPacket() const
{
	//See VehicleDriverPacket
	static constexpr size_t headerBytes = 1 + sizeof(netIDType) * 2 + 1;
	ENetPacket* packet = enet_packet_create(NULL, headerBytes + passengerSeats.size() * sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	netIDType id = getID();
	packet->data[0] = VehicleDriver;
	memcpy(packet->data + 1, &id, sizeof(netIDType));
	memcpy(packet->data + 1 + sizeof(netIDType), &driverID, sizeof(netIDType));
	packet->data[headerBytes - 1] = (enet_uint8)passengerSeats.size();

	for (size_t a = 0; a < passengerSeats.size(); a++)
		memcpy(packet->data + headerBytes + a * sizeof(netIDType), &passengerSeats[a].riderID, sizeof(netIDType));
	return packet;
}

bool Vehicle::requiresNetUpdate()
{
	flaggedForUpdate = false;
	if (!body || getTicksMS() - lastSentTime < 25)
		return false;

	//Every tick while it moves, and now and then while it sits still so late snapshots don't leave it somewhere else
	flaggedForUpdate = body->isActive() || stateChanged || loopResends > 0 || oneShotResends > 0 || getTicksMS() - lastSentTime > 1500;
	return flaggedForUpdate;
}

unsigned char Vehicle::getFlags() const
{
	return (headlight.hasLight ? flagHasHeadlight : 0) | (headlightOn ? flagHeadlightOn : 0) | (flight.enabled ? flagFlies : 0);
}

void Vehicle::readFlags(unsigned char flags)
{
	hasHeadlight = flags & flagHasHeadlight;
	headlightLit = flags & flagHeadlightOn;
	fliesForDriver = flags & flagFlies;
}

unsigned int Vehicle::getCreationPacketBytes() const
{
	return creationHeaderBytes + (unsigned int)wheels.size() * wheelCreationBytes + 1 + (unsigned int)passengerSeats.size() * seatCreationBytes
		+ 1 + (unsigned int)loopingAnimations.size();
}

unsigned int Vehicle::getUpdatePacketBytes() const
{
	return updateHeaderBytes + (unsigned int)wheels.size() * wheelUpdateBytes + animationUpdateBytes + (unsigned int)loopingAnimations.size();
}

void Vehicle::addToCreationPacket(enet_uint8* dest) const
{
	size_t at = 0;
	auto put = [&](const void* data, size_t count)
	{
		memcpy(dest + at, data, count);
		at += count;
	};

	netIDType id = getID();
	put(&id, sizeof(netIDType));

	btTransform transform = body ? body->getWorldTransform() : btTransform::getIdentity();
	btQuaternion turn = transform.getRotation();
	addPosition(dest + at, b2g3(transform.getOrigin()));
	at += PositionBytes;
	addQuaternion(dest + at, glm::quat(turn.w(), turn.x(), turn.y(), turn.z()));
	at += QuaternionBytes;

	put(&brickOffset[0], sizeof(float) * 3);
	put(&forward[0], sizeof(float) * 3);
	put(&seat[0], sizeof(float) * 3);
	put(&exitHeight, sizeof(float));

	uint16_t brickCount = (uint16_t)bricks.size();
	put(&brickCount, sizeof(uint16_t));
	put(&driverID, sizeof(netIDType));
	put(&dirtEmitterType, sizeof(uint16_t));
	put(&bodyTypeID, sizeof(netIDType));
	put(&wheelTypeID, sizeof(netIDType));
	put(&bodyHalfExtents[0], sizeof(float) * 3);
	put(&bodyOffset[0], sizeof(float) * 3);
	put(&headlightMount[0], sizeof(float) * 3);
	dest[at++] = getFlags();

	dest[at++] = (enet_uint8)wheels.size();

	for (const VehicleWheel& wheel : wheels)
	{
		put(&wheel.connection[0], sizeof(float) * 3);
		put(&wheel.radius, sizeof(float));
		put(&wheel.width, sizeof(float));
		put(&wheel.settings.suspensionLength, sizeof(float));
	}

	dest[at++] = (enet_uint8)passengerSeats.size();

	for (const PassengerSeat& passengerSeat : passengerSeats)
	{
		put(&passengerSeat.top[0], sizeof(float) * 3);
		put(&passengerSeat.riderID, sizeof(netIDType));
	}

	dest[at++] = (enet_uint8)loopingAnimations.size();
	if (!loopingAnimations.empty())
		put(loopingAnimations.data(), loopingAnimations.size());
}

void Vehicle::addToUpdatePacket(enet_uint8* dest)
{
	unsigned int msSinceLastSend = getTicksMS() - lastSentTime;
	lastSentTime = getTicksMS();
	dest[0] = (enet_uint8)std::min(msSinceLastSend, 255u);

	const btTransform& transform = body->getWorldTransform();
	btQuaternion turn = transform.getRotation();
	addPosition(dest + 1, b2g3(transform.getOrigin()));
	addQuaternion(dest + 1 + PositionBytes, glm::quat(turn.w(), turn.x(), turn.y(), turn.z()));

	glm::vec3 velocity = b2g3(body->getLinearVelocity());
	memcpy(dest + 1 + PositionBytes + QuaternionBytes, &velocity[0], sizeof(float) * 3);
	dest[1 + PositionBytes + QuaternionBytes + sizeof(float) * 3] = getFlags();
	stateChanged = false;

	unsigned int at = updateHeaderBytes;
	for (const VehicleWheel& wheel : wheels)
	{
		dest[at] = (enet_uint8)(int8_t)std::clamp((int)std::lround(wheel.steering * 40.0f), -127, 127);
		dest[at + 1] = (enet_uint8)std::clamp((int)std::lround(wheel.suspension * 25.0f), 0, 255);
		dest[at + 2] = (wheel.contact ? 1 : 0) | (wheel.dirt ? 2 : 0);
		at += wheelUpdateBytes;
	}

	//Updates go out unreliably, so an animation is repeated in the next few of them, see Dynamic
	bool sendOneShot = oneShotResends > 0 && oneShotAnimation >= 0;
	if (sendOneShot)
		oneShotResends--;

	dest[at] = sendOneShot ? (enet_uint8)oneShotAnimation : noAnimation;
	dest[at + 1] = oneShotCount;
	at += 2;

	if (loopResends > 0)
		loopResends--;

	dest[at++] = (enet_uint8)loopingAnimations.size();
	if (!loopingAnimations.empty())
		memcpy(dest + at, loopingAnimations.data(), loopingAnimations.size());
}

void Vehicle::requestDestruction()
{
	if (body && body->getUserPointer())
		((std::shared_ptr<SimObject>*)body->getUserPointer())->reset();
}

Vehicle::~Vehicle()
{
	if (renderer && brickGroup != -1)
		renderer->removeBrickGroup(brickGroup);

	delete bodyInstance;

	for (VehicleWheel& wheel : wheels)
		delete wheel.tire;

	if (raycastVehicle)
	{
		world->removeAction(raycastVehicle);
		delete raycastVehicle;
	}
	delete raycaster;

	if (body)
	{
		if (body->getUserPointer())
			delete (std::shared_ptr<SimObject>*)body->getUserPointer();
		world->removeBody(body);
		delete body;
	}

	delete shape;
	for (btCollisionShape* owned : ownedShapes)
		delete owned;
}

std::shared_ptr<Vehicle> vehicleFromBody(const btCollisionObject* body)
{
	if (!body || body->getUserIndex() != vehicleBody || !body->getUserPointer())
		return nullptr;

	std::shared_ptr<SimObject>* pointer = (std::shared_ptr<SimObject>*)body->getUserPointer();
	return *pointer ? std::static_pointer_cast<Vehicle>(*pointer) : nullptr;
}
