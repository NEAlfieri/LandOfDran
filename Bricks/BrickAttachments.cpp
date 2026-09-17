#include "BrickAttachments.h"

#include "Brick.h"
#include "../SimObjects/Light.h"

#include <cmath>

static float finiteOr(float value, float fallback)
{
	return std::isfinite(value) ? value : fallback;
}

void WheelSettings::clampValues()
{
	const WheelSettings defaults;
	engineForce = std::clamp(finiteOr(engineForce, defaults.engineForce), -2000.0f, 2000.0f);
	brakeForce = std::clamp(finiteOr(brakeForce, defaults.brakeForce), 0.0f, 2000.0f);
	steerAngle = std::clamp(finiteOr(steerAngle, defaults.steerAngle), -3.1415f, 3.1415f);
	suspensionLength = std::clamp(finiteOr(suspensionLength, defaults.suspensionLength), 0.1f, 5.0f);
	suspensionStiffness = std::clamp(finiteOr(suspensionStiffness, defaults.suspensionStiffness), 1.0f, 1000.0f);
	dampingCompression = std::clamp(finiteOr(dampingCompression, defaults.dampingCompression), 1.0f, 100.0f);
	dampingRelaxation = std::clamp(finiteOr(dampingRelaxation, defaults.dampingRelaxation), 1.0f, 100.0f);
	frictionSlip = std::clamp(finiteOr(frictionSlip, defaults.frictionSlip), 0.1f, 10.0f);
	rollInfluence = std::clamp(finiteOr(rollInfluence, defaults.rollInfluence), 0.1f, 10.0f);
}

void SteeringSettings::clampValues()
{
	const SteeringSettings defaults;
	mass = std::clamp(finiteOr(mass, defaults.mass), 1.5f, 30.0f);
	angularDamping = std::clamp(finiteOr(angularDamping, defaults.angularDamping), 0.0f, 1.0f);
}

unsigned char BrickAttachments::getFlags() const
{
	return (musicName.empty() ? 0 : BrickAttachment_Music) | (hasLight ? BrickAttachment_Light : 0) | (emitterName.empty() ? 0 : BrickAttachment_Emitter) |
		(hasWheel ? BrickAttachment_Wheel : 0) | (hasSteering ? BrickAttachment_Steering : 0) | (hasHorn || lightIsHeadlight ? BrickAttachment_Horn : 0);
}

void BrickAttachments::clampValues()
{
	musicName = musicName.substr(0, maxNameLength);
	emitterName = emitterName.substr(0, maxNameLength);
	hornName = hornName.substr(0, maxNameLength);

	//Same ranges as Lua's startSoundLoop
	musicVolume = std::clamp(finiteOr(musicVolume, 1.0f), 0.0f, 1.0f);
	musicPitch = std::clamp(finiteOr(musicPitch, 1.0f), 0.05f, 10.0f);

	for (int axis = 0; axis < 3; axis++)
	{
		lightColor[axis] = std::clamp(finiteOr(lightColor[axis], 1.0f), 0.0f, 1.0f);
		lightDirection[axis] = finiteOr(lightDirection[axis], 0.0f);
		lightOffset[axis] = std::clamp(finiteOr(lightOffset[axis], 0.0f), -maxLightOffset, maxLightOffset);
	}

	lightBrightness = std::clamp(finiteOr(lightBrightness, 0.0f), 0.0f, Light::maxBrightness);
	lightFlicker = std::clamp(finiteOr(lightFlicker, 0.0f), 0.0f, Light::maxFlicker);
	lightBlinkSpeed = std::clamp(finiteOr(lightBlinkSpeed, 0.0f), 0.0f, Light::maxBlinkSpeed);
	lightBlinkStrength = std::clamp(finiteOr(lightBlinkStrength, 1.0f), 0.0f, 1.0f);
	lightCoronaWidth = std::clamp(finiteOr(lightCoronaWidth, 0.0f), 0.0f, Light::maxCoronaWidth);
	lightSpin = std::clamp(finiteOr(lightSpin, 0.0f), -Light::maxSpin, Light::maxSpin);

	lightConeAngle = finiteOr(lightConeAngle, 0.0f);
	lightConeAngle = lightConeAngle <= 0.0f ? 0.0f : std::clamp(lightConeAngle, 1.0f, Light::maxConeAngle);

	float length = glm::length(lightDirection);
	lightDirection = length > 0.0001f ? lightDirection / length : glm::vec3(0, -1, 0);

	wheel.clampValues();
	steering.clampValues();
}

void BrickAttachments::resetLight()
{
	const BrickAttachments defaults;
	lightColor = defaults.lightColor;
	lightBrightness = defaults.lightBrightness;
	lightFlicker = defaults.lightFlicker;
	lightBlinkSpeed = defaults.lightBlinkSpeed;
	lightBlinkStrength = defaults.lightBlinkStrength;
	lightCoronaWidth = defaults.lightCoronaWidth;
	lightConeAngle = defaults.lightConeAngle;
	lightDirection = defaults.lightDirection;
	lightSpin = defaults.lightSpin;
	lightOffset = defaults.lightOffset;
}

//Like a player's flashlight, a little narrower
static constexpr float headlightBrightness = 150.0f;
static constexpr float headlightConeAngle = 70.0f;
static constexpr float headlightCoronaWidth = 0.5f;

void BrickAttachments::resetHeadlight(const glm::vec3& forward)
{
	resetLight();
	lightBrightness = headlightBrightness;
	lightConeAngle = headlightConeAngle;
	lightCoronaWidth = headlightCoronaWidth;
	lightDirection = glm::length(forward) > 0.0001f ? glm::normalize(forward) : glm::vec3(0, 0, 1);
}

