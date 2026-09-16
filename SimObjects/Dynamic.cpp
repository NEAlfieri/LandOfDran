#include "Dynamic.h"
#include "../GameLoop/PlayerAppearance.h"
#include <cmath>

Dynamic::Dynamic(std::shared_ptr<DynamicType> _type, const btVector3& initialPos, const btQuaternion &initialRot)
	: type(_type)
{
	//Without this, lastSentTime stays 0 until the first successful send, so the very first update this object
	//ever sends computes msSinceLastSend as getTicksMS() - 0 (the server's entire uptime), which gets clamped to
	//255 during quantization and makes the interpolator schedule that first real snapshot ~250ms+ in the future
	//instead of almost immediately - looks like the object sits still for a beat right after being created
	lastSentTime = getTicksMS();

	body = type->createBody();
	world->addBody(body);

	btTransform t;
	t.setIdentity();
	t.setOrigin(initialPos);
	t.setRotation(initialRot);
	body->setWorldTransform(t);

	modelInstance = new ModelInstance(type->getModel().get());

	if (!type->getModel()->isServerSide())
	{
		//TODO: Pass initial rotation as well
		interpolator.addSnapshot(b2g3(initialPos), glm::quat(initialRot.w(),initialRot.x(),initialRot.y(),initialRot.z()), 4, 0);
		modelInstance->setModelTransform(glm::translate(glm::vec3(initialPos.x(), initialPos.y(), initialPos.z())));
	}
}

void Dynamic::onCreation()
{
	//Pointer to a smart pointer to this
	body->setUserPointer((void*)new std::shared_ptr<SimObject>(getMe()));
}

//Swimming slower than this, a player turns back upright
static constexpr float swimTiltMinSpeed = 1.5f;

//Per second, how quickly a swimming player's model turns toward the way they're going
static constexpr float swimTiltRate = 6.0f;

//Per second, how quickly the movement other players' tilt follows catches up, evens out uneven network updates
static constexpr float tiltVelocitySmoothing = 8.0f;

void Dynamic::updateSnapshot(float deltaT, bool forceUsePhysicsTransform, float waterLevel)
{
	//Every frame mixes toward the last, so one NaN target would otherwise hide this and misplace its sounds forever
	if (glm::any(glm::isnan(renderedPosition)) || glm::any(glm::isnan(renderedRotation)) || glm::any(glm::isnan(tiltVelocity)))
	{
		renderedTransformInitialized = false;
		tiltVelocity = glm::vec3(0);
	}

	glm::vec3 previousPosition = renderedPosition;
	bool wasInitialized = renderedTransformInitialized;

	glm::vec3 targetPos;
	glm::quat targetRot;

	if (clientControlled || forceUsePhysicsTransform)
	{
		const btTransform& t = body->getWorldTransform();
		const btVector3& v = t.getOrigin();
		const btQuaternion &q = t.getRotation();
		targetPos = glm::vec3(v.x(), v.y(), v.z());
		targetRot = glm::quat(q.w(), q.x(), q.y(), q.z());
	}
	else
	{
		targetPos = interpolator.getPosition();
		targetRot = interpolator.getRotation();
	}

	if (!renderedTransformInitialized)
	{
		renderedPosition = targetPos;
		renderedRotation = targetRot;
		renderedTransformInitialized = true;
	}
	else
	{
		//While driven directly by physics (predicting a local collision, or the debug physics view), track the
		//target essentially instantly - the whole point of local prediction is immediate feedback, so this shouldn't
		//add lag on top of it. The smoothing is for reconciling with the authoritative/interpolated target instead,
		//where a sudden jump (rather than the continuous motion physics/interpolation normally produce) is possible
		float correctionRatePerSecond = (clientControlled || forceUsePhysicsTransform) ? 1000.0f : 15.0f;
		float t = 1.0f - std::exp(-correctionRatePerSecond * (deltaT / 1000.0f));

		renderedPosition = glm::mix(renderedPosition, targetPos, t);
		renderedRotation = glm::slerp(renderedRotation, targetRot, t);
	}

	//A swimming player's head points the way they're going, and turns back upright when they stop
	//Someone else's body here falls with no water forces between server updates, so its velocity always points down,
	//go by how their model actually moves instead
	if (clientControlled)
		tiltVelocity = b2g3(body->getLinearVelocity());
	else if (wasInitialized && deltaT > 0)
	{
		glm::vec3 moved = (renderedPosition - previousPosition) / (deltaT / 1000.0f);
		tiltVelocity = glm::mix(tiltVelocity, moved, 1.0f - std::exp(-tiltVelocitySmoothing * (deltaT / 1000.0f)));
	}

	glm::quat targetTilt = glm::quat(1, 0, 0, 0);
	if (playWalkingAnimation && getSubmergedFraction(waterLevel) >= swimDepth && glm::length(tiltVelocity) > swimTiltMinSpeed)
		targetTilt = glm::rotation(glm::vec3(0, 1, 0), glm::normalize(tiltVelocity));

	renderedTilt = glm::slerp(renderedTilt, targetTilt, 1.0f - std::exp(-swimTiltRate * (deltaT / 1000.0f)));

	//Tilts around the middle of the collision box instead of the model's origin
	glm::vec3 pivot = renderedPosition + renderedRotation * type->getModel()->getColOffset();
	modelInstance->setModelTransform(glm::translate(pivot) * glm::toMat4(renderedTilt) * glm::translate(renderedPosition - pivot) * glm::toMat4(renderedRotation));

	turnHead(deltaT);
}

