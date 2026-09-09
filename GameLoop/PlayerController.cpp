#include "PlayerController.h"


//Client only, send last inputs to server for caching and reflection
//Can return nullptr if object was deleted or packet was recently sent
//Always resends the current full state (not just on change) since this goes out unreliably -
//that way one dropped packet only leaves the server stale for one more interval instead of
//potentially forever if the player holds a key with no further state changes to trigger a resend
ENetPacket* PlayerController::makeMovementInputsPacket()
{
	if (getTicksMS() - lastSentControls < 100)
		return nullptr;

	lastSentControls = getTicksMS();

	std::shared_ptr<Dynamic> targetLock = target.lock();
	if (!targetLock)
		return nullptr;

	return makeMovementInputs(
		targetLock->getID(),
		lastJump,
		lastForward,
		lastBackward,
		lastLeft,
		lastRight,
		lastCameraDirection
	);
}

//Server only wrapper
bool PlayerController::controlWithLastInput(std::shared_ptr<PhysicsWorld> world, float deltaT)
{
	serverSide = true;
	return control(world, deltaT, lastCameraDirection, lastJump, lastForward, lastBackward, lastLeft, lastRight);
}

//Server and client side, called per frame, server caches last inputs from clients
bool PlayerController::control(std::shared_ptr<PhysicsWorld> world, float deltaT, glm::vec3 cameraDirection, bool jump, bool forward, bool backward, bool left, bool right)
{
	lastCameraDirection = cameraDirection;
	lastJump = jump;
	lastForward = forward;
	lastBackward = backward;
	lastLeft = left;
	lastRight = right;

	//Prevent huge deltaTs from causing huge jumps (like when debugging and pausing the game for a while)
	deltaT = std::clamp(deltaT, 0.0f, 33.0f);

	std::shared_ptr<Dynamic> targetLock = target.lock();
	if (!targetLock)
		return true;

	targetLock->body->activate();

	if (jump)
	{
		//Make sure we are standing on the ground before we try and jump
		btTransform feetStart = targetLock->body->getWorldTransform();
		btTransform feetEnd = targetLock->body->getWorldTransform();
		feetStart.setOrigin(feetEnd.getOrigin() + btVector3(0.0,  1.0, 0.0));
		feetEnd.setOrigin(feetEnd.getOrigin()   + btVector3(0.0, -1.0, 0.0));
		btVector3 boxSize = g2b3(targetLock->getType()->getModel()->getColHalfExtents());

		btRigidBody *sweepResult = world->boxSweepTest(boxSize, feetStart, feetEnd, targetLock->body);

		//TODO: Check if we're on the ground
		if(sweepResult)
			targetLock->body->applyCentralImpulse(btVector3(0, 30, 0));
	}


	//TODO: Move this to a constructor or something
	targetLock->body->setAngularFactor(btVector3(0, 0, 0));

	float speed = 10.0;
	float blendTime = 50.0; //MS

	btVector3 dir = g2b3(cameraDirection);
	dir.setY(0);
	dir = dir.normalized();
	float cameraYaw = atan2(dir.getX(), dir.getZ());

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

	if (!(leftRightUsed || forwardBackUsed))
	{
		targetLock->body->setFriction(1.0);
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

	//TODO: This LERP isn't right
	playerYaw = playerYaw.slerp(turn, deltaT / blendTime);

	btVector3 walkDir = btMatrix3x3(turn) * btVector3(0.0, 0.0, -1.0);

	if (!serverSide)
	{
		//Don't want to compete with ControlledPhysics packets from the same client
		btTransform t = targetLock->body->getWorldTransform();
		t.setRotation(playerYaw);
		targetLock->body->setWorldTransform(t);
	}

	btVector3 oldVel = targetLock->getVelocity();
	//TODO: This LERP isn't right
	btVector3 newVel = oldVel.lerp(walkDir * speed, deltaT / blendTime);
	newVel.setY(oldVel.getY());
	targetLock->setVelocity(newVel);

	return false;
}

/*
	Client side wrapper
	Call for each controller each frame, returns true if weak_ptr lock expired
*/
bool PlayerController::control(const std::shared_ptr<InputMap> input, const std::shared_ptr<Camera> camera, float deltaT, std::shared_ptr<PhysicsWorld> world)
{
	serverSide = false;
	return control(world, deltaT, camera->getDirection(), input->pollCommand(Jump), input->isCommandKeydown(WalkForward), input->isCommandKeydown(WalkBackward), input->isCommandKeydown(WalkLeft), input->isCommandKeydown(WalkRight));
}