#include "Item.h"
#include "../GameLoop/ClientData.h"

#include <cmath>

//The old game's swing speed, a bit over a fifth of a second from rest, down to the ground, and back
static constexpr float swingRadiansPerMS = 0.03f;

//How far the head tips toward the ground at the bottom of a swing
static const float swingDepth = glm::radians(90.0f);

//A kick snaps back in a fraction of this and eases home over the rest, quick enough to be over before an automatic's next shot
static constexpr float kickLengthMS = 90.0f;
static constexpr float kickRiseMS = 18.0f;

//Animations go in a byte: 0 for none, 1 for the swing, 2 and up for the model's own, and 255 for the kick, which games
//from before it take for a model animation they don't have and ignore
static unsigned char encodeAnimation(int id)
{
	if (id == itemSwingAnimation)
		return 1;
	if (id == itemKickAnimation)
		return 255;
	if (id < 0 || id > 252)
		return 0;
	return (unsigned char)(id + 2);
}

static int decodeAnimation(unsigned char value)
{
	if (value == 0)
		return itemNoAnimation;
	if (value == 1)
		return itemSwingAnimation;
	if (value == 255)
		return itemKickAnimation;
	return value - 2;
}

Item::Item(std::shared_ptr<DynamicType> _type, const btVector3& initialPos, const btQuaternion& initialRot)
	: Dynamic(_type, initialPos, initialRot)
{
}

bool Item::isHeld() const
{
	if (type->getModel()->isServerSide())
		return !owner.expired();
	return held;
}

std::shared_ptr<Dynamic> Item::getHolder() const
{
	std::shared_ptr<ClientData> carrier = owner.lock();
	if (!carrier || carrier->controllers.empty())
		return nullptr;
	return carrier->controllers[0].target.lock();
}

void Item::makeDisplay(netIDType brickID)
{
	display = true;
	displayBrickID = brickID;
	applyDisplayBody();
}

void Item::applyDisplayBody()
{
	if (!body)
		return;

	/*
		No contact response keeps it out of everyone's way while rays still find it, like a brick that doesn't collide, and
		with no gravity and nothing pushing it (see LoopServer's water and Lua's radiusImpulse) it stays put and soon falls asleep
		It isn't taken out of the simulation altogether: Bullet only refreshes the broadphase box of an active body, and a body
		is added to the world before it's moved to where it starts, so a body that never simulates keeps a box at the origin
		and rays never find it. The server refreshes the box here as well, so Lua can raycast it the same tick it's made;
		a client's copy is refreshed by its first physics step, long before anyone's crosshair looks for it
	*/
	body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	body->setGravity(btVector3(0, 0, 0));
	body->setLinearVelocity(btVector3(0, 0, 0));
	body->setAngularVelocity(btVector3(0, 0, 0));
	gravityUpdated = true;

	if (world && inWorld && type->getModel()->isServerSide())
		world->getDynamicsWorld()->updateSingleAabb(body);
}

bool Item::isEquipped() const
{
	std::shared_ptr<ClientData> carrier = owner.lock();
	if (!carrier)
		return false;

	//An item Lua put in their hand is held instead of whatever their item bar has picked, see ClientData::setHandItem
	if (std::shared_ptr<Item> hand = carrier->handItem.lock())
		return hand.get() == this;

	return carrier->inventoryOpen && carrier->selectedSlot == slot;
}

void Item::playAnimation(int id, bool loop)
{
	if (loop)
	{
		loopAnimation = id;
		return;
	}

	oneShotItemAnimation = id;
	oneShotItemCount++;
}

void Item::stopAnimation(int id)
{
	if (id == itemNoAnimation || id == loopAnimation)
		loopAnimation = itemNoAnimation;
}

void Item::writeState(enet_uint8* dest) const
{
	std::shared_ptr<Dynamic> holderDynamic = getHolder();
	netIDType holderNetID = holderDynamic ? holderDynamic->getID() : NO_ID;
	memcpy(dest, &holderNetID, sizeof(netIDType));
	unsigned int at = sizeof(netIDType);

	dest[at++] = (isHeld() ? ItemFlag_Held : 0) | (isEquipped() ? ItemFlag_Equipped : 0) | (display ? ItemFlag_Display : 0);
	dest[at++] = slot < 0 ? 255 : (unsigned char)slot;
	dest[at++] = encodeAnimation(loopAnimation);
	dest[at++] = encodeAnimation(oneShotItemAnimation);
	dest[at++] = oneShotItemCount;

	//Where it comes back out into the world when it's dropped
	const btTransform& transform = body->getWorldTransform();
	const btQuaternion& rotation = transform.getRotation();
	addPosition(dest + at, b2g3(transform.getOrigin()));
	at += PositionBytes;
	addQuaternion(dest + at, glm::quat(rotation.w(), rotation.x(), rotation.y(), rotation.z()));
}

