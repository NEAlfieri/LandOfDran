#include "UpdateSimObjects.h"

bool UpdateSimObjectsPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	//TODO: Have server not send updates until we confirm we've loaded in
	if (cmdArgs.gameState != InGame)
		return true;

	//Too short to have any objects
	if (packet->dataLength < 4)
		return true;

	switch ((SimObjectType)packet->data[1])
	{
	case StaticTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;

		for (unsigned int a = 0; a < numObjects; a++)
		{
			//Ids are compressed by representing most of them using the difference between the last sent object in the packet
			lastId = simulation.dynamics->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			float friction, restitution;
			memcpy(&friction, packet->data + byteIterator, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(&restitution, packet->data + byteIterator, sizeof(float));
			byteIterator += sizeof(float);

			unsigned char flags = packet->data[byteIterator];
			byteIterator++;

			std::shared_ptr<StaticObject> toUpdate = simulation.statics->find(lastId);

			if (toUpdate)
			{
				toUpdate->body->setFriction(friction);
				toUpdate->body->setRestitution(restitution);

				if(flags & 1)
					toUpdate->setHidden(true);
				else
					toUpdate->setHidden(false);
				if(flags & 2)
					toUpdate->setColliding(true);
				else
					toUpdate->setColliding(false);
			}
		}

		//Hiding a static changes shadows, see LoopClient::renderEverything
		simulation.staticsChanged++;
		break;
	}
	case LightTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;

		for (unsigned int a = 0; a < numObjects; a++)
		{
			lastId = simulation.lights->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			if (byteIterator + Light::packetBytes > packet->dataLength)
				break;

			std::shared_ptr<Light> toUpdate = simulation.lights->find(lastId);
			if (toUpdate)
				toUpdate->readFromPacket(packet->data + byteIterator);

			byteIterator += Light::packetBytes;
		}

		break;
	}
	case EmitterTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;

		for (unsigned int a = 0; a < numObjects; a++)
		{
			lastId = simulation.emitters->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			if (byteIterator + Emitter::packetBytes > packet->dataLength)
				break;

			std::shared_ptr<Emitter> toUpdate = simulation.emitters->find(lastId);
			if (toUpdate)
				toUpdate->readFromPacket(packet->data + byteIterator);

			byteIterator += Emitter::packetBytes;
		}

		break;
	}
	case RopeTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;

		for (unsigned int a = 0; a < numObjects && simulation.ropes; a++)
		{
			lastId = simulation.ropes->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			if (byteIterator > packet->dataLength)
				break;

			unsigned int bytes = Rope::readStateBytes(packet->data + byteIterator, (unsigned int)packet->dataLength - byteIterator);
			if (bytes == 0)
				break;

			std::shared_ptr<Rope> toUpdate = simulation.ropes->find(lastId);
			if (toUpdate)
				toUpdate->readFromPacket(packet->data + byteIterator);

			byteIterator += bytes;
		}

		break;
	}
	case VehicleTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;

		for (unsigned int a = 0; a < numObjects; a++)
		{
			lastId = simulation.vehicles->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			//How many bytes an update takes depends on the vehicle's wheels, so one we don't know about leaves the rest unreadable
			std::shared_ptr<Vehicle> toUpdate = simulation.vehicles->find(lastId);
			if (!toUpdate || byteIterator + toUpdate->getUpdatePacketBytes() > packet->dataLength)
				break;

			toUpdate->readUpdate(packet->data + byteIterator, simulation.idealBufferSize);
			byteIterator += toUpdate->getUpdatePacketBytes();
		}

		break;
	}
	case DynamicTypeId:
	{
		unsigned int numObjects = packet->data[2];
		unsigned int byteIterator = 3;
		netIDType lastId = NO_ID;
		for (unsigned int a = 0; a < numObjects; a++)
		{
			//Ids are compressed by representing most of them using the difference between the last sent object in the packet
			lastId = simulation.dynamics->getIdFromDelta(packet->data + byteIterator, lastId, byteIterator);

			//0 or 255 will restart the 'clock' on this objects interpolation stack
			//Otherwise this is how many milliseconds it should take to interpolate from the last added snapshot to this one
			unsigned char msSinceLastSend = packet->data[byteIterator];
			byteIterator++;

			unsigned char flags = packet->data[byteIterator];
			byteIterator++;

			unsigned char extraFlags = packet->data[byteIterator];
			byteIterator++;

			bool needPosRot = flags & 1;
			bool needVel = flags & 2;
			bool needAngVel = flags & 4;

			//Gravity, restitution (bouncyness) and friction are only sent when a lua command changes those properties
			bool needGravity = flags & 8;
			bool needRestitution = flags & 16;
			bool needFriction = flags & 32;			

			bool playWalkAnimation = flags & 64;	//Is this a player that is currently walking (will probably be expanded soon)
			bool forcePlayerUpdate = flags & 128;	//Were the pos/rot/vel changes in this packet the result of an explicit lua command
			bool velocityOnly = extraFlags & DynamicExtra_VelocityOnly;	//Lua set only the velocity, so our own player keeps its position, see Dynamic::forcePlayerVelocity
		
			//Often only 1-3 of these will be sent, but the order if these if statements is still important
			glm::vec3 pos;
			glm::quat rot;
			glm::vec3 linVel;
			glm::vec3 angVel;
			glm::vec3 gravity;
			float restitution, friction;

			//A position sent as a delta off a keyframe we never got leaves us with no position at all, see Dynamic::readUpdatePosition
			bool havePos = false;

			//A position delta is measured off this object's own last keyframe, so it has to be found before one can be read
			std::shared_ptr<Dynamic> toUpdate = simulation.dynamics->find(lastId);

			if (needPosRot)
			{
				if (toUpdate)
					havePos = toUpdate->readUpdatePosition(packet->data + byteIterator, extraFlags, pos);
				byteIterator += updatePositionBytes(extraFlags);

				getQuaternion(packet->data + byteIterator, rot);
				byteIterator += QuaternionBytes;
			}

			if (needVel)
			{
				getVelocity(packet->data + byteIterator, linVel);
				byteIterator += VelocityBytes;
			}

			if (needAngVel)
			{
				getAngularVelocity(packet->data + byteIterator, angVel);
				byteIterator += AngularVelocityBytes;
			}

			if (needGravity)
			{
				memcpy(&gravity.x, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
				memcpy(&gravity.y, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
				memcpy(&gravity.z, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
			}

			if (needRestitution)
			{
				memcpy(&restitution, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
			}

			if (needFriction)
			{
				memcpy(&friction, packet->data + byteIterator, sizeof(float));
				byteIterator += sizeof(float);
			}

			//Where a player looks, which turns their head
			bool needLook = extraFlags & DynamicExtra_Look;
			glm::vec3 look;
			if (needLook)
			{
				getLookDirection(packet->data + byteIterator, look);
				byteIterator += LookDirectionBytes;
			}

			//An animation played once, like a grab, resent in a few updates with the same count
			bool needOneShot = extraFlags & DynamicExtra_OneShot;
			int oneShotAnimation = -1;
			unsigned char oneShotCount = 0;
			if (needOneShot)
			{
				oneShotAnimation = packet->data[byteIterator];
				oneShotCount = packet->data[byteIterator + 1];
				byteIterator += 2;
			}

			//Every animation the server has looping on it, resent in a few updates after a change, see Dynamic::startLoop
			bool needLoops = extraFlags & DynamicExtra_Loops;
			std::vector<unsigned char> loops;
			if (needLoops)
			{
				unsigned int numLoops = packet->data[byteIterator];
				byteIterator++;
				loops.assign(packet->data + byteIterator, packet->data + byteIterator + numLoops);
				byteIterator += numLoops;
			}

			//TODO: Friction, per-object gravity, and restitution are not actually sent on object creation yet so new joining players won't have the same values client-side
			if (toUpdate)
			{
				//Carried items are drawn in hand, not wherever the server last had them on the ground
				if (toUpdate->isInWorld() && (!toUpdate->clientControlled || forcePlayerUpdate))
				{
					//While we're locally predicting a collision response for this object (see LoopClient::predictLocalCollisions),
					//don't let a server update - which is still describing the pre-collision state it hasn't caught up to yet -
					//stomp our local physics body mid-prediction. Still feed the interpolator so it has accurate history ready
					//for when prediction ends and Dynamic::handOffFromPrediction hands control back to it
					bool predictingLocally = getTicksMS() < toUpdate->predictLocallyUntil;

					//A velocity Lua set on our own player: the position that came with it is where the server had us a round
					//trip ago, and taking it every tick of something like a rope swing drags us back that far each time
					bool keepOwnPosition = toUpdate->clientControlled && velocityOnly;

					if (needPosRot && havePos && !keepOwnPosition)
					{
						//msSinceLastSend already accounts for how often this client is sent one, see SimObject::scaleUpdateInterval
						toUpdate->interpolator.addSnapshot(pos, rot, simulation.idealBufferSize, msSinceLastSend);

						if (!predictingLocally)
						{
							btTransform t;
							t.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
							t.setOrigin(btVector3(pos.x, pos.y, pos.z));
							toUpdate->body->setWorldTransform(t);
						}
					}

					if (needVel && !predictingLocally)
						toUpdate->body->setLinearVelocity(btVector3(linVel.x, linVel.y, linVel.z));

					if (needAngVel && !predictingLocally)
						toUpdate->body->setAngularVelocity(btVector3(angVel.x, angVel.y, angVel.z));

					//Also tilts a swimming player, see Dynamic::updateSnapshot. Our own player decides its walk from its keys
					if (!keepOwnPosition)
					{
						toUpdate->playWalkingAnimation = playWalkAnimation;
						if (playWalkAnimation)
							toUpdate->play(0, true);
						else
							toUpdate->stop(0);
					}

					if (glm::length(linVel) > 0.1 || glm::length(angVel) > 0.1)
						toUpdate->body->activate();
				}

				//Our own player's head is already turned from its camera
				if (needLook && !toUpdate->clientControlled)
				{
					toUpdate->lookDirection = look;
					toUpdate->hasLook = true;
				}

				//Our own player's grab is already played on the click, see LoopClient, anything else played once on it comes from here like everyone else's
				if (needOneShot && oneShotCount != toUpdate->oneShotCount)
				{
					toUpdate->oneShotCount = oneShotCount;
					if (!toUpdate->clientControlled || oneShotAnimation != toUpdate->getType()->getModel()->getAnimationID("grab"))
						toUpdate->playOneShot(oneShotAnimation);
				}

				//Looping animations aren't predicted for anyone, our own player included
				if (needLoops)
					toUpdate->syncLoops(loops);

				if (needGravity)
					toUpdate->body->setGravity(g2b3(gravity));

				if (needRestitution)
					toUpdate->body->setRestitution(restitution);

				if (needFriction)
					toUpdate->body->setFriction(friction);
			}

			if (byteIterator >= packet->dataLength)
				break;
		}
		break;
	}
	}

	return true;
}

UpdateSimObjectsPacket::UpdateSimObjectsPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

UpdateSimObjectsPacket::~UpdateSimObjectsPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