void BrickAttachments::writeParts(const std::function<void(const void*, size_t)>& writeBytes) const
{
	auto writeName = [&](const std::string& name)
	{
		unsigned char length = (unsigned char)std::min(name.length(), maxNameLength);
		writeBytes(&length, 1);
		writeBytes(name.data(), length);
	};

	if (!musicName.empty())
	{
		writeName(musicName);
		writeBytes(&musicVolume, sizeof(float));
		writeBytes(&musicPitch, sizeof(float));
	}

	if (hasLight)
	{
		const float light[lightFloatCount] = { lightColor.r, lightColor.g, lightColor.b, lightBrightness, lightFlicker, lightCoronaWidth, lightConeAngle,
			lightDirection.x, lightDirection.y, lightDirection.z, lightSpin, lightOffset.x, lightOffset.y, lightOffset.z, lightBlinkSpeed, lightBlinkStrength };
		writeBytes(light, sizeof(light));
	}

	if (!emitterName.empty())
		writeName(emitterName);

	if (hasWheel)
	{
		const float values[WheelSettings::floatCount] = { wheel.engineForce, wheel.brakeForce, wheel.steerAngle, wheel.suspensionLength, wheel.suspensionStiffness,
			wheel.dampingCompression, wheel.dampingRelaxation, wheel.frictionSlip, wheel.rollInfluence };
		writeBytes(values, sizeof(values));
	}

	if (hasSteering)
	{
		const float values[2] = { steering.mass, steering.angularDamping };
		unsigned char realistic = steering.realisticCenterOfMass ? 1 : 0;
		writeBytes(values, sizeof(values));
		writeBytes(&realistic, 1);
	}

	if (hasHorn || lightIsHeadlight)
	{
		writeName(hornName);
		unsigned char bits = (lightIsHeadlight ? 1 : 0) | (hasHorn ? 2 : 0);
		writeBytes(&bits, 1);
	}
}

bool BrickAttachments::readParts(unsigned char flags, const std::function<bool(void*, size_t)>& readBytes, size_t lightFloats)
{
	auto readName = [&](std::string& name) -> bool
	{
		unsigned char length;
		if (!readBytes(&length, 1))
			return false;

		name.resize(length);
		return length == 0 || readBytes(name.data(), length);
	};

	musicName = "";
	emitterName = "";
	hornName = "";
	hasHorn = false;
	lightIsHeadlight = false;
	hasLight = flags & BrickAttachment_Light;
	hasWheel = flags & BrickAttachment_Wheel;
	hasSteering = flags & BrickAttachment_Steering;

	if ((flags & BrickAttachment_Music) && (!readName(musicName) || !readBytes(&musicVolume, sizeof(float)) || !readBytes(&musicPitch, sizeof(float))))
		return false;

	if (hasLight)
	{
		//Older saves wrote 15 floats with the last always 0, which reads as no blinking, and strength gets its default
		float light[lightFloatCount] = {};
		light[15] = BrickAttachments().lightBlinkStrength;
		if (lightFloats > lightFloatCount || !readBytes(light, sizeof(float) * lightFloats))
			return false;

		lightColor = glm::vec3(light[0], light[1], light[2]);
		lightBrightness = light[3];
		lightFlicker = light[4];
		lightCoronaWidth = light[5];
		lightConeAngle = light[6];
		lightDirection = glm::vec3(light[7], light[8], light[9]);
		lightSpin = light[10];
		lightOffset = glm::vec3(light[11], light[12], light[13]);
		lightBlinkSpeed = light[14];
		lightBlinkStrength = light[15];
	}

	if ((flags & BrickAttachment_Emitter) && !readName(emitterName))
		return false;

	if (hasWheel)
	{
		float values[WheelSettings::floatCount];
		if (!readBytes(values, sizeof(values)))
			return false;

		wheel.engineForce = values[0];
		wheel.brakeForce = values[1];
		wheel.steerAngle = values[2];
		wheel.suspensionLength = values[3];
		wheel.suspensionStiffness = values[4];
		wheel.dampingCompression = values[5];
		wheel.dampingRelaxation = values[6];
		wheel.frictionSlip = values[7];
		wheel.rollInfluence = values[8];
	}

	if (hasSteering)
	{
		float values[2];
		unsigned char realistic;
		if (!readBytes(values, sizeof(values)) || !readBytes(&realistic, 1))
			return false;

		steering.mass = values[0];
		steering.angularDamping = values[1];
		steering.realisticCenterOfMass = realistic & 1;
	}

	if (flags & BrickAttachment_Horn)
	{
		unsigned char bits;
		if (!readName(hornName) || !readBytes(&bits, 1))
			return false;

		lightIsHeadlight = hasLight && (bits & 1);
		hasHorn = bits & 2;
	}

	return true;
}

void BrickAttachments::write(std::vector<unsigned char>& bytes) const
{
	bytes.push_back(getFlags());
	writeParts([&bytes](const void* data, size_t count)
	{
		bytes.insert(bytes.end(), (const unsigned char*)data, (const unsigned char*)data + count);
	});
}

bool BrickAttachments::read(const unsigned char* data, size_t length, size_t& at)
{
	if (at >= length)
		return false;

	unsigned char flags = data[at++];
	return readParts(flags, [&](void* out, size_t count)
	{
		if (at + count > length)
			return false;

		memcpy(out, data + at, count);
		at += count;
		return true;
	});
}