void Item::addKindCreationData(enet_uint8* dest) const
{
	writeState(dest);
}

/*
	1 byte			-	packet type
	4 bytes			-	item net ID
	stateBytes		-	see writeState
*/
ENetPacket* Item::makeStatePacket()
{
	std::shared_ptr<Dynamic> holderDynamic = getHolder();
	sentHolderID = holderDynamic ? holderDynamic->getID() : NO_ID;
	sentEquipped = isEquipped();

	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(netIDType) + stateBytes, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)ItemState;
	memcpy(ret->data + 1, &netID, sizeof(netIDType));
	writeState(ret->data + 1 + sizeof(netIDType));
	return ret;
}

void Item::readState(enet_uint8* src, bool creating, float idealBufferSize)
{
	memcpy(&holderID, src, sizeof(netIDType));
	unsigned int at = sizeof(netIDType);

	unsigned char flags = src[at++];
	unsigned char slotByte = src[at++];
	int newLoopAnimation = decodeAnimation(src[at++]);
	int newOneShotAnimation = decodeAnimation(src[at++]);
	unsigned char newOneShotCount = src[at++];

	glm::vec3 position;
	::getPosition(src + at, position);
	at += PositionBytes;
	glm::quat rotation;
	getQuaternion(src + at, rotation);

	held = flags & ItemFlag_Held;
	equipped = flags & ItemFlag_Equipped;
	slot = slotByte == 255 ? -1 : slotByte;

	//Our own body stops bumping our player around too, since we simulate our player against what's here
	if ((flags & ItemFlag_Display) && !display)
	{
		display = true;
		applyDisplayBody();
	}

	if (held && isInWorld())
		removeFromWorld();
	else if (!held && !isInWorld())
	{
		//Back on the ground where it was dropped, rather than gliding over from wherever it was before it was picked up
		btTransform transform;
		transform.setOrigin(g2b3(position));
		transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
		returnToWorld(transform);

		interpolator.reset();
		interpolator.addSnapshot(position, rotation, idealBufferSize, 0);
		renderedTransformInitialized = false;
		renderedTilt = glm::quat(1, 0, 0, 0);
	}

	if (newLoopAnimation != loopAnimation)
	{
		stopClientAnimation(loopAnimation);
		loopAnimation = newLoopAnimation;
		startClientAnimation(loopAnimation, true);
	}

	//Someone who just got here didn't see the last one
	if (newOneShotCount != oneShotItemCount)
	{
		oneShotItemCount = newOneShotCount;
		oneShotItemAnimation = newOneShotAnimation;
		if (!creating)
			startClientAnimation(oneShotItemAnimation, false);
	}
}

void Item::startClientAnimation(int id, bool loop)
{
	if (id == itemSwingAnimation)
	{
		swinging = true;
		swingLooping = swingLooping || loop;
		return;
	}

	//Each shot kicks again from the start, so an automatic's kicks don't run together into a lean
	if (id == itemKickAnimation)
	{
		kicking = true;
		kickMS = 0;
		kickLooping = kickLooping || loop;
		return;
	}

	if (id < 0 || id >= (int)type->getModel()->animations.size())
		return;

	if (loop)
		play(id, true);
	else
		modelInstance->restartAnimation(id);
}

void Item::stopClientAnimation(int id)
{
	//Finishes the swing it's partway through instead of snapping back
	if (id == itemSwingAnimation)
		swingLooping = false;
	else if (id == itemKickAnimation)
		kickLooping = false;
	else if (id >= 0 && id < (int)type->getModel()->animations.size())
		stop(id);
}

void Item::updateSwing(float deltaT)
{
	if (kicking)
	{
		kickMS += deltaT;
		if (kickMS >= kickLengthMS)
		{
			kickMS = kickLooping ? std::fmod(kickMS, kickLengthMS) : 0;
			kicking = kickLooping;
		}
	}

	if (!swinging)
		return;

	swingPhase += deltaT * swingRadiansPerMS;
	if (swingPhase < glm::two_pi<float>())
		return;

	if (swingLooping)
		swingPhase = std::fmod(swingPhase, glm::two_pi<float>());
	else
	{
		swingPhase = 0;
		swinging = false;
	}
}

float Item::getSwingAngle() const
{
	return -swingDepth * (1.0f - std::cos(swingPhase)) * 0.5f;
}

float Item::getKickAmount() const
{
	if (!kicking)
		return 0;

	//Straight back in an instant, then settling home more and more slowly
	if (kickMS < kickRiseMS)
		return kickMS / kickRiseMS;

	float home = (kickMS - kickRiseMS) / (kickLengthMS - kickRiseMS);
	return (1.0f - home) * (1.0f - home);
}
