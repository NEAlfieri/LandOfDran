#pragma once

#include "../LandOfDran.h"

//Which parts a BrickAttachments has, the same bits as a brick record's flags byte in Land of Dran saves, where 1 is collision
#define BrickAttachment_Music 2
#define BrickAttachment_Light 4
#define BrickAttachment_Emitter 16
#define BrickAttachment_Wheel 32
#define BrickAttachment_Steering 64
//8 is a print in saves, see Bricks/BrickSaves.cpp, so the vehicle horn and headlight take the last bit
#define BrickAttachment_Horn 128

//How a wheel brick's wheel drives once its bricks are sliced into a vehicle, from the old game's wheel wrench dialog
struct WheelSettings
{
	//Forward push while driving, negative runs it backwards
	float engineForce = 200.0f;
	float brakeForce = 400.0f;
	//Radians the wheel turns while steering, 0 for a wheel that doesn't steer
	float steerAngle = 0.5f;
	//World units from where the wheel hangs to its middle when resting
	float suspensionLength = 0.7f;
	float suspensionStiffness = 100.0f;
	float dampingCompression = 6.0f;
	float dampingRelaxation = 10.0f;
	//Grip, higher slides less
	float frictionSlip = 1.2f;
	//How much the wheels' grip tips the vehicle over in turns, lower is steadier
	float rollInfluence = 0.6f;

	static constexpr size_t floatCount = 9;

	//Same ranges as the old game's wrench dialog
	void clampValues();
};

//How a whole vehicle handles, set on its steering wheel brick
struct SteeringSettings
{
	//Weight each brick adds to how hard the vehicle is to turn over
	float mass = 1.5f;
	//How quickly spinning slows down
	float angularDamping = 0.03f;
	//Off, the vehicle turns around a point near its wheels so it's hard to flip, on, around the middle of its bricks
	bool realisticCenterOfMass = false;

	void clampValues();
};

/*
	What the wrench dialog puts on a brick besides its collision and name: a music loop, a light, and an emitter
	Only the server keeps these, clients just see the sound loop, Light, and Emitter made from them, see LuaFunctions/BrickLua.h
*/
struct BrickAttachments
{
	//Longest music or emitter type name
	static constexpr size_t maxNameLength = 255;
	//Floats a light part has, saves from before blinking have one less
	static constexpr size_t lightFloatCount = 16;
	//Furthest a light can be from the middle of its brick along each axis, in world units
	static constexpr float maxLightOffset = 32.0f;

	//A sound type's name, "" for no music
	std::string musicName = "";
	float musicVolume = 1.0f;
	float musicPitch = 1.0f;

	bool hasLight = false;
	glm::vec3 lightColor = glm::vec3(1);
	float lightBrightness = 50.0f;
	float lightFlicker = 0.0f;
	//Seconds per blink cycle, 0 for none, and how much it dims at the low point, see Light
	float lightBlinkSpeed = 0.0f;
	float lightBlinkStrength = 1.0f;
	float lightCoronaWidth = 0.0f;
	//0 shines every way, otherwise a spotlight's full width in degrees
	float lightConeAngle = 0.0f;
	glm::vec3 lightDirection = glm::vec3(0, -1, 0);
	float lightSpin = 0.0f;
	//From the middle of the brick, in world units. Point light shadows leave out bricks the light is inside, so it isn't buried by its own brick
	glm::vec3 lightOffset = glm::vec3(0);

	//An emitter type's name, "" for no emitter
	std::string emitterName = "";

	//A wheel brick's settings from its wrench dialog, the defaults until it's wrenched, see WheelSettings
	bool hasWheel = false;
	WheelSettings wheel;

	//A steering wheel brick's settings from its wrench dialog, see SteeringSettings
	bool hasSteering = false;
	SteeringSettings steering;

	/*
		The horn a vehicle made of these bricks honks, on its steering wheel brick like its music, see Vehicle::hornName
		hasHorn with an empty hornName is a vehicle wrenched to have no horn at all, without it a new vehicle gets the Honk sound
		lightIsHeadlight makes the light above the vehicle's headlight once the bricks are sliced, which its driver switches
		on and off, rather than a light that's always on. Both ride in the BrickAttachment_Horn part
	*/
	bool hasHorn = false;
	std::string hornName = "";
	bool lightIsHeadlight = false;

	//Server only: the loop, light, and emitter made from the settings above, never saved or sent
	unsigned int musicLoopID = NO_ID;
	netIDType lightID = NO_ID;
	netIDType emitterID = NO_ID;

	//BrickAttachment bits for the parts it has
	unsigned char getFlags() const;
	bool isEmpty() const { return getFlags() == 0; }

	//Keeps every value in the range the loop, light, and emitter take them in, and names to maxNameLength
	void clampValues();

	//Every light setting back to what a newly added light starts with, in the middle of its brick, leaving hasLight alone
	void resetLight();

	//What a vehicle's headlight starts as: a white spotlight a little narrower than a player's flashlight, shining the way the vehicle drives (forward, in its body's space)
	void resetHeadlight(const glm::vec3& forward);

	/*
		Only the parts in getFlags, passed to writeBytes as they go
		Music: name length byte, name, volume and pitch floats
		Light: lightFloatCount floats, color, brightness, flicker, corona width, cone angle, direction, spin, offset, blink speed, blink strength
		Emitter: name length byte, name
		Horn: name length byte, name, then a byte with 1 for a headlight and 2 for a horn that was set (see hasHorn)
	*/
	void writeParts(const std::function<void(const void*, size_t)>& writeBytes) const;

	//Reads the parts flags says follow, false if readBytes runs out. lightFloats is how many floats the light part has, older saves have fewer
	bool readParts(unsigned char flags, const std::function<bool(void*, size_t)>& readBytes, size_t lightFloats = lightFloatCount);

	//For packets: the flags byte then writeParts
	void write(std::vector<unsigned char>& bytes) const;
	bool read(const unsigned char* data, size_t length, size_t& at);
};
