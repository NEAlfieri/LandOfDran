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

			bool needPosRot = flags & 1;
			bool needVel = flags & 2;
			bool needAngVel = flags & 4;

			//Gravity, restitution (bouncyness) and friction are only sent when a lua command changes those properties
			bool needGravity = flags & 8;
			bool needRestitution = flags & 16;
			bool needFriction = flags & 32;			

			bool playWalkAnimation = flags & 64;	//Is this a player that is currently walking (will probably be expanded soon)
			bool forcePlayerUpdate = flags & 128;	//Were the pos/rot/vel changes in this packet the result of an explicit lua command
		
			//Often only 1-3 of these will be sent, but the order if these if statements is still important
			glm::vec3 pos;
			glm::quat rot;
			glm::vec3 linVel;
			glm::vec3 angVel;
			glm::vec3 gravity;
			float restitution, friction;
			if (needPosRot)
			{
				getPosition(packet->data + byteIterator, pos);
				byteIterator += PositionBytes;

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

			//TODO: Friction, per-object gravity, and restitution are not actually sent on object creation yet so new joining players won't have the same values client-side
			std::shared_ptr<Dynamic> toUpdate = simulation.dynamics->find(lastId);
			if (toUpdate)
			{
				if (!toUpdate->clientControlled || forcePlayerUpdate)
				{
					//While we're locally predicting a collision response for this object (see LoopClient::predictLocalCollisions),
					//don't let a server update - which is still describing the pre-collision state it hasn't caught up to yet -
					//stomp our local physics body mid-prediction. Still feed the interpolator so it has accurate history ready
					//for when prediction ends and Dynamic::handOffFromPrediction hands control back to it
					bool predictingLocally = getTicksMS() < toUpdate->predictLocallyUntil;

					if (needPosRot)
					{
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

					if (playWalkAnimation)
						toUpdate->play(0, true);
					else
						toUpdate->stop(0);

					if (glm::length(linVel) > 0.1 || glm::length(angVel) > 0.1)
						toUpdate->body->activate();
				}

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
