#include "PlayerController.h"

#include "../Bricks/Brick.h"

//Tallest ledge a walking player steps onto without jumping
static constexpr float maxStepHeight = 4 * PLATE_SIZE;

//Surfaces with a normal steeper than this are walls to step over, flatter ones are ground to step onto
static constexpr float walkableNormalY = 0.7f;

//With more than this much under water, but not all of it, the player can jump out of the water
static constexpr float treadWaterDepth = 0.25f;

//Top swimming speed, water drag keeps the real speed somewhat under it
static constexpr float swimSpeed = 10.0f;

//MS, about how long swimming takes to get up to speed or turn, slower than walking so it feels like water
static constexpr float swimBlendTime = 150.0f;

//MS after jumping out of the water before swimming takes over again
static constexpr unsigned int waterJumpMS = 400;

//Jets cancel gravity and push up this much more, like the old game's upward gravity of 20
static constexpr float jetLift = 20.0f;
//Climbing faster than this, jets only hold the speed instead of adding to it
static constexpr float jetMaxRiseSpeed = 30.0f;
//Moving while jetting heads toward twice walking speed, taking longer to change direction, like the old game in the air
static constexpr float jetSpeedMultiplier = 2.0f;
static constexpr float jetBlendTime = 150.0f;

//MS lying down or getting back up takes, like the old game's crouch
static constexpr float crawlBlendTime = 200.0f;

//How much of walking speed a player flat on the ground moves at
static constexpr float crawlSpeedMultiplier = 0.5f;

/*
	Top speed jets carry a player lying flat, world units a second, which is what their thrust builds up to and what
	holding a movement key heads for. Faster than jetting upright (walking speed times jetSpeedMultiplier): the whole
	point of flying flat out is that it beats flying along standing up
*/
static constexpr float crawlJetSpeed = 30.0f;

//How far a box of these half extents, turned this way, reaches up and down from its middle
static btScalar verticalHalfExtent(const btVector3& halfExtents, const btQuaternion& turn)
{
	btMatrix3x3 basis(turn);
	return std::abs(basis[1].getX()) * halfExtents.getX() + std::abs(basis[1].getY()) * halfExtents.getY() + std::abs(basis[1].getZ()) * halfExtents.getZ();
}

/*
	Whether a player lying down has room to stand up again: nothing solid over their collision box up to where a
	standing one's top would be. Until there is, letting go of the key leaves them down, like the old game's crouch

	Measured from where the box really is at whatever angle it's tipped to, not from a flat one: a box halfway up is
	as much taller as its middle is higher, so guessing either would move the answer around as the player rises and
	leave them flickering between lying and standing in a room that's tall enough
*/
static bool roomToStand(std::shared_ptr<PhysicsWorld> world, const std::shared_ptr<Dynamic>& player)
{
	std::shared_ptr<Model> model = player->getType()->getModel();
	btVector3 halfExtents = g2b3(model->getColHalfExtents());

	const btTransform& transform = player->body->getWorldTransform();
	btVector3 center = transform * g2b3(model->getColOffset());

	//setPose keeps the box standing on the same spot however it's turned, so this is the floor under them either way
	btScalar bottom = center.getY() - verticalHalfExtent(halfExtents, transform.getRotation());

	//Straight up the middle of where they'd stand, from just off the floor to the top of a standing box
	btVector3 from(center.getX(), bottom + 0.05f, center.getZ());
	btVector3 to(center.getX(), bottom + halfExtents.getY() * 2.0f, center.getZ());

	if (to.getY() <= from.getY())
		return true;

	return world->rayHitFraction(from, to, player->body, nullptr) >= 1;
}

/*
	Turns the player's body, collision box and all, keeping the box where it stood: its middle stays over the same
	spot and its lowest point stays at the same height, so lying down doesn't sink it into the ground or pop it out
*/
static void setPose(const std::shared_ptr<Dynamic>& player, const btQuaternion& rotation)
{
	std::shared_ptr<Model> model = player->getType()->getModel();
	btVector3 halfExtents = g2b3(model->getColHalfExtents());
	btVector3 boxOffset = g2b3(model->getColOffset());

	btTransform transform = player->body->getWorldTransform();
	btQuaternion turned = transform.getRotation();

	//Where the box's middle would go on its own, undone, plus however much taller or shorter the turned box is
	btVector3 stay = quatRotate(turned, boxOffset) - quatRotate(rotation, boxOffset);
	btScalar rise = verticalHalfExtent(halfExtents, rotation) - verticalHalfExtent(halfExtents, turned);

	transform.setOrigin(transform.getOrigin() + stay + btVector3(0, rise, 0));
	transform.setRotation(rotation);
	player->body->setWorldTransform(transform);
}

