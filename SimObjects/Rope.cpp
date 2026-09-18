#include "Rope.h"
#include "../Bricks/BrickHolder.h"

//Fixed bytes of state, before the bends
static constexpr unsigned int fixedStateBytes = RopeAnchor::packetBytes * 2 + sizeof(float) * 2 + 1 + 4 + 1;

Rope::Rope()
{
	ropeWorld = world;
}

void Rope::changed()
{
	updatesLeft = resendCount;
	physicsChanged = true;
}

void Rope::tieToPoint(int end, const glm::vec3& position)
{
	RopeAnchor& anchor = anchors[end];
	anchor.kind = RopeAnchorFixed;
	anchor.id = NO_ID;
	anchor.offset = position;
	anchor.dynamic.reset();
	anchor.vehicle.reset();
	changed();
}

void Rope::tieToDynamic(int end, const std::shared_ptr<Dynamic>& target, const glm::vec3& offset)
{
	RopeAnchor& anchor = anchors[end];
	anchor.kind = RopeAnchorDynamic;
	anchor.id = target->getID();
	anchor.offset = offset;
	anchor.dynamic = target;
	anchor.vehicle.reset();
	changed();
}

void Rope::tieToBrick(int end, netIDType brickID, const glm::vec3& position)
{
	tieToPoint(end, position);
	anchors[end].kind = RopeAnchorBrick;
	anchors[end].id = brickID;
}

void Rope::tieToVehicle(int end, const std::shared_ptr<Vehicle>& target, const glm::vec3& offset)
{
	RopeAnchor& anchor = anchors[end];
	anchor.kind = RopeAnchorVehicle;
	anchor.id = target->getID();
	anchor.offset = offset;
	anchor.dynamic.reset();
	anchor.vehicle = target;
	changed();
}

void Rope::drawOn(int end, const std::shared_ptr<Dynamic>& target, const glm::vec3& offset)
{
	RopeAnchor& anchor = anchors[end];
	anchor.drawnOnID = target ? target->getID() : NO_ID;
	anchor.drawnOffset = target ? offset : glm::vec3(0);
	anchor.drawnOn = target;
	updatesLeft = resendCount;
}

void Rope::setLength(float _length)
{
	_length = std::clamp(_length, 0.0f, maxLength);
	if (_length == length)
		return;

	length = _length;
	changed();
}

void Rope::setLinks(int _links)
{
	links = (unsigned char)std::clamp(_links, minLinks, maxLinks);
	updatesLeft = resendCount;
}

void Rope::setWidth(float _width)
{
	width = std::clamp(_width, 0.01f, 10.0f);
	updatesLeft = resendCount;
}

void Rope::setColor(const glm::u8vec4& _color)
{
	if (_color == color)
		return;

	color = _color;
	updatesLeft = resendCount;
}

void Rope::setBends(const std::vector<glm::vec3>& _bends)
{
	bends = _bends;
	if (bends.size() > (size_t)maxBends)
		bends.resize(maxBends);
	changed();
}

glm::vec3 Rope::getEnd(int end) const
{
	const RopeAnchor& anchor = anchors[end];

	if (anchor.kind == RopeAnchorDynamic)
	{
		std::shared_ptr<Dynamic> target = anchor.dynamic.lock();
		if (target && target->body)
			return b2g3(target->body->getWorldTransform() * g2b3(anchor.offset));
	}
	else if (anchor.kind == RopeAnchorVehicle)
	{
		std::shared_ptr<Vehicle> target = anchor.vehicle.lock();
		if (target && target->body)
			return b2g3(target->body->getWorldTransform() * g2b3(anchor.offset));
	}

	return anchor.offset;
}

glm::vec3 Rope::getDrawnEnd(int end, bool tied) const
{
	const RopeAnchor& anchor = anchors[end];

	if (std::shared_ptr<Dynamic> drawn = tied ? nullptr : anchor.drawnOn.lock())
		return drawn->getMeshCenter(-1) + drawn->getMeshRotation(-1) * anchor.drawnOffset;

	if (anchor.kind == RopeAnchorDynamic)
	{
		if (std::shared_ptr<Dynamic> target = anchor.dynamic.lock())
			return target->getMeshCenter(-1) + target->getMeshRotation(-1) * anchor.offset;
	}
	else if (anchor.kind == RopeAnchorVehicle)
	{
		if (std::shared_ptr<Vehicle> target = anchor.vehicle.lock())
			return target->renderedPosition + target->renderedRotation * anchor.offset;
	}

	return anchor.offset;
}