//Per second, how quickly other players' heads catch up to where the server says they look, evens out its updates
static constexpr float lookSmoothing = 15.0f;

//How far a head turns from facing the way the body does, radians
static const float maxHeadYaw = glm::radians(70.0f);
static const float maxHeadPitchUp = glm::radians(50.0f);
static const float maxHeadPitchDown = glm::radians(35.0f);

//Looking further behind the body than this, the head turns back toward the front, so it doesn't snap side to side looking straight back
static const float headYawFadeStart = glm::radians(100.0f);

void Dynamic::turnHead(float deltaT)
{
	int head = type->getModel()->getHeadNodeIdx();
	if (!hasLook || head == -1 || glm::any(glm::isnan(lookDirection)) || glm::length(lookDirection) < 0.0001f)
		return;

	glm::vec3 target = glm::normalize(lookDirection);
	if (clientControlled || !renderedLookInitialized || glm::any(glm::isnan(renderedLook)))
		renderedLook = target;
	else
	{
		glm::vec3 mixed = glm::mix(renderedLook, target, 1.0f - std::exp(-lookSmoothing * (deltaT / 1000.0f)));
		renderedLook = glm::length(mixed) > 0.0001f ? glm::normalize(mixed) : target;
	}
	renderedLookInitialized = true;

	//The player model faces -Z, the same way PlayerController walks it
	glm::vec3 local = glm::inverse(renderedTilt * renderedRotation) * renderedLook;
	float pitch = std::asin(std::clamp(local.y, -1.0f, 1.0f));
	float yaw = std::atan2(-local.x, -local.z);

	float fade = 1.0f - std::clamp((std::abs(yaw) - headYawFadeStart) / (glm::pi<float>() - headYawFadeStart), 0.0f, 1.0f);
	yaw = std::clamp(yaw, -maxHeadYaw, maxHeadYaw) * fade;
	pitch = std::clamp(pitch, -maxHeadPitchDown, maxHeadPitchUp);

	modelInstance->setNodeRotation(head, glm::angleAxis(yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(pitch, glm::vec3(1, 0, 0)));
}

void Dynamic::playOneShot(int id)
{
	if (!type->getModel()->isServerSide())
	{
		modelInstance->restartAnimation(id);
		return;
	}

	oneShotAnimation = id;
	oneShotCount++;
	oneShotResends = 4;
}

glm::vec3 Dynamic::getMeshCenter(int meshIndex) const
{
	if (!modelInstance || meshIndex < 0 || meshIndex >= modelInstance->getNumMeshes())
		return renderedTransformInitialized ? renderedPosition : b2g3(getPosition());

	return modelInstance->getMeshCenter(meshIndex);
}

glm::quat Dynamic::getMeshRotation(int meshIndex) const
{
	if (modelInstance && meshIndex >= 0 && meshIndex < modelInstance->getNumMeshes())
		return modelInstance->getMeshRotation(meshIndex);

	if (renderedTransformInitialized)
		return renderedTilt * renderedRotation;

	btQuaternion rotation = body->getWorldTransform().getRotation();
	return glm::quat(rotation.w(), rotation.x(), rotation.y(), rotation.z());
}

void Dynamic::handOffFromPrediction(float idealBufferSize)
{
	const btTransform& t = body->getWorldTransform();
	const btVector3& v = t.getOrigin();
	const btQuaternion& q = t.getRotation();
	interpolator.addSnapshot(glm::vec3(v.x(), v.y(), v.z()), glm::quat(q.w(), q.x(), q.y(), q.z()), idealBufferSize, 0);
}

void Dynamic::setPosition(const btVector3& pos)
{
	btTransform t = body->getWorldTransform();
	t.setOrigin(pos);
	body->setWorldTransform(t);
}

void Dynamic::activate() const
{
	body->activate();
}

void Dynamic::removeFromWorld()
{
	if (!inWorld)
		return;

	unsnapFromCursor();
	outOfWorldGravity = body->getGravity();
	world->removeBody(body);
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	inWorld = false;
}

void Dynamic::returnToWorld(const btTransform& transform)
{
	if (inWorld)
		return;

	body->setWorldTransform(transform);
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	world->addBody(body);
	//Adding a body gives it the world's gravity
	body->setGravity(outOfWorldGravity);
	body->activate(true);
	inWorld = true;
	forceUpdateAll = true;
}

void Dynamic::setDrawnTransform(const glm::vec3& position, const glm::quat& rotation)
{
	renderedPosition = position;
	renderedRotation = rotation;
	renderedTilt = glm::quat(1, 0, 0, 0);
	renderedTransformInitialized = true;
	modelInstance->setModelTransform(glm::translate(position) * glm::toMat4(rotation));
}

void Dynamic::setVelocity(const btVector3& vel)
{
	body->setLinearVelocity(vel);
}

void Dynamic::setAngularVelocity(const btVector3& vel)
{
	body->setAngularVelocity(vel);
}

//Slower than this in studs per second, a projectile's heading wobbles too much to turn it to, like the old game
static constexpr btScalar projectileTurnMinSpeed = 8.0f;

void Dynamic::faceVelocity()
{
	btVector3 velocity = body->getLinearVelocity();
	if (velocity.length() < projectileTurnMinSpeed)
		return;

	btTransform transform = body->getWorldTransform();
	transform.setRotation(shortestArcQuat(btVector3(0, 1, 0), velocity.normalized()));
	body->setWorldTransform(transform);
	body->setAngularVelocity(btVector3(0, 0, 0));
}

btVector3 Dynamic::getVelocity() const
{
	return body->getLinearVelocity();
}

btVector3 Dynamic::getAngularVelocity() const
{
	return body->getAngularVelocity();
}

btVector3 Dynamic::getPosition() const
{
	return body->getWorldTransform().getOrigin();
}

//Per second, how quickly water slows movement and spinning when all the way under
static constexpr float waterLinearDrag = 2.0f;
static constexpr float waterAngularDrag = 3.0f;

btScalar Dynamic::getSubmergedFraction(float waterLevel) const
{
	btVector3 aabbMin, aabbMax;
	body->getAabb(aabbMin, aabbMax);
	btScalar height = aabbMax.y() - aabbMin.y();
	if (height <= 0)
		return 0;

	return std::clamp((waterLevel - aabbMin.y()) / height, (btScalar)0, (btScalar)1);
}

void Dynamic::applyWaterForces(float waterLevel, float deltaT)
{
	if (!body || body->getInvMass() <= 0)
		return;

	btScalar submerged = getSubmergedFraction(waterLevel);
	if (submerged <= 0)
		return;

	body->activate();

	//Roughly Archimedes: the more of it is under, the more of its weight the water holds up
	btScalar mass = 1.0f / body->getInvMass();
	body->applyCentralForce(-body->getGravity() * mass * buoyancy * submerged);

	btScalar seconds = std::clamp(deltaT / 1000.0f, 0.0f, 0.1f);
	body->setLinearVelocity(body->getLinearVelocity() * std::exp(-waterLinearDrag * submerged * seconds));
	body->setAngularVelocity(body->getAngularVelocity() * std::exp(-waterAngularDrag * submerged * seconds));
}

void Dynamic::snapToCursor(std::shared_ptr<JoinedClient> client, const glm::vec3& offset)
{
	//Only stash gravity the first time - re-snapping (new client/offset) shouldn't clobber the real pre-snap value
	if (!isSnappedToCursor())
		preSnapGravity = body->getGravity();

	snappedToClient = client;
	snapOffset = offset;

	gravityUpdated = true;
	body->setGravity(btVector3(0, 0, 0));
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	activate();
}

void Dynamic::unsnapFromCursor()
{
	if (!isSnappedToCursor())
		return;

	snappedToClient.reset();

	gravityUpdated = true;
	body->setGravity(preSnapGravity);
	body->setLinearVelocity(btVector3(0, 0, 0));
	activate();
}

void Dynamic::updateCursorSnapPosition(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection)
{
	glm::vec3 forward = glm::normalize(cameraDirection);

	glm::vec3 worldUp(0, 1, 0);
	glm::vec3 right = glm::cross(forward, worldUp);
	//Looking (near) straight up/down, cross product degenerates - fall back to an arbitrary right vector
	if (glm::length(right) < 0.001f)
		right = glm::vec3(1, 0, 0);
	else
		right = glm::normalize(right);

	glm::vec3 up = glm::normalize(glm::cross(right, forward));

	glm::vec3 target = cameraPosition + right * snapOffset.x + up * snapOffset.y + forward * snapOffset.z;

	setPosition(btVector3(target.x, target.y, target.z));
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	forcePlayerUpdate = true;
	activate();
}

const bool noVelUpdates = false;

//A player's look direction has to turn this far before it's sent again, the head follows it smoothly on clients
static const float lookResendCosine = std::cos(glm::radians(2.0f));

//Server only, whether an update would carry a look direction, see DynamicExtra_Look
static bool sendsLook(const Dynamic& dynamic)
{
	//A client's own player turns its head from its camera, and the server has no use for it
	return dynamic.hasLook && !dynamic.clientControlled;
}

bool Dynamic::requiresNetUpdate() //const
{
	//Carried items have no position of their own to send, see Item
	//A player on a vehicle still sends where they look and their grabs, clients stand them on it themselves, see Vehicle
	if (!inWorld)
	{
		bool lookChanged = sendsLook(*this) && glm::dot(lookDirection, lastSentLook) < lookResendCosine;
		flaggedForUpdate = getKind() == DynamicKind_Plain && getTicksMS() - lastSentTime >= 25 && (lookChanged || oneShotResends > 0);
		return flaggedForUpdate;
	}

	if (getTicksMS() - lastSentTime < 25)
	{
		flaggedForUpdate = false;
		return false;
	}

	bool lookChanged = sendsLook(*this) && glm::dot(lookDirection, lastSentLook) < lookResendCosine;

	//For dynamics requiresUpdate means a change to something like a decal, or a node color
	if (requiresUpdate || gravityUpdated || frictionUpdated || restitutionUpdated || lookChanged || oneShotResends > 0)
	{
		flaggedForUpdate = true;
		return true;
	}

	const btTransform& t = body->getWorldTransform();

	//If the object has moved more than 0.14 studs
	if (t.getOrigin().distance2(lastSentTransform.getOrigin()) > 0.005)
	{
		flaggedForUpdate = true;
		return true;
	}

	if (body->getWorldTransform().getRotation().angleShortestPath(lastSentTransform.getRotation()) > 0.01)
	{
		flaggedForUpdate = true;
		return true;
	}

	//While snapped, updateCursorSnapPosition zeroes velocity every tick server-side, so it never actually
	//changes from the client's perspective and the threshold checks below would stop resyncing it after the
	//first tick - leaving the client's own local physics body free to pick up stray velocity (e.g. brushing
	//against geometry while swinging the held item around) that never gets corrected again. Keep forcing it
	//while snapped so the client stays pinned at zero instead of visibly drifting (only shows up in the debug
	//physics view, which renders the raw local body instead of the smoothed interpolator)
	if ((lastSentVel.distance2(body->getLinearVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
	{
		flaggedForUpdate = true;
		return true;
	}

	//More than like 6 degrees difference in rotation?
	if ((lastSentAngVel.distance2(body->getAngularVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
	{
		flaggedForUpdate = true;
		return true;
	}

	//Even if the object isn't moving much at all we should still send out an update every once in a while
	if (getTicksMS() - lastSentTime > 1500)
	{
		forceUpdateAll = true;
		flaggedForUpdate = true;
		return true;
	}
	//This caused crashes cause the time can increment between the time we check and the time we send the packet
	

	return false;
}

unsigned int Dynamic::getUpdatePacketBytes() const
{
	/*
		1 byte ms since last send
		1 byte what needs updating
		position
		rotation
		velocity
		angular velocity
	*/

	//Plus a second flags byte, see DynamicExtra flags
	unsigned int ret = 3;

	if (sendsLook(*this))
		ret += LookDirectionBytes;

	if (oneShotResends > 0)
		ret += 2;

	const btTransform& t = body->getWorldTransform();

	bool needPosRot = false;

	//If the object has moved more than 0.14 studs
	if (t.getOrigin().distance2(lastSentTransform.getOrigin()) > 0.005)
		needPosRot = true;

	if (body->getWorldTransform().getRotation().angleShortestPath(lastSentTransform.getRotation()) > 0.01)
		needPosRot = true;

	if (forceUpdateAll)
		needPosRot = true;

	if (needPosRot)
	{
		ret += PositionBytes;
		ret += QuaternionBytes;
	}

	//See the matching comment in requiresNetUpdate for why isSnappedToCursor is included here
	if ((lastSentVel.distance2(body->getLinearVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
		ret += VelocityBytes;

	//More than like 6 degrees difference in rotation?
	if ((lastSentAngVel.distance2(body->getAngularVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
		ret += AngularVelocityBytes;

	if (gravityUpdated)
		ret += sizeof(float) * 3;

	if (frictionUpdated)
		ret += sizeof(float);

	if (restitutionUpdated)
		ret += sizeof(float);

	return ret;
}

void Dynamic::setMeshColor(int meshIdx, const glm::vec4& color)
{
	modelInstance->setColor(meshIdx, color);
}

ENetPacket* Dynamic::setMeshColor(const std::string &meshName,const glm::vec4& color)
{
	int meshIdx = getType()->getModel()->getMeshIdx(meshName);
	if(meshIdx == -1)
	{
		error("Mesh " + meshName + " not found in model");
		return nullptr;
	} 

	modelInstance->setColor(meshIdx, color);

	ENetPacket *ret = enet_packet_create(NULL, sizeof(netIDType) + 3 + sizeof(glm::vec4), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)MeshAppearance;
	ret->data[1] = (unsigned char)DynamicTypeId;
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	ret->data[sizeof(netIDType) + 2] = meshIdx;
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 0, &color.r, sizeof(float));
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 1, &color.g, sizeof(float));
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 2, &color.b, sizeof(float));	
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 3, &color.a, sizeof(float));

	return ret;
}

void Dynamic::setMeshDecal(int meshIdx, int decalId)
{
	modelInstance->setDecal(meshIdx, decalId);
}

/*
	1 byte		-	packet type
	4 bytes		-	dynamic net ID
	1 byte		-	mesh index
	1 byte		-	name length, 0 to take the decal off
	0-64 bytes	-	face or shirt file name
*/
ENetPacket* Dynamic::setMeshDecal(const std::string& meshName, const std::string& decalName)
{
	int meshIdx = getType()->getModel()->getMeshIdx(meshName);
	if (meshIdx == -1 || meshIdx > 255)
	{
		error("Mesh " + meshName + " not found in model");
		return nullptr;
	}

	std::string name = decalName.substr(0, PlayerAppearance::maxNameLength);
	if (name.empty())
		meshDecals.erase(meshIdx);
	else
		meshDecals[meshIdx] = name;

	ENetPacket* ret = enet_packet_create(NULL, 3 + sizeof(netIDType) + name.length(), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)MeshDecal;
	memcpy(ret->data + 1, &netID, sizeof(netIDType));
	ret->data[1 + sizeof(netIDType)] = (unsigned char)meshIdx;
	ret->data[2 + sizeof(netIDType)] = (unsigned char)name.length();
	memcpy(ret->data + 3 + sizeof(netIDType), name.data(), name.length());

	return ret;
}

void Dynamic::setHighlight(const glm::vec4& color, float thickness)
{
	modelInstance->setHighlight(color, thickness);
}

ENetPacket* Dynamic::makeHighlightPacket(const glm::vec4& color, float thickness) const
{
	ENetPacket* ret = enet_packet_create(NULL, sizeof(netIDType) + 2 + sizeof(glm::vec4) + sizeof(float), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)HighlightAppearance;
	ret->data[1] = (unsigned char)DynamicTypeId;
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 0, &color.r, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 1, &color.g, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 2, &color.b, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 3, &color.a, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 4, &thickness, sizeof(float));

	return ret;
}

void Dynamic::setNameTag(const std::string& text, const glm::vec3& color)
{
	nameTag = text.substr(0, maxNameTagLength);
	nameTagColor = color;
}

/*
	1 byte		-	NameTag
	4 bytes		-	net ID
	12 bytes	-	color
	1 byte		-	text length, 0 to take the tag off
	0-64 bytes	-	the text
*/
ENetPacket* Dynamic::makeNameTagPacket() const
{
	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(netIDType) + sizeof(float) * 3 + nameTag.length(), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)NameTag;
	memcpy(ret->data + 1, &netID, sizeof(netIDType));
	memcpy(ret->data + 1 + sizeof(netIDType), &nameTagColor.r, sizeof(float) * 3);
	ret->data[1 + sizeof(netIDType) + sizeof(float) * 3] = (unsigned char)nameTag.length();
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 3, nameTag.data(), nameTag.length());

	return ret;
}

ENetPacket* Dynamic::makeBuoyancyPacket() const
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(netIDType) + sizeof(float), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)DynamicBuoyancy;
	memcpy(ret->data + 1, &netID, sizeof(netIDType));
	memcpy(ret->data + 1 + sizeof(netIDType), &buoyancy, sizeof(float));

	return ret;
}

void Dynamic::addToUpdatePacket(enet_uint8 * dest)
{
	requiresUpdate = false;

	const btTransform& t = body->getWorldTransform();

	bool needPosRot = false;
	if (t.getOrigin().distance2(lastSentTransform.getOrigin()) > 0.005)
		needPosRot = true;
	if (body->getWorldTransform().getRotation().angleShortestPath(lastSentTransform.getRotation()) > 0.01)
		needPosRot = true;

	if (forceUpdateAll)
		needPosRot = true;
	forceUpdateAll = false;

	//See the matching comment in requiresNetUpdate for why isSnappedToCursor is included here
	bool needVel = false;
	if ((lastSentVel.distance2(body->getLinearVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
		needVel = true;

	bool needAngVel = false;
	if ((lastSentAngVel.distance2(body->getAngularVelocity()) > 0.37 && !noVelUpdates) || isSnappedToCursor())
		needAngVel = true;

	unsigned int msSinceLastSend = getTicksMS() - lastSentTime;

	dest[0] = std::min(msSinceLastSend,255u);

	//Flags saying what was updated
	unsigned char flags = 0;
	flags += needPosRot ? 1 : 0;
	flags += needVel    ? 2 : 0;
	flags += needAngVel ? 4 : 0;
	flags += gravityUpdated ? 8 : 0;
	flags += restitutionUpdated ? 16 : 0;
	flags += frictionUpdated ? 32 : 0;
	flags += playWalkingAnimation ? 64 : 0;
	flags += forcePlayerUpdate ? 128 : 0;
	dest[1] = flags;

	forcePlayerUpdate = false;

	bool needLook = sendsLook(*this);
	bool needOneShot = oneShotResends > 0;

	unsigned char extraFlags = 0;
	extraFlags |= needLook ? DynamicExtra_Look : 0;
	extraFlags |= needOneShot ? DynamicExtra_OneShot : 0;
	dest[2] = extraFlags;

	int byteIterator = 3;

	if (needPosRot)
	{
		forceUpdateAll = false;
		lastSentTime = getTicksMS();
		lastSentTransform = body->getWorldTransform();
		const glm::vec3& pos = glm::vec3(lastSentTransform.getOrigin().x(), lastSentTransform.getOrigin().y(), lastSentTransform.getOrigin().z());
		const glm::quat& quat = glm::quat(lastSentTransform.getRotation().w(), lastSentTransform.getRotation().x(), lastSentTransform.getRotation().y(), lastSentTransform.getRotation().z());
		addPosition(dest + byteIterator, pos);
		byteIterator += PositionBytes;
		addQuaternion(dest + byteIterator, quat);
		byteIterator += QuaternionBytes;
	}
	if (needVel)
	{
		lastSentVel = body->getLinearVelocity();
		const glm::vec3& linVel = glm::vec3(body->getLinearVelocity().x(), body->getLinearVelocity().y(), body->getLinearVelocity().z());
		addVelocity(dest + byteIterator, linVel);
		byteIterator += VelocityBytes;
	}
	if (needAngVel)
	{
		lastSentAngVel = body->getAngularVelocity();
		const glm::vec3& angVel = glm::vec3(body->getAngularVelocity().x(), body->getAngularVelocity().y(), body->getAngularVelocity().z());
		addAngularVelocity(dest + byteIterator, angVel);
		byteIterator += AngularVelocityBytes;
	}

	if (gravityUpdated)
	{
		glm::vec3 gravity = b2g3(body->getGravity());
		memcpy(dest + byteIterator, &gravity.x, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &gravity.y, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &gravity.z, sizeof(float));
		byteIterator += sizeof(float);
		gravityUpdated = false;
	}

	if (restitutionUpdated)
	{
		float restitution = body->getRestitution();
		memcpy(dest + byteIterator, &restitution, sizeof(float));
		byteIterator += sizeof(float);
		restitutionUpdated = false;
	}

	if (frictionUpdated)
	{
		float friction = body->getFriction();
		memcpy(dest + byteIterator, &friction, sizeof(float));
		byteIterator += sizeof(float);
		frictionUpdated = false;
	}

	if (needLook)
	{
		lastSentLook = lookDirection;
		addLookDirection(dest + byteIterator, lookDirection);
		byteIterator += LookDirectionBytes;
	}

	if (needOneShot)
	{
		oneShotResends--;
		dest[byteIterator] = (unsigned char)std::clamp(oneShotAnimation, 0, 255);
		dest[byteIterator + 1] = oneShotCount;
		byteIterator += 2;
	}
}

/*
	Note, does not include packet header or anything, only the marginal bytes added by* this* object in a bigger packet
	Same goes for all packet related functions here
*/
unsigned int Dynamic::getCreationPacketBytes() const
{
	/*
		4 bytes - net ID
		4 bytes - dyanamic type ID
		position
		rotation
		scale
	*/

	int meshColorsSize = 0;
	glm::vec4 color;
	for (int a = 0; a < modelInstance->getNumMeshes(); a++)
		if (modelInstance->getMeshColor(a, color))
			meshColorsSize++;

	meshColorsSize *= sizeof(glm::vec4) + 1; //1 byte for mesh index
	meshColorsSize++; //1 extra byte for how many mesh colors we are sending

	float highlightThickness;
	//1 flag byte for whether a highlight is present, plus its data if so
	int highlightSize = 1 + (modelInstance->getHighlight(color, highlightThickness) ? sizeof(glm::vec4) + sizeof(float) : 0);

	//1 byte for how many meshes have decals, then each one's mesh index, name length, and name
	int decalsSize = 1;
	for (const auto& [meshIdx, decalName] : meshDecals)
		decalsSize += 2 + (int)decalName.length();

	//1 byte for how long the name tag is, then its color and text if it has one
	int nameTagSize = 1 + (nameTag.empty() ? 0 : (int)sizeof(float) * 3 + (int)nameTag.length());

	//Buoyancy, decals, and then the name tag go last, then a DynamicKind byte and whatever that kind adds
	return meshColorsSize + highlightSize + decalsSize + nameTagSize + PositionBytes + QuaternionBytes + sizeof(netIDType) * 2 + sizeof(float) + 1 + getKindCreationBytes();
}

void Dynamic::addToCreationPacket(enet_uint8* dest) const
{
	memcpy(dest, &netID, sizeof(netIDType));

	const netIDType typeID = type->getID();
	memcpy(dest + sizeof(netIDType), &typeID, sizeof(netIDType));

	const btTransform& t = body->getWorldTransform();
	const glm::vec3& pos = glm::vec3(t.getOrigin().x(), t.getOrigin().y(), t.getOrigin().z());
	const glm::quat& quat = glm::quat(t.getRotation().w(), t.getRotation().x(), t.getRotation().y(), t.getRotation().z());

	addPosition(dest + sizeof(netIDType) * 2, pos);
	addQuaternion(dest + sizeof(netIDType) * 2 + PositionBytes, quat);

	int meshColorsStart = sizeof(netIDType) * 2 + PositionBytes + QuaternionBytes;

	//Skip the byte with the mesh colors count for now until we actually know how many meshes need colors sent
	int byteIterator = meshColorsStart+1; 

	int meshColors = 0;
	glm::vec4 color;
	for (int a = 0; a < modelInstance->getNumMeshes(); a++)
	{
		if (modelInstance->getMeshColor(a, color))
		{
			dest[byteIterator] = a;
			byteIterator++;

			memcpy(dest + byteIterator, &color.r, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.g, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.b, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.a, sizeof(float));
			byteIterator += sizeof(float);

			meshColors++;
		}
	}

	//Now we know how many meshes need updating
	dest[meshColorsStart] = meshColors;

	glm::vec4 highlightColor;
	float highlightThickness;
	bool hasHighlight = modelInstance->getHighlight(highlightColor, highlightThickness);

	dest[byteIterator] = hasHighlight ? 1 : 0;
	byteIterator++;

	if (hasHighlight)
	{
		memcpy(dest + byteIterator, &highlightColor.r, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.g, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.b, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.a, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightThickness, sizeof(float));
		byteIterator += sizeof(float);
	}

	memcpy(dest + byteIterator, &buoyancy, sizeof(float));
	byteIterator += sizeof(float);

	dest[byteIterator] = (unsigned char)meshDecals.size();
	byteIterator++;

	for (const auto& [meshIdx, decalName] : meshDecals)
	{
		dest[byteIterator] = (unsigned char)meshIdx;
		dest[byteIterator + 1] = (unsigned char)decalName.length();
		memcpy(dest + byteIterator + 2, decalName.data(), decalName.length());
		byteIterator += 2 + decalName.length();
	}

	dest[byteIterator] = (unsigned char)nameTag.length();
	byteIterator++;

	if (!nameTag.empty())
	{
		memcpy(dest + byteIterator, &nameTagColor.r, sizeof(float) * 3);
		byteIterator += sizeof(float) * 3;
		memcpy(dest + byteIterator, nameTag.data(), nameTag.length());
		byteIterator += nameTag.length();
	}

	dest[byteIterator] = (unsigned char)getKind();
	byteIterator++;
	addKindCreationData(dest + byteIterator);
}

void Dynamic::requestDestruction()
{
	((std::shared_ptr<SimObject>*)body->getUserPointer())->reset();
}

Dynamic::~Dynamic()
{
	if (modelInstance)
		delete modelInstance;

	if (body)
	{
		if (body->getUserPointer())
		{
			std::shared_ptr<SimObject>* userDataPtr = (std::shared_ptr<SimObject>*)body->getUserPointer();
			delete userDataPtr;
		}
		if (inWorld)
			world->removeBody(body);
		delete body;
	}
}

std::shared_ptr<Dynamic> dynamicFromBody(const btRigidBody* in)
{
	if (in->getUserIndex() != dynamicBody)
	{
		error("btRigidBody expected to have dynamic type but didn't");
		return nullptr;
	}

	if (!in->getUserPointer())
		return nullptr;

	std::shared_ptr<SimObject>* userDataPointer = (std::shared_ptr<SimObject>*)in->getUserPointer();
	
	if (!(*userDataPointer))
		return nullptr;

	return std::dynamic_pointer_cast<Dynamic>(*userDataPointer);
}
