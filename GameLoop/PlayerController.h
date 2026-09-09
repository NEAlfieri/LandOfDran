#pragma once

#include "../SimObjects/Dynamic.h"
#include "../Graphics/PlayerCamera.h"
#include "../Networking/ClientPacketCreators.h"

/*
	Bind to a dynamic, pass InputMap to it
	Server settings parameters of how this works through MovementSettingsPacket
*/
struct PlayerController
{
	//Some parameters here:

	//The thing this is controlling
	std::weak_ptr<Dynamic> target;

	btQuaternion playerYaw = btQuaternion(0,0,0,1.0);

	bool serverSide = false;

	//Client uses these to send last inputs to server
	//Server cachces these and applies them each frame until a new packet comes in
	bool lastJump, lastForward, lastBackward, lastLeft, lastRight;
	glm::vec3 lastCameraDirection = glm::vec3(0.01,1.0,0.01);

	//Client only, last time we sent a packet to the server
	unsigned int lastSentControls = 0;

	//Client only, send last inputs to server for caching and reflection
	//Can return nullptr if object was deleted or packet was recently sent
	ENetPacket* makeMovementInputsPacket();

	//Server only wrapper
	bool controlWithLastInput(std::shared_ptr<PhysicsWorld> world, float deltaT);

	//Server and client side, called per frame, server caches last inputs from clients
	bool control(std::shared_ptr<PhysicsWorld> world, float deltaT, glm::vec3 cameraDirection, bool jump, bool forward, bool backward, bool left, bool right);

	/*
	    Client side wrapper
		Call for each controller each frame, returns true if weak_ptr lock expired
	*/
	bool control(const std::shared_ptr<InputMap> input, const std::shared_ptr<Camera> camera, float deltaT, std::shared_ptr<PhysicsWorld> world);
};