/*
	If the player is walking into a wall no taller than maxStepHeight with room above it, lifts them on top of it
	Works on anything solid except other dynamics, so walking into a loose object still pushes it
*/
static void stepUp(std::shared_ptr<PhysicsWorld> world, const std::shared_ptr<Dynamic>& player, const btVector3& walkDir)
{
	std::shared_ptr<Model> model = player->getType()->getModel();
	btVector3 halfExtents = g2b3(model->getColHalfExtents());

	btTransform bodyTransform = player->body->getWorldTransform();
	btTransform box = bodyTransform * btTransform(btQuaternion::getIdentity(), g2b3(model->getColOffset()));

	auto sweep = [&](const btVector3& from, const btVector3& to)
	{
		btTransform start = box;
		btTransform end = box;
		start.setOrigin(from);
		end.setOrigin(to);
		return world->boxSweep(halfExtents, start, end, player->body);
	};

	const btVector3 up = btVector3(0, 1, 0);
	const btVector3 center = box.getOrigin();

	//Slightly more than a frame of walking, so the step happens as the player reaches the ledge
	const btVector3 ahead = walkDir * 0.3f;

	//Only while standing on something, a jump or fall shouldn't grab ledges
	if (!sweep(center + up * 0.05f, center - up * 0.2f).body)
		return;

	SweepResult wall = sweep(center, center + ahead);
	if (!wall.body || wall.normal.getY() > walkableNormalY || wall.body->getUserIndex() == dynamicBody)
		return;

	//Probe from a little above the limit, a ledge exactly maxStepHeight tall would start the sweep already touching its top
	float probeHeight = maxStepHeight + 0.1f;

	//A low ceiling, like the top of a doorway, only lowers the probe, the step just can't rise past it
	//Stays well clear of it, sweeps count anything within the collision margin as touching
	SweepResult ceiling = sweep(center, center + up * probeHeight);
	if (ceiling.body)
		probeHeight = probeHeight * ceiling.fraction - 0.1f;

	//Room above the ledge
	btVector3 raised = center + up * probeHeight;
	if (probeHeight < 0.06f || sweep(raised, raised + ahead).body)
		return;

	SweepResult ledge = sweep(raised + ahead, center + ahead);
	if (!ledge.body || ledge.normal.getY() < walkableNormalY)
		return;

	float rise = probeHeight * (1.0f - ledge.fraction);
	if (rise < 0.05f || rise > maxStepHeight + 0.02f || rise + 0.01f > probeHeight)
		return;

	bodyTransform.setOrigin(bodyTransform.getOrigin() + up * (rise + 0.01f));
	player->body->setWorldTransform(bodyTransform);

	btVector3 velocity = player->getVelocity();
	velocity.setY(std::max(velocity.getY(), (btScalar)0));
	player->setVelocity(velocity);
}

/*
	Forward and backward follow the camera up and down as well, left and right stay level, jump held swims straight up
	While swimming the player holds their depth instead of sinking or floating up, letting go hands them back to buoyancy
*/
static void swim(const std::shared_ptr<Dynamic>& player, float deltaT, glm::vec3 cameraDirection, bool jumpHeld, bool forward, bool backward, bool left, bool right, btScalar submerged)
{
	const btVector3 up = btVector3(0, 1, 0);

	btVector3 look = g2b3(cameraDirection);
	if (look.length2() < 0.0001f)
		return;
	look.normalize();

	btVector3 side = look.cross(up);
	//Looking straight up or down
	if (side.length2() < 0.0001f)
		side = btVector3(1, 0, 0);
	else
		side.normalize();

	btVector3 swimDir = look * (float(forward) - float(backward)) + side * (float(right) - float(left));
	if (jumpHeld)
		swimDir += up;

	if (swimDir.length2() > 0.0001f)
		swimDir.normalize();
	else
		swimDir.setZero();

	btRigidBody* body = player->body;
	if (deltaT > 0 && body->getInvMass() > 0)
	{
		//Cancel out gravity and whatever the water holds up, which Dynamic::applyWaterForces adds on both client and server
		btScalar mass = 1.0f / body->getInvMass();
		body->applyCentralForce(-body->getGravity() * mass * (1 - player->buoyancy * submerged));
	}

	btScalar blend = 1.0f - std::exp(-deltaT / swimBlendTime);
	player->setVelocity(player->getVelocity().lerp(swimDir * swimSpeed, blend));
}