bool Rope::isDrawable() const
{
	for (const RopeAnchor& anchor : anchors)
	{
		if (anchor.kind == RopeAnchorDynamic && anchor.dynamic.expired())
			return false;
		if (anchor.kind == RopeAnchorVehicle && anchor.vehicle.expired())
			return false;
		if (anchor.drawnOnID != NO_ID && anchor.drawnOn.expired())
			return false;
	}

	return true;
}

float Rope::getPathLength() const
{
	glm::vec3 last = getEnd(0);
	float total = 0;
	for (const glm::vec3& bend : bends)
	{
		total += glm::distance(last, bend);
		last = bend;
	}
	return total + glm::distance(last, getEnd(1));
}

bool Rope::isAnchorGone(const BrickHolder* bricks) const
{
	for (const RopeAnchor& anchor : anchors)
	{
		if (anchor.kind == RopeAnchorDynamic && anchor.dynamic.expired())
			return true;
		if (anchor.kind == RopeAnchorVehicle && anchor.vehicle.expired())
			return true;
		if (anchor.kind == RopeAnchorBrick && bricks && !bricks->find(anchor.id))
			return true;
	}

	return false;
}

btRigidBody* Rope::getAnchorBody(int end) const
{
	const RopeAnchor& anchor = anchors[end];

	if (anchor.kind == RopeAnchorDynamic)
	{
		std::shared_ptr<Dynamic> target = anchor.dynamic.lock();
		return target && target->isInWorld() ? target->body : nullptr;
	}

	if (anchor.kind == RopeAnchorVehicle)
	{
		std::shared_ptr<Vehicle> target = anchor.vehicle.lock();
		return target ? target->body : nullptr;
	}

	return &btTypedConstraint::getFixedBody();
}

void Rope::dropConstraint()
{
	if (!constraint)
		return;

	if (ropeWorld)
		ropeWorld->removeRope(constraint);
	delete constraint;
	constraint = nullptr;
}

void Rope::updatePhysics(const ObjHolder<Dynamic>* dynamics, const ObjHolder<Vehicle>* vehicles)
{
	//The server tied them by pointer, a client only has the IDs, and what they belong to can arrive after the rope does
	for (RopeAnchor& anchor : anchors)
	{
		if (anchor.kind == RopeAnchorDynamic && dynamics)
		{
			std::shared_ptr<Dynamic> target = anchor.dynamic.lock();
			if (!target || target->getID() != anchor.id)
			{
				anchor.dynamic = dynamics->find(anchor.id);
				physicsChanged = true;
			}
		}
		else if (anchor.kind == RopeAnchorVehicle && vehicles)
		{
			std::shared_ptr<Vehicle> target = anchor.vehicle.lock();
			if (!target || target->getID() != anchor.id)
			{
				anchor.vehicle = vehicles->find(anchor.id);
				physicsChanged = true;
			}
		}

		if (anchor.drawnOnID != NO_ID && dynamics)
		{
			std::shared_ptr<Dynamic> drawn = anchor.drawnOn.lock();
			if (!drawn || drawn->getID() != anchor.drawnOnID)
				anchor.drawnOn = dynamics->find(anchor.drawnOnID);
		}
		else if (anchor.drawnOnID == NO_ID)
			anchor.drawnOn.reset();
	}

	btRigidBody* bodyA = getAnchorBody(0);
	btRigidBody* bodyB = getAnchorBody(1);

	//Two spots in the world hold themselves apart
	bool wanted = bodyA && bodyB && bodyA != bodyB;

	//PhysicsWorld took it out along with one of its bodies, or its ends are tied to something else now
	if (constraint && (!wanted || !constraint->inWorld || &constraint->getRigidBodyA() != bodyA || &constraint->getRigidBodyB() != bodyB))
		dropConstraint();

	if (!wanted || !ropeWorld)
		return;

	if (!constraint)
	{
		constraint = new RopeConstraint(*bodyA, *bodyB, g2b3(anchors[0].offset), g2b3(anchors[1].offset), length);
		ropeWorld->addRope(constraint);
		physicsChanged = true;
	}

	if (physicsChanged)
	{
		constraint->setPivotA(g2b3(anchors[0].offset));
		constraint->setPivotB(g2b3(anchors[1].offset));
		constraint->setMaxLength(length);

		std::vector<btVector3> path;
		for (const glm::vec3& bend : bends)
			path.push_back(g2b3(bend));
		constraint->setBends(path);

		//Something asleep at the end of a rope that just got shorter has to notice
		bodyA->activate();
		bodyB->activate();
		physicsChanged = false;
	}
}

