#include "Light.h"
#include "Dynamic.h"
#include "Vehicle.h"

#include <random>
#include <glm/gtc/constants.hpp>

//Light reaching a surface dimmer than this isn't drawn, a bit under what's visible after tone mapping, see model.frag's pointLighting
static constexpr float rangeCutoff = 0.02f;

//Also the far plane of the light's shadow cube, so it can't be too far
static constexpr float maxRange = 500.0f;

//How long a flickering light takes to glide from one spot to the next
static constexpr uint32_t minFlickerHoldMS = 40;
static constexpr uint32_t maxFlickerHoldMS = 160;

//A held light's direction only arrives about ten times a second (the holder's movement inputs), so it glides over roughly this long
static constexpr float heldTurnSmoothingMS = 60.0f;

Light::Light(const glm::vec3& _position, const glm::vec3& _color, float _brightness, float _flicker, float _coronaWidth)
{
	setPosition(_position);
	setColor(_color);
	setBrightness(_brightness);
	setFlicker(_flicker);
	setCoronaWidth(_coronaWidth);

	//The creation packet already carries all of it
	updatesLeft = 0;
}

std::shared_ptr<Light> Light::makeLocal(const glm::vec3& position)
{
	return std::shared_ptr<Light>(new Light(position, glm::vec3(1), 0, 0, 0));
}

void Light::setPosition(const glm::vec3& _position)
{
	position = _position;
	holderID = NO_ID;
	holder.reset();
	vehicleID = NO_ID;
	vehicle.reset();
	updatesLeft = resendCount;
}

void Light::setHolder(const std::shared_ptr<Dynamic>& dynamic)
{
	holder = dynamic;
	holderID = dynamic ? dynamic->getID() : NO_ID;
	vehicleID = NO_ID;
	vehicle.reset();
	updatesLeft = resendCount;
}

void Light::attachToVehicle(const std::shared_ptr<Vehicle>& target, const glm::vec3& offset)
{
	setPosition(offset);
	vehicle = target;
	vehicleID = target ? target->getID() : NO_ID;
}

glm::vec3 Light::getPosition() const
{
	if (holderID != NO_ID)
	{
		if (std::shared_ptr<Dynamic> held = holder.lock())
			return b2g3(held->getPosition());
	}

	if (vehicleID != NO_ID)
	{
		std::shared_ptr<Vehicle> on = vehicle.lock();
		if (on && on->body)
			return b2g3(on->body->getWorldTransform() * g2b3(position));
	}

	return position;
}

void Light::setColor(const glm::vec3& _color)
{
	color = glm::clamp(_color, glm::vec3(0.0f), glm::vec3(1.0f));
	updatesLeft = resendCount;
}

void Light::setBrightness(float _brightness)
{
	brightness = std::clamp(_brightness, 0.0f, maxBrightness);
	updatesLeft = resendCount;
}

void Light::setFlicker(float _flicker)
{
	flicker = std::clamp(_flicker, 0.0f, maxFlicker);
	updatesLeft = resendCount;
}

void Light::setBlink(float speed, float strength)
{
	blinkSpeed = std::clamp(speed, 0.0f, maxBlinkSpeed);
	blinkStrength = std::clamp(strength, 0.0f, 1.0f);
	updatesLeft = resendCount;
}

void Light::setCoronaWidth(float _coronaWidth)
{
	coronaWidth = std::clamp(_coronaWidth, 0.0f, maxCoronaWidth);
	updatesLeft = resendCount;
}

bool Light::setDirection(const glm::vec3& _direction)
{
	float length = glm::length(_direction);
	if (!(length > 0.0001f))
		return false;

	direction = _direction / length;
	updatesLeft = resendCount;
	return true;
}

void Light::setConeAngle(float degrees)
{
	coneAngle = degrees <= 0.0f ? 0.0f : std::clamp(degrees, 1.0f, maxConeAngle);
	updatesLeft = resendCount;
}

void Light::setSpin(float degreesPerSecond)
{
	spin = std::clamp(degreesPerSecond, -maxSpin, maxSpin);
	updatesLeft = resendCount;
}

float Light::getRange() const
{
	//Falloff is brightness / (distance squared + 1), so solve for where that reaches the cutoff
	float strongest = brightness * std::max(color.r, std::max(color.g, color.b));
	if (strongest <= rangeCutoff)
		return 0.0f;

	return std::min(std::sqrt(strongest / rangeCutoff - 1.0f), maxRange);
}

float Light::getConeCosine() const
{
	return coneAngle > 0.0f ? std::cos(glm::radians(coneAngle * 0.5f)) : -2.0f;
}

