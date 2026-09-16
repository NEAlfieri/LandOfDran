#include "Vehicle.h"
#include "../Graphics/InstancedBrickRenderer.h"
#include "../Bricks/BrickHolder.h"

#include <glm/gtc/constants.hpp>

//The engine stops pushing past this speed, world units per second, like the old game
static constexpr float maxDriveSpeed = 200.0f;

//Wheels on the ground throw dirt going faster than this while turning or braking, like the old game
static constexpr float dirtSpeedKmHour = 50.0f;

//How hard a wheel's suspension can push, Bullet's default of 6000 left anything over a few hundred bricks sitting on the ground
static constexpr float maxSuspensionForce = 10000000.0f;

//The old game popped new cars upward so bricks sitting on the ground don't start stuck in it
static constexpr float spawnUpwardSpeed = 20.0f;

//Net ID, position, rotation, brick offset, forward, seat, exit height, brick count, driver, dirt emitter type, wheel count
static constexpr unsigned int creationHeaderBytes = sizeof(netIDType) + PositionBytes + QuaternionBytes + sizeof(float) * 10 + sizeof(uint16_t) + sizeof(netIDType) + sizeof(uint16_t) + 1;

//Milliseconds since the last update, position, rotation, velocity as full floats since cars go faster than dynamics' quantized velocity reaches
static constexpr unsigned int updateHeaderBytes = 1 + PositionBytes + QuaternionBytes + sizeof(float) * 3;

//Packet type, vehicle net ID, u16 index of the first brick, u16 count
static constexpr unsigned int brickPacketHeaderBytes = 1 + sizeof(netIDType) + sizeof(uint16_t) * 2;

//Per second, how quickly where it's drawn catches up to the interpolator, same as dynamics
static constexpr float correctionRate = 15.0f;

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
	std::vector<btScalar> masses(children, steering.mass);
	btTransform principal;
	btVector3 inertia;
	shape->calculatePrincipalAxisTransform(masses.data(), principal, inertia);

	btRigidBody::btRigidBodyConstructionInfo info((btScalar)children, nullptr, shape, inertia);
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

	body->setLinearVelocity(btVector3(0, spawnUpwardSpeed, 0));
	return true;
}

bool Vehicle::drive(bool forwardHeld, bool backwardHeld, bool leftHeld, bool rightHeld, bool brakeHeld)
{
	if (!raycastVehicle)
		return false;

	throwsDirt = brakeHeld || leftHeld || rightHeld;

	//Bullet turns wheels toward the right for positive steering, with our axles and a vehicle's up
	float steer = leftHeld ? -1.0f : (rightHeld ? 1.0f : 0.0f);
	float engine = forwardHeld ? 1.0f : (backwardHeld ? -1.0f : 0.0f);
	bool speeding = body->getLinearVelocity().length() > maxDriveSpeed;

	for (int a = 0; a < (int)wheels.size(); a++)
	{
		const WheelSettings& settings = wheels[a].settings;
		raycastVehicle->setBrake(brakeHeld ? settings.brakeForce : 0.0f, a);
		raycastVehicle->setSteeringValue(steer * settings.steerAngle, a);
		raycastVehicle->applyEngineForce(speeding ? 0.0f : settings.engineForce * engine, a);
	}

	if (forwardHeld || backwardHeld || leftHeld || rightHeld || brakeHeld)
		body->activate();

	return speeding;
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

	//Looking straight up or down, they face the way it drives
	glm::vec3 local = glm::inverse(rotation) * look;
	local.y = 0.0f;
	if (glm::length(local) < 0.001f || glm::any(glm::isnan(local)))
		local = forward;

	//Player models face -Z
	glm::quat facing = rotation * glm::angleAxis(std::atan2(-local.x, -local.z), glm::vec3(0, 1, 0));

	//The bottom of its collision box on the seat's top
	std::shared_ptr<Model> model = rider.getType()->getModel();
	float feetToOrigin = model->getColHalfExtents().y - model->getColOffset().y;
	glm::vec3 position = origin + rotation * (passengerSeats[seatIndex].top + glm::vec3(0, feetToOrigin, 0));

	return btTransform(btQuaternion(facing.x, facing.y, facing.z, facing.w), g2b3(position));
}

int Vehicle::findFreeSeat(const glm::vec3& near) const
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

		float distance = glm::distance2(origin + rotation * passengerSeats[a].top, near);
		if (best == -1 || distance < bestDistance)
		{
			best = a;
			bestDistance = distance;
		}
	}
	return best;
}

void Vehicle::finishClient(const BrickTypes* types, InstancedBrickRenderer* _renderer, Model* tireModel)
{
	if (brickGroup != -1 || body)
		return;

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

	renderer = _renderer;
	if (renderer)
		brickGroup = renderer->addBrickGroup(bricks);

	for (VehicleWheel& wheel : wheels)
	{
		if (tireModel && !wheel.tire)
			wheel.tire = new ModelInstance(tireModel);
	}
}

void Vehicle::removeBricks(std::vector<uint16_t> indices, const BrickTypes* types)
{
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

	size += src[size - 1] * seatCreationBytes;
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
	interpolator.addSnapshot(position, rotation, idealBufferSize, src[0]);

	memcpy(&serverVelocity[0], src + 1 + PositionBytes + QuaternionBytes, sizeof(float) * 3);

	unsigned int at = updateHeaderBytes;
	for (VehicleWheel& wheel : wheels)
	{
		wheel.steering = (int8_t)src[at] / 40.0f;
		wheel.suspension = src[at + 1] / 25.0f;
		wheel.contact = src[at + 2] & 1;
		wheel.dirt = src[at + 2] & 2;
		at += wheelUpdateBytes;
	}
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
	flaggedForUpdate = body->isActive() || getTicksMS() - lastSentTime > 1500;
	return flaggedForUpdate;
}

unsigned int Vehicle::getCreationPacketBytes() const
{
	return creationHeaderBytes + (unsigned int)wheels.size() * wheelCreationBytes + 1 + (unsigned int)passengerSeats.size() * seatCreationBytes;
}

unsigned int Vehicle::getUpdatePacketBytes() const
{
	return updateHeaderBytes + (unsigned int)wheels.size() * wheelUpdateBytes;
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

	unsigned int at = updateHeaderBytes;
	for (const VehicleWheel& wheel : wheels)
	{
		dest[at] = (enet_uint8)(int8_t)std::clamp((int)std::lround(wheel.steering * 40.0f), -127, 127);
		dest[at + 1] = (enet_uint8)std::clamp((int)std::lround(wheel.suspension * 25.0f), 0, 255);
		dest[at + 2] = (wheel.contact ? 1 : 0) | (wheel.dirt ? 2 : 0);
		at += wheelUpdateBytes;
	}
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
