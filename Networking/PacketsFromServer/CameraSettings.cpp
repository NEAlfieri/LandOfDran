#include "CameraSettings.h"

bool CameraSettingsPacket::applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs)
{
	if (cmdArgs.gameState != InGame)
		return false;

	if (packet->dataLength < 2)
		return true;

	int packetIter = 1;
	unsigned char flags =   packet->data[packetIter++];
	bool boundToObject =	flags & CameraFlag_BoundToObject;
	bool lockCamDir =		flags & CameraFlag_LockDirection;
	bool lockCamPos =		flags & CameraFlag_LockPosition;
	bool lockUpVec =		flags & CameraFlag_LockUpVector;

	netIDType objectId;
	std::shared_ptr<Dynamic> target;
	float maxFollowDistance;
	if (boundToObject)
	{
		if(packet->dataLength < (packetIter + sizeof(netIDType) + sizeof(float)))
			return false;
		 
		memcpy(&objectId, packet->data + packetIter, sizeof(netIDType));
		packetIter += sizeof(netIDType);

		target = simulation.dynamics->find(objectId);
		if (!target)
			return false;

		memcpy(&maxFollowDistance, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);

		simulation.camera->maxThirdPersonDistance = maxFollowDistance;
	}

	glm::vec3 camPos;
	if (lockCamPos)
	{
		if (packet->dataLength < (packetIter + sizeof(float) * 3))
			return false;

		memcpy(&camPos.x, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&camPos.y, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&camPos.z, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
	}

	glm::vec3 camDir;
	if (lockCamDir)
	{
		if (packet->dataLength < (packetIter + sizeof(float) * 3))
			return false;

		memcpy(&camDir.x, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&camDir.y, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&camDir.z, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
	}

	glm::vec3 upVec;
	if (lockUpVec && !boundToObject)
	{
		if (packet->dataLength < (packetIter + sizeof(float) * 3))
			return false;

		memcpy(&upVec.x, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&upVec.y, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
		memcpy(&upVec.z, packet->data + packetIter, sizeof(float));
		packetIter += sizeof(float);
	}

	std::shared_ptr<Dynamic> prevTarget = simulation.camera->target.lock();
	if (prevTarget && prevTarget != target)
		prevTarget->setHidden(false);

	if (!boundToObject)
	{
		simulation.camera->target.reset();
		simulation.camera->freeDirection = !lockCamDir;
		if (lockCamDir)
			simulation.camera->setDirection(camDir);
	}
	else
	{
		simulation.camera->target = target;
		simulation.camera->freeDirection = true;
	}

	simulation.camera->freePosition = !lockCamPos;
	if(lockCamPos)
		simulation.camera->setPosition(camPos);

	simulation.camera->freeUpVector = !lockUpVec;
	if (lockUpVec && !boundToObject)
		simulation.camera->setUp(upVec);

	return true;
}

CameraSettingsPacket::CameraSettingsPacket(unsigned int holdTime, ENetPacket* _packet)
{
	packet = _packet;
	deletionTime = SDL_GetTicks() + holdTime;
}

CameraSettingsPacket::~CameraSettingsPacket()
{
	if (packet)
		enet_packet_destroy(packet);
}