//Client only, send last inputs to server for caching and reflection
//Can return nullptr if object was deleted or packet was recently sent
//Always resends the current full state (not just on change) since this goes out unreliably -
//that way one dropped packet only leaves the server stale for one more interval instead of
//potentially forever if the player holds a key with no further state changes to trigger a resend
ENetPacket* PlayerController::makeMovementInputsPacket()
{
	unsigned char flags = (lastJump ? MovementFlag_Jump : 0) | (lastForward ? MovementFlag_Forward : 0) | (lastBackward ? MovementFlag_Backward : 0) |
		(lastLeft ? MovementFlag_Left : 0) | (lastRight ? MovementFlag_Right : 0) | (lastJumpHeld ? MovementFlag_JumpHeld : 0) | (lastJet ? MovementFlag_Jet : 0) |
		(lastCrawl ? MovementFlag_Crawl : 0);

	//Jets starting or stopping go out right away, so the flames under the player don't lag behind, and so does any other key, which steers a vehicle
	//While left mouse is held they go out more often, so scripts following the crosshair, like the paint can, keep up with it
	if (getTicksMS() - lastSentControls < (sendQuickly ? 30u : 100u) && flags == lastSentFlags)
		return nullptr;

	lastSentControls = getTicksMS();
	lastSentJet = lastJet;
	lastSentFlags = flags;

	std::shared_ptr<Dynamic> targetLock = target.lock();
	if (!targetLock)
		return nullptr;

	return makeMovementInputs(
		targetLock->getID(),
		lastJump,
		lastJumpHeld,
		lastForward,
		lastBackward,
		lastLeft,
		lastRight,
		lastJet,
		lastCrawl,
		lastCameraDirection,
		lastCameraPosition
	);
}

//Server only wrapper
bool PlayerController::controlWithLastInput(std::shared_ptr<PhysicsWorld> world, float deltaT, float waterLevel)
{
	serverSide = true;
	return control(world, deltaT, lastCameraDirection, lastCameraPosition, lastJump, lastJumpHeld, lastForward, lastBackward, lastLeft, lastRight, lastJet, lastCrawl, waterLevel);
}

