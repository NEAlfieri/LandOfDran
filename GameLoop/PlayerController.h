#pragma once

#include <limits>

#include "../SimObjects/Dynamic.h"
#include "../SimObjects/Emitter.h"
#include "../Graphics/PlayerCamera.h"
#include "../Networking/ClientPacketCreators.h"

/*
	Bind to a dynamic, pass InputMap to it
	Server settings parameters of how this works through MovementSettingsPacket
*/
struct PlayerController
{
	//Water level to pass to control when the world has no water
	static constexpr float noWater = -std::numeric_limits<float>::infinity();

	//Some parameters here:

	//The thing this is controlling
	std::weak_ptr<Dynamic> target;

	btQuaternion playerYaw = btQuaternion(0,0,0,1.0);

	bool serverSide = false;

	/*
		Nobody is playing this one: Lua walks it with dynamic:setBotInput and the server simulates it like any
		other object it owns. A player's own game turns their body and sends the rotation back, so the server
		never sets it; a bot has no game to do that, so control turns it toward where it walks itself
	*/
	bool bot = false;

	//Client only, set each frame by the client wrapper: our camera is in first person on the target, so its hidden body turns to face where we look
	//instead of where we walk, keeping what it holds (the flashlight) in front of it
	bool faceCamera = false;

	//Client uses these to send last inputs to server
	//Server cachces these and applies them each frame until a new packet comes in
	bool lastJump, lastJumpHeld, lastForward, lastBackward, lastLeft, lastRight;
	//Right mouse held, jetting if jetsAllowed
	bool lastJet = false;
	//The crawl key held, which lies the player down, see crawlProgress
	bool lastCrawl = false;

	/*
		0 standing, 1 lying flat on the ground, moved along by control while the crawl key is held or let go
		The body is tipped this far forward, collision box and all, so a crawling player fits where a standing one doesn't
	*/
	float crawlProgress = 0;

	//Lua's client:setJetsEnabled on the server, the PlayerAbilities packet on the client
	bool jetsAllowed = true;

	//Server only: the jet flames under each foot while jetting, see LoopServer::updatePlayerAbilities
	std::weak_ptr<Emitter> jetEmitters[2];
	glm::vec3 lastCameraDirection = glm::vec3(0.01,1.0,0.01);
	//Server: taken from the client's periodic movement input packets. Client: taken from the local Camera each frame.
	//Used for cursor-based features (dynamic:snapToCursor, client:getCursorItem) - only meaningful once at least one
	//movement input packet has arrived, see the comment on the default value above
	glm::vec3 lastCameraPosition = glm::vec3(0,0,0);

	//Client only, last time we sent a packet to the server
	unsigned int lastSentControls = 0;
	//Client only, the jet state in the last packet we sent
	bool lastSentJet = false;

	//Client only, the MovementFlag bits in the last packet we sent, a change goes out right away so driving a vehicle responds quickly
	unsigned char lastSentFlags = 0;

	//Whether the last control call made the target jump
	bool jumped = false;

	//getTicksMS of the last jump out of the water, swimming waits a moment after it so it doesn't slow the jump down
	unsigned int lastWaterJump = 0;

	//Client only, set each frame while left mouse is held, so the camera goes to the server more often for scripts following the crosshair
	bool sendQuickly = false;

	//Client only, send last inputs to server for caching and reflection
	//Can return nullptr if object was deleted or packet was recently sent
	ENetPacket* makeMovementInputsPacket();

	//Server only wrapper
	bool controlWithLastInput(std::shared_ptr<PhysicsWorld> world, float deltaT, float waterLevel);

	/*
		Server and client side, called per frame, server caches last inputs from clients
		jump is a new press of the jump key, jumpHeld is whether it's down at all (it swims up)
		jet is whether right mouse is held, which lifts the player and speeds them along while jetsAllowed
		crawl is whether the crawl key is held, which lies them down on land, slows them, and aims their jets forward
		waterLevel is noWater if there's no water
	*/
	bool control(std::shared_ptr<PhysicsWorld> world, float deltaT, glm::vec3 cameraDirection, glm::vec3 cameraPosition, bool jump, bool jumpHeld, bool forward, bool backward, bool left, bool right, bool jet, bool crawl, float waterLevel);

	/*
	    Client side wrapper
		Call for each controller each frame, returns true if weak_ptr lock expired
	*/
	bool control(const std::shared_ptr<InputMap> input, const std::shared_ptr<Camera> camera, float deltaT, std::shared_ptr<PhysicsWorld> world, bool jet, bool crawl, float waterLevel);
};
