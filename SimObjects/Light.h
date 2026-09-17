#pragma once

#include "SimObject.h"

class Dynamic;
class Vehicle;

/*
	A point light or spotlight with no model or physics body, placed by server Lua with createLight
	Clients light the world with it, give the ones nearest the camera shadows (graphics/pointshadows), and draw its corona, see Graphics/PointLights.h
*/
class Light : public SimObject
{
	friend ObjHolder<Light>;

	//Update packets go out unreliably, so a change is sent this many times to make sure it arrives
	static constexpr int resendCount = 4;
	int updatesLeft = 0;

	//Where it is, or where it is in the body's space of the vehicle it's on
	glm::vec3 position = glm::vec3(0);
	//0 to 1 per channel
	glm::vec3 color = glm::vec3(1);
	//Multiplies color, how far the light reaches follows from it, see getRange
	float brightness = 0;
	//How far, in world units, the light wanders around its position
	float flicker = 0;
	//Seconds for one full cycle of dimming and brightening again, 0 for no blinking
	float blinkSpeed = 0;
	//0 to 1, how much of its brightness the light loses at the dimmest point of a blink
	float blinkStrength = 1;
	//Width in world units of the glow drawn around the light, 0 for none
	float coronaWidth = 0;
	//Which way a spotlight points, always normalized, in the vehicle body's space for a light on a vehicle
	glm::vec3 direction = glm::vec3(0, -1, 0);
	//Full width of a spotlight's beam in degrees, 0 for a light that shines every way
	float coneAngle = 0;
	//Degrees per second the direction turns around the vertical axis
	float spin = 0;
	//Net ID of the dynamic holding it like a flashlight, NO_ID for none, see setHolder
	netIDType holderID = NO_ID;
	//Net ID of the vehicle it's on, NO_ID for none, see attachToVehicle
	netIDType vehicleID = NO_ID;

	//Client only, see getRenderedPosition and getRenderedDirection
	mutable glm::vec3 flickerFrom = glm::vec3(0);
	mutable glm::vec3 flickerTo = glm::vec3(0);
	mutable uint32_t flickerStartMS = 0;
	mutable uint32_t nextFlickerMS = 0;
	mutable float spinAngle = 0;
	mutable uint32_t lastSpinMS = 0;
	mutable glm::vec3 heldDirection = glm::vec3(0, -1, 0);
	mutable bool heldDirectionSet = false;

	void writeState(enet_uint8* dest) const;

	protected:

	explicit Light(const glm::vec3& _position, const glm::vec3& _color, float _brightness, float _flicker, float _coronaWidth);

	virtual void onCreation() override {}

	virtual void requestDestruction() override {}

	public:

	//Largest values Lua can set
	static constexpr float maxBrightness = 100000.0f;
	static constexpr float maxFlicker = 16.0f;
	static constexpr float maxCoronaWidth = 256.0f;
	static constexpr float maxConeAngle = 179.0f;
	static constexpr float maxSpin = 3600.0f;
	static constexpr float maxBlinkSpeed = 60.0f;

	//State written by both creation and update packets, after the net ID
	static constexpr unsigned int packetBytes = sizeof(float) * 16 + sizeof(netIDType) * 2;

	//Client only: a light no server object backs, which no ObjHolder owns and nothing is ever sent about
	//Its settings come straight from the setters below, see LoopClient::updateLightPreview
	static std::shared_ptr<Light> makeLocal(const glm::vec3& position);

	//Server: the dynamic holding it. Client: the dynamic with getHolderID, found again whenever that isn't it
	std::weak_ptr<Dynamic> holder;

	//Same for the vehicle it's on
	std::weak_ptr<Vehicle> vehicle;

	//Server: where its holder or vehicle is if it has one
	glm::vec3 getPosition() const;
	netIDType getHolderID() const { return holderID; }
	netIDType getVehicleID() const { return vehicleID; }
	const glm::vec3& getColor() const { return color; }
	float getBrightness() const { return brightness; }
	float getFlicker() const { return flicker; }
	float getBlinkSpeed() const { return blinkSpeed; }
	float getBlinkStrength() const { return blinkStrength; }
	float getCoronaWidth() const { return coronaWidth; }
	const glm::vec3& getDirection() const { return direction; }
	float getConeAngle() const { return coneAngle; }
	float getSpin() const { return spin; }

	//Each of these clamps to a valid range and has clients sent the change
	//setPosition also takes it away from its holder or vehicle
	void setPosition(const glm::vec3& _position);
	void setColor(const glm::vec3& _color);
	void setBrightness(float _brightness);
	void setFlicker(float _flicker);
	//Speed is seconds per cycle from 0 to maxBlinkSpeed, strength 0 to 1
	void setBlink(float speed, float strength);
	void setCoronaWidth(float _coronaWidth);
	//0 makes it shine every way, anything else is clamped to 1 to maxConeAngle degrees
	void setConeAngle(float degrees);
	void setSpin(float degreesPerSecond);

	//Returns false and changes nothing for a zero length direction
	bool setDirection(const glm::vec3& _direction);

	//Has a dynamic hold it like a flashlight: clients shine it from just past that dynamic's right hand toward its direction, see LoopClient::placeHeldLight
	void setHolder(const std::shared_ptr<Dynamic>& dynamic);

	//Puts it on a vehicle at offset in the vehicle body's space, its direction turning with the vehicle too, see Vehicle
	void attachToVehicle(const std::shared_ptr<Vehicle>& target, const glm::vec3& offset);

	//Distance past which the light adds too little to see, model.frag fades it to exactly nothing there
	float getRange() const;

	//Cosine of half the cone angle, or -2 for a light that shines every way
	float getConeCosine() const;

	//Client: where the light is drawn, gliding toward a new random spot within flicker of its position every so often
	//For a light on a vehicle, that's in the vehicle's space
	glm::vec3 getRenderedPosition(uint32_t nowMS) const;

	//Client: brightness dimmed by however far into its blink cycle it is
	float getRenderedBrightness(uint32_t nowMS) const;

	//Client: direction turned by however far it has spun, call once a frame. A held light glides toward a new direction instead of jumping
	glm::vec3 getRenderedDirection(uint32_t nowMS) const;

	//Client: applies packetBytes of state written by the server
	void readFromPacket(const enet_uint8* src);

	virtual bool requiresNetUpdate() override;

	virtual unsigned int getCreationPacketBytes() const override;

	virtual unsigned int getUpdatePacketBytes() const override;

	virtual void addToCreationPacket(enet_uint8* dest) const override;

	virtual void addToUpdatePacket(enet_uint8* dest) override;
};