//Server and client side, called per frame, server caches last inputs from clients
bool PlayerController::control(std::shared_ptr<PhysicsWorld> world, float deltaT, glm::vec3 cameraDirection, glm::vec3 cameraPosition, bool jump, bool jumpHeld, bool forward, bool backward, bool left, bool right, bool jet, bool crawl, float waterLevel)
{
	lastCameraDirection = cameraDirection;
	lastCameraPosition = cameraPosition;
	lastJump = jump;
	lastJumpHeld = jumpHeld;
	lastForward = forward;
	lastBackward = backward;
	lastLeft = left;
	lastRight = right;
	lastJet = jet;
	lastCrawl = crawl;
	jumped = false;

	//Prevent huge deltaTs from causing huge jumps (like when debugging and pausing the game for a while)
	deltaT = std::clamp(deltaT, 0.0f, 33.0f);

	std::shared_ptr<Dynamic> targetLock = target.lock();
	if (!targetLock)
		return true;

	//Turns the player's head, see Dynamic::lookDirection
	if (glm::length(cameraDirection) > 0.0001f && !glm::any(glm::isnan(cameraDirection)))
	{
		targetLock->lookDirection = glm::normalize(cameraDirection);
		targetLock->hasLook = true;
	}

	//Out of the physics world while it drives a vehicle, which takes these keys instead, see LoopServer::updateVehicles
	if (!targetLock->isInWorld())
	{
		targetLock->playWalkingAnimation = false;
		targetLock->stop(0);
		return false;
	}

	targetLock->body->activate();

	btScalar submerged = targetLock->getSubmergedFraction(waterLevel);

	if (jump)
	{
		//Make sure we are standing on the ground before we try and jump
		btTransform feetStart = targetLock->body->getWorldTransform();
		btTransform feetEnd = targetLock->body->getWorldTransform();
		feetStart.setOrigin(feetEnd.getOrigin() + btVector3(0.0,  1.0, 0.0));
		feetEnd.setOrigin(feetEnd.getOrigin()   + btVector3(0.0, -1.0, 0.0));
		btVector3 boxSize = g2b3(targetLock->getType()->getModel()->getColHalfExtents());

		btRigidBody *sweepResult = world->boxSweepTest(boxSize, feetStart, feetEnd, targetLock->body);

		//Or treading water with their head above it, so they can climb out onto something
		bool treadingWater = submerged > treadWaterDepth && submerged < 1;

		//TODO: Check if we're on the ground
		if (sweepResult || treadingWater)
		{
			targetLock->body->applyCentralImpulse(btVector3(0, 30, 0));
			jumped = true;

			if (!sweepResult)
				lastWaterJump = getTicksMS();
		}
	}

	//Past Dynamic::swimDepth they go wherever the camera points
	bool swimming = submerged >= Dynamic::swimDepth &&getTicksMS() - lastWaterJump >= waterJumpMS;

	btVector3 dir = g2b3(cameraDirection);
	dir.setY(0);
	dir = dir.length2() > 0.00000001f ? dir.normalized() : btVector3(0, 0, 1);
	float cameraYaw = atan2(dir.getX(), dir.getZ());

	/*
		Lying down, on land only - water already decides how the player moves. Getting back up waits for the room to do it,
		so crawling under something doesn't push them through it
	*/
	bool lyingDown = crawl && !swimming;
	if (!lyingDown && crawlProgress > 0.5f && !roomToStand(world, targetLock))
		lyingDown = true;

	float lastCrawlProgress = crawlProgress;
	crawlProgress = std::clamp(crawlProgress + (lyingDown ? deltaT : -deltaT) / crawlBlendTime, 0.0f, 1.0f);
	bool crawling = crawlProgress > 0.5f;

	//The body's yaw, tipped forward as far as it has lain down, which turns its collision box with it
	btQuaternion pose = playerYaw * btQuaternion(btVector3(1, 0, 0), -SIMD_HALF_PI * crawlProgress);

	//Not while swimming, which already decides how the player moves
	bool jetting = jet && jetsAllowed && !swimming;
	if (jetting && deltaT > 0 && targetLock->body->getInvMass() > 0)
	{
		btRigidBody* body = targetLock->body;
		btScalar mass = 1.0f / body->getInvMass();
		btVector3 velocity = targetLock->getVelocity();

		//Standing, the jets lift. Lying down they push along the way the player faces instead, like the old game's crouch jet
		btScalar lift = velocity.getY() < jetMaxRiseSpeed ? jetLift : 0.0f;
		btScalar push = dir.dot(velocity) < crawlJetSpeed ? jetLift : 0.0f;
		btVector3 thrust = btVector3(0, lift * (1.0f - crawlProgress), 0) + dir * (push * crawlProgress);
		body->applyCentralForce((thrust - body->getGravity()) * mass);
	}

	//TODO: Move this to a constructor or something
	targetLock->body->setAngularFactor(btVector3(0, 0, 0));

	float speed = 10.0;
	float blendTime = 50.0; //MS

	//On foot, crawling is slower, all the way down to crawlSpeedMultiplier of walking flat on the ground
	float walkSpeed = speed * (1.0f - crawlProgress * (1.0f - crawlSpeedMultiplier));

	/*
		Jetting is its own speed rather than that one doubled: crawling slows the legs, not the jets, and a player
		flat out flies faster than an upright one. Heading for the same speed the thrust above builds to means
		holding a movement key steers the flight instead of dragging it back to a walk
	*/
	float jetSpeed = speed * jetSpeedMultiplier + (crawlJetSpeed - speed * jetSpeedMultiplier) * crawlProgress;

	if (faceCamera && !serverSide)
	{
		playerYaw = playerYaw.slerp(btQuaternion(3.1415 + cameraYaw, 0, 0), std::min(deltaT / blendTime, 1.0f));
		pose = playerYaw * btQuaternion(btVector3(1, 0, 0), -SIMD_HALF_PI * crawlProgress);

		//Don't want to compete with ControlledPhysics packets from the same client
		setPose(targetLock, pose);
	}
	else if (!serverSide && crawlProgress != lastCrawlProgress)
	{
		//Lying down or getting up turns the body even when nothing else would, like standing still in third person
		setPose(targetLock, pose);
	}

	bool leftRightUsed = false;
	bool forwardBackUsed = false;
	btQuaternion leftRightTurn, forwardBackTurn;

	if (forward)
	{
		forwardBackUsed = true;
		forwardBackTurn = btQuaternion(3.1415 + cameraYaw, 0, 0);
	}
	else if (backward)
	{
		forwardBackUsed = true;
		forwardBackTurn = btQuaternion(0 + cameraYaw, 0, 0);
	}

	if (left)
	{
		leftRightUsed = true;
		leftRightTurn = btQuaternion(3.0 * (3.1415 / 2.0) + cameraYaw, 0.0, 0.0);
	}
	else if (right)
	{
		leftRightUsed = true;
		leftRightTurn = btQuaternion(3.1415 / 2.0 + cameraYaw, 0.0, 0.0);
	}

	bool moving = leftRightUsed || forwardBackUsed;

	if (!moving && !(swimming && jumpHeld))
	{
		//Jets pushing a crawling player along shouldn't have to drag them over the ground
		targetLock->body->setFriction(crawling && jetting ? 0.0 : 1.0);
		targetLock->stop(0);
		targetLock->playWalkingAnimation = false;
		return false;
	}
	else
	{
		targetLock->body->setFriction(0.0);
		targetLock->play(0, true);
		targetLock->playWalkingAnimation = true;
	}

	if (swimming && !moving)
	{
		swim(targetLock, deltaT, cameraDirection, jumpHeld, forward, backward, left, right, submerged);
		return false;
	}

	btQuaternion turn;
	if (leftRightUsed)
	{
		if (forwardBackUsed)
			turn = leftRightTurn.slerp(forwardBackTurn, 0.5);
		else
			turn = leftRightTurn;
	}
	else if (forwardBackUsed)
		turn = forwardBackTurn;

	if (!faceCamera && !serverSide)
	{
		//TODO: This LERP isn't right
		playerYaw = playerYaw.slerp(turn, deltaT / blendTime);

		//Don't want to compete with ControlledPhysics packets from the same client
		setPose(targetLock, playerYaw * btQuaternion(btVector3(1, 0, 0), -SIMD_HALF_PI * crawlProgress));
	}

	if (swimming)
	{
		swim(targetLock, deltaT, cameraDirection, jumpHeld, forward, backward, left, right, submerged);
		return false;
	}

	btVector3 walkDir = btMatrix3x3(turn) * btVector3(0.0, 0.0, -1.0);

	//Someone flat on the ground doesn't pull themselves onto ledges, and their box isn't upright to sweep with anyway
	if (!crawling)
		stepUp(world, targetLock, walkDir);

	btVector3 oldVel = targetLock->getVelocity();
	float moveSpeed = jetting ? jetSpeed : walkSpeed;
	float moveBlendTime = jetting ? jetBlendTime : blendTime;
	//TODO: This LERP isn't right
	btVector3 newVel = oldVel.lerp(walkDir * moveSpeed, deltaT / moveBlendTime);
	newVel.setY(oldVel.getY());
	targetLock->setVelocity(newVel);

	return false;
}

/*
	Client side wrapper
	Call for each controller each frame, returns true if weak_ptr lock expired
*/
bool PlayerController::control(const std::shared_ptr<InputMap> input, const std::shared_ptr<Camera> camera, float deltaT, std::shared_ptr<PhysicsWorld> world, bool jet, bool crawl, float waterLevel)
{
	serverSide = false;
	faceCamera = camera->getFirstPerson() && camera->target.lock() == target.lock();

	//Dropping an item is Ctrl plus a key that walks forward by default, which shouldn't walk as well
	bool ctrlDown = SDL_GetModState() & KMOD_CTRL;
	bool forward = input->isCommandKeydown(WalkForward) && !(ctrlDown && input->getKeyBind(DropItem) == input->getKeyBind(WalkForward));

	return control(world, deltaT, camera->getDirection(), camera->getPosition(), input->pollCommand(Jump), input->isCommandKeydown(Jump), forward, input->isCommandKeydown(WalkBackward), input->isCommandKeydown(WalkLeft), input->isCommandKeydown(WalkRight), jet, crawl, waterLevel);
}