unsigned int Rope::getStateBytes() const
{
	return fixedStateBytes + (unsigned int)bends.size() * sizeof(float) * 3;
}

unsigned int Rope::readStateBytes(const enet_uint8* src, unsigned int available)
{
	if (available < fixedStateBytes)
		return 0;

	unsigned int bendCount = src[fixedStateBytes - 1];
	unsigned int bytes = fixedStateBytes + bendCount * sizeof(float) * 3;
	return bendCount <= (unsigned int)maxBends && bytes <= available ? bytes : 0;
}

void Rope::writeState(enet_uint8* dest) const
{
	unsigned int at = 0;
	for (const RopeAnchor& anchor : anchors)
	{
		dest[at] = anchor.kind;
		at++;
		memcpy(dest + at, &anchor.id, sizeof(netIDType));
		at += sizeof(netIDType);
		memcpy(dest + at, &anchor.offset[0], sizeof(float) * 3);
		at += sizeof(float) * 3;
		memcpy(dest + at, &anchor.drawnOnID, sizeof(netIDType));
		at += sizeof(netIDType);
		memcpy(dest + at, &anchor.drawnOffset[0], sizeof(float) * 3);
		at += sizeof(float) * 3;
	}

	memcpy(dest + at, &length, sizeof(float));
	at += sizeof(float);
	memcpy(dest + at, &width, sizeof(float));
	at += sizeof(float);
	dest[at] = links;
	at++;
	memcpy(dest + at, &color[0], 4);
	at += 4;

	dest[at] = (enet_uint8)bends.size();
	at++;
	for (const glm::vec3& bend : bends)
	{
		memcpy(dest + at, &bend[0], sizeof(float) * 3);
		at += sizeof(float) * 3;
	}
}

void Rope::readFromPacket(const enet_uint8* src)
{
	unsigned int at = 0;
	for (RopeAnchor& anchor : anchors)
	{
		anchor.kind = src[at] <= RopeAnchorVehicle ? (RopeAnchorKind)src[at] : RopeAnchorFixed;
		at++;
		memcpy(&anchor.id, src + at, sizeof(netIDType));
		at += sizeof(netIDType);
		memcpy(&anchor.offset[0], src + at, sizeof(float) * 3);
		at += sizeof(float) * 3;
		memcpy(&anchor.drawnOnID, src + at, sizeof(netIDType));
		at += sizeof(netIDType);
		memcpy(&anchor.drawnOffset[0], src + at, sizeof(float) * 3);
		at += sizeof(float) * 3;

		//updatePhysics finds whatever these belong to now
		if (anchor.kind != RopeAnchorDynamic)
			anchor.dynamic.reset();
		if (anchor.kind != RopeAnchorVehicle)
			anchor.vehicle.reset();
	}

	memcpy(&length, src + at, sizeof(float));
	at += sizeof(float);
	memcpy(&width, src + at, sizeof(float));
	at += sizeof(float);
	links = (unsigned char)std::clamp((int)src[at], minLinks, maxLinks);
	at++;
	memcpy(&color[0], src + at, 4);
	at += 4;

	unsigned int bendCount = std::min((unsigned int)src[at], (unsigned int)maxBends);
	at++;
	bends.resize(bendCount);
	for (glm::vec3& bend : bends)
	{
		memcpy(&bend[0], src + at, sizeof(float) * 3);
		at += sizeof(float) * 3;
	}

	if (!std::isfinite(length))
		length = 0;
	length = std::clamp(length, 0.0f, maxLength);

	physicsChanged = true;
}

bool Rope::requiresNetUpdate()
{
	flaggedForUpdate = updatesLeft > 0;
	return flaggedForUpdate;
}

unsigned int Rope::getCreationPacketBytes() const
{
	return sizeof(netIDType) + getStateBytes();
}

unsigned int Rope::getUpdatePacketBytes() const
{
	return getStateBytes();
}

void Rope::addToCreationPacket(enet_uint8* dest) const
{
	netIDType id = getID();
	memcpy(dest, &id, sizeof(netIDType));
	writeState(dest + sizeof(netIDType));
}

void Rope::addToUpdatePacket(enet_uint8* dest)
{
	writeState(dest);
	if (updatesLeft > 0)
		updatesLeft--;
}

void Rope::requestDestruction()
{
	dropConstraint();
	for (RopeAnchor& anchor : anchors)
	{
		anchor.dynamic.reset();
		anchor.vehicle.reset();
		anchor.drawnOn.reset();
	}
}

Rope::~Rope()
{
	dropConstraint();
}
