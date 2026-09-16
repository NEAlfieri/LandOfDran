#pragma once

#include "../LandOfDran.h"

#include "ShaderSpecification.h"
#include "../Interface/InputMap.h"
#include "../SimObjects/Dynamic.h"

class Camera
{
	glm::mat4 viewMatrix = glm::mat4(1.0);

	//Like the view matrix, but only rotation, not translation, good for rendering skyboxes and stuff
	glm::mat4 angleMatrix = glm::mat4(1.0);

	glm::mat4 projectionMatrix = glm::mat4(1.0);

	//In world space
	glm::vec3 position = glm::vec3(0, 0, 0);

	//What direction the camera is pointing
	glm::vec3 direction = glm::vec3(0, 0, 1);

	//Should almost always remain 0,1,0 (up) but we could implement camera shake, rag doll camera, etc.
	glm::vec3 nominalUp = glm::vec3(0, 1, 0);

	float fieldOfVision = 90.0;
	//Field of view while zooming, like the old game, and what the projection uses right now as it eases between the two
	float zoomedFieldOfVision = 15.0;
	float currentFieldOfVision = 90.0;
	float nearPlane = 0.5;
	float farPlane = 1000.0;
	float aspectRatio = 1.0;

	float mouseSensitivity = 1.0;
	bool invertMouse = false;

	bool firstPerson = true;

	float thirdPersonDistance = 30.0;

	//Rebuilds projectionMatrix from currentFieldOfVision and aspectRatio
	void updateProjection();

	public:

	//Set each frame while the zoom key is held, render() narrows the view toward zoomedFieldOfVision
	bool zooming = false;

	bool getFirstPerson() const { return firstPerson; }

	//Takes OpenGL normalized device coordinates and returns a position in world space
	glm::vec3 mouseCoordsToWorldSpace(glm::vec2 mouseCoords) const;

	//TODO: Move this to environment class
	//Three shadow cascades covering the view out to shadowDistance, nearest first, for a mapResolution square shadow map
	//radii, if given, comes back with how wide each cascade is in world units, for deciding when a cached one has gone stale
	void calculateLightSpaceMatricies(glm::vec3 lightDirection, float shadowDistance, int mapResolution, glm::mat4 *result, float* radii = nullptr);

	float maxThirdPersonDistance = 30.0;

	//A body the third person camera sees through besides its target's, like the vehicle the target is driving, nullptr for none
	const btRigidBody* alsoIgnore = nullptr;

	void setFirstPerson(bool _firstPerson);
	void swapPerson();

	//See CameraSettingsPacket
	std::weak_ptr<Dynamic> target;
	bool freePosition = true;	//Only used if target is nullptr
	bool freeDirection = true; //same
	bool freeUpVector = false;  //Upvector can be explicitly set if unlocked and there's no target object

	void setPosition(const glm::vec3& pos);

	void setDirection(const glm::vec3& dir);

	void setUp(const glm::vec3& up);

	void control(float deltaT,std::shared_ptr<InputMap> input);

	void updateSettings(std::shared_ptr<SettingManager> settings);

	//Positive amount forward, negative backward, only use for no-clip camera
	void flyStraight(float amount);

	//Positive amount right, negative left, only use for no-clip camera
	void flySideways(float amount);

	//Look around with the mouse
	void turn(float relMouseX, float relMouseY);

	//Call once per frame
	void render(std::shared_ptr<ShaderManager> graphics, float deltaT, const std::shared_ptr<PhysicsWorld>  world);

	//Pushes the matrices from the last render() call to the camera UBO without moving the camera
	void uploadUniforms(std::shared_ptr<ShaderManager> graphics) const;

	//Like uploadUniforms, but mirrored below a horizontal plane at waterLevel, for rendering water reflections
	//The image comes out upside down compared to a true reflection, water.frag flips it back
	void uploadReflectionUniforms(std::shared_ptr<ShaderManager> graphics, float waterLevel) const;

	void setFOV(float fov);

	//Call when screen size changed
	void setAspectRatio(float ratio);

	glm::vec3 getPosition();

	glm::vec3 getDirection();

	Camera(float _aspectRatio = 1.0, float _fieldOfVision = 90.0, float _nearPlane = 0.5, float _farPlane = 1000.0);
};