glm::vec3 Light::getRenderedPosition(uint32_t nowMS) const
{
	if (flicker <= 0.0f)
		return position;

	if (nowMS >= nextFlickerMS)
	{
		static std::mt19937 random(std::random_device{}());
		std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
		std::uniform_int_distribution<uint32_t> hold(minFlickerHoldMS, maxFlickerHoldMS);

		//Starts from the spot it just reached, even if frames were missed getting there
		flickerFrom = flickerTo;

		//Anywhere in a ball rather than a cube, so it never reaches further along a diagonal
		do
			flickerTo = glm::vec3(unit(random), unit(random), unit(random));
		while (glm::dot(flickerTo, flickerTo) > 1.0f);

		flickerStartMS = nowMS;
		nextFlickerMS = nowMS + hold(random);
	}

	//Eases in and out of each spot rather than turning sharply at it
	float progress = std::clamp((float)(nowMS - flickerStartMS) / (float)(nextFlickerMS - flickerStartMS), 0.0f, 1.0f);
	progress = progress * progress * (3.0f - 2.0f * progress);

	return position + glm::mix(flickerFrom, flickerTo, progress) * flicker;
}

float Light::getRenderedBrightness(uint32_t nowMS) const
{
	if (blinkSpeed <= 0.0f || blinkStrength <= 0.0f)
		return brightness;

	//Doubles so hours of ticks don't round the cycle off
	double cycle = std::fmod(nowMS / 1000.0, (double)blinkSpeed) / blinkSpeed;

	//Full brightness at the start of each cycle, dimmest halfway through
	float dim = (1.0f - (float)std::cos(cycle * glm::two_pi<double>())) * 0.5f;
	return brightness * (1.0f - blinkStrength * dim);
}

glm::vec3 Light::getRenderedDirection(uint32_t nowMS) const
{
	float elapsedMS = lastSpinMS != 0 ? (float)(nowMS - lastSpinMS) : 0.0f;
	if (spin != 0.0f)
		spinAngle = std::fmod(spinAngle + glm::radians(spin) * elapsedMS / 1000.0f, glm::two_pi<float>());
	lastSpinMS = nowMS;

	glm::vec3 pointing = direction;
	if (holderID != NO_ID)
	{
		if (!heldDirectionSet)
			heldDirection = direction;
		else
		{
			glm::vec3 glided = glm::mix(heldDirection, direction, 1.0f - std::exp(-elapsedMS / heldTurnSmoothingMS));
			heldDirection = glm::length(glided) > 0.0001f ? glm::normalize(glided) : direction;
		}
		heldDirectionSet = true;
		pointing = heldDirection;
	}

	if (spinAngle == 0.0f)
		return pointing;

	//Around the vertical axis, like the old game's yaw velocity
	float c = std::cos(spinAngle);
	float s = std::sin(spinAngle);
	return glm::vec3(pointing.x * c + pointing.z * s, pointing.y, pointing.z * c - pointing.x * s);
}

void Light::writeState(enet_uint8* dest) const
{
	const float state[16] = { position.x, position.y, position.z, color.r, color.g, color.b, brightness, flicker, coronaWidth,
		direction.x, direction.y, direction.z, coneAngle, spin, blinkSpeed, blinkStrength };
	memcpy(dest, state, sizeof(state));
	memcpy(dest + sizeof(state), &holderID, sizeof(netIDType));
	memcpy(dest + sizeof(state) + sizeof(netIDType), &vehicleID, sizeof(netIDType));
}

void Light::readFromPacket(const enet_uint8* src)
{
	float state[16];
	memcpy(state, src, sizeof(state));

	netIDType newHolderID;
	memcpy(&newHolderID, src + sizeof(state), sizeof(netIDType));
	if (newHolderID != holderID)
	{
		holder.reset();
		heldDirectionSet = false;
	}
	holderID = newHolderID;

	netIDType newVehicleID;
	memcpy(&newVehicleID, src + sizeof(state) + sizeof(netIDType), sizeof(netIDType));
	if (newVehicleID != vehicleID)
		vehicle.reset();
	vehicleID = newVehicleID;

	position = glm::vec3(state[0], state[1], state[2]);
	color = glm::vec3(state[3], state[4], state[5]);
	brightness = state[6];
	flicker = state[7];
	coronaWidth = state[8];

	//A newly set direction starts over from where Lua pointed it, other updates leave the spin where it is
	glm::vec3 newDirection(state[9], state[10], state[11]);
	if (newDirection != direction)
		spinAngle = 0;
	direction = newDirection;

	coneAngle = state[12];
	spin = state[13];
	blinkSpeed = state[14];
	blinkStrength = state[15];
}

bool Light::requiresNetUpdate()
{
	flaggedForUpdate = updatesLeft > 0;
	return flaggedForUpdate;
}

unsigned int Light::getCreationPacketBytes() const
{
	return sizeof(netIDType) + packetBytes;
}

unsigned int Light::getUpdatePacketBytes() const
{
	return packetBytes;
}

void Light::addToCreationPacket(enet_uint8* dest) const
{
	netIDType id = getID();
	memcpy(dest, &id, sizeof(netIDType));
	writeState(dest + sizeof(netIDType));
}

void Light::addToUpdatePacket(enet_uint8* dest)
{
	writeState(dest);
	if (updatesLeft > 0)
		updatesLeft--;
}
