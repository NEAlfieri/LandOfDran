#include "Quantization.h"


//The three smallest components of a quaternion can never be larger than sqrt(2)/2 so we multiply by this to normalize to 0.0-1.0 before turning into an integer
const float quatCompMulti = 1.414f;

/*
	This uses sends the three smallest components normalized from 0 to sqrt(2)/2 with an extra 2 bits to encode which component was largest
	A 4-float 128-bit quaternion is compressed to 32 bits
*/
void addQuaternion(enet_uint8* dest, glm::quat quat)
{
	//First byte cleared in switch statement
	dest[1] = 0;
	dest[2] = 0;
	dest[3] = 0;

	//Look at the 4 components of the quaternion, and figure out which is the biggest and what sign it has
	int largestSign = quat[0] > 0 ? 1 : -1;
	int maxIndex = 0;
	float maxValue = fabs(quat[0]);

	for(int i = 0; i<4; i++)
	{
		if(fabs(quat[i]) > fabs(maxValue))
		{
			maxValue = quat[i];
			maxIndex = i;
			largestSign = quat[i] > 0 ? 1 : -1;
		}
	}

	//TODO: If maxValue ~= 1 then we could just signal (no) rotation with a single bit


	//Get the smaller 3 components in order
	float a = 0;
	float b = 0;
	float c = 0;

	//Setting dest[0] upper 2 bits encodes which of the the four components we are not sending
	switch (maxIndex)
	{
	case 0:
		a = quat[1];
		b = quat[2];
		c = quat[3];
		dest[0] = 0;
		break;
	case 1:
		a = quat[0];
		b = quat[2];
		c = quat[3];
		dest[0] = 64;
		break;
	case 2:
		a = quat[0];
		b = quat[1];
		c = quat[3];
		dest[0] = 128;
		break;
	case 3:
		a = quat[0];
		b = quat[1];
		c = quat[2];
		dest[0] = 192;
		break;
	}

	//This is so we do not need to transmit the sign of the largest component, if its neg. just invert everything else
	a *= largestSign;
	b *= largestSign;
	c *= largestSign;

	//Convert each to 9 bits, 10 including the sign
	unsigned short aInt = fabs(a) * quatCompMulti * 511.f;
	unsigned short bInt = fabs(b) * quatCompMulti * 511.f;
	unsigned short cInt = fabs(c) * quatCompMulti * 511.f;

	aInt |= a < 0 ? 512 : 0;
	bInt |= b < 0 ? 512 : 0;
	cInt |= c < 0 ? 512 : 0;

	//Upper 6 bits of A go to lower 6 bits of 0
	dest[0] |= (aInt >> 4);
	//Lower 4 bits of A go to upper 4 bits of 1
	dest[1] |= (aInt & 0b1111) << 4;

	//Upper 4 bits of B go to lower 4 bits of 1
	dest[1] |= (bInt >> 6);
	//Lower 6 bits of B go to upper 6 bits of 2
	dest[2] |= (bInt & 0b111111) << 2;

	//Upper 2 bits of C go to lower 2 bits of 2
	dest[2] |= (cInt >> 8);
	//Lower 8 bits of C are all of 3
	dest[3] |= (cInt & 0b11111111);
}

void getQuaternion(enet_uint8 const* src, glm::quat& quat)
{
	//Which component was omitted
	int largest = src[0] & 0b11000000;

	//Get the raw integer bit values for the smaller 3 components
	short rawA = (src[0] & 0b00111111) << 4;
	rawA |= (src[1] >> 4);

	short rawB = (src[1] & 0b00001111) << 6;
	rawB |= (src[2] >> 2);

	short rawC = (src[2] & 0b00000011) << 8;
	rawC |= src[3];

	//Turn three smaller components from 0 - 511 to 0 to sqrt(2)/2
	float a = (rawA & 0b0111111111);
	a /= 511.f;
	a /= quatCompMulti;
	a *= (rawA & 0b1000000000) ? -1 : 1;

	float b = (rawB & 0b0111111111);
	b /= 511.f;
	b /= quatCompMulti;
	b *= (rawB & 0b1000000000) ? -1 : 1;

	float c = (rawC & 0b0111111111);
	c /= 511.f;
	c /= quatCompMulti;
	c *= (rawC & 0b1000000000) ? -1 : 1;

	//Reconstitute largest component from 3 smallest ones
	float d = sqrt(1.0f - (a * a + b * b + c * c));

	switch (largest)
	{
		case 0:
			quat[1] = a;
			quat[2] = b;
			quat[3] = c;
			quat[0] = d;

		break;
		case 64:
			quat[0] = a;
			quat[1] = d;
			quat[2] = b;
			quat[3] = c;

		break;
		case 128:
			quat[0] = a;
			quat[1] = b;
			quat[2] = d;
			quat[3] = c;

		break;
		case 192:
			quat[0] = a;
			quat[1] = b;
			quat[2] = c;
			quat[3] = d;

		break;
	}
}

/*
	Each component is a 21 bit unsigned step count off of -PositionMaxCoordinate, PositionStepsPerStud steps to a stud,
	so -16384 to +16384 studs in every axis at 1/64th of a stud a step
	The three of them pack into 8 bytes big endian with the top bit of dest[0] left clear for whatever wants it later

	The old encoding spent 16 bits a component on a sign, 9 integer bits and 6 fraction bits, which walled objects in at
	+/-511.98 studs - well inside the +/-32767 studs a brick can be placed at, see BrickHolder::writeRecord
*/
#define PositionComponentBits 21
static const uint32_t positionMaxStep = (1u << PositionComponentBits) - 1;

//Coordinates past the edge of the world clamp rather than wrap, so a runaway object parks at the border instead of teleporting across it
static uint32_t quantizePosition(float value)
{
	float steps = (value + PositionMaxCoordinate) * PositionStepsPerStud;

	//Written so a NaN, which compares false against everything, lands on 0 instead of an unspecified lround
	if (!(steps > 0.0f))
		return 0;
	if (steps >= (float)positionMaxStep)
		return positionMaxStep;

	return (uint32_t)std::lround(steps);
}

static float dequantizePosition(uint32_t steps)
{
	return (float)steps / PositionStepsPerStud - PositionMaxCoordinate;
}

void addPosition(enet_uint8* dest, const glm::vec3& pos)
{
	uint64_t packed = ((uint64_t)quantizePosition(pos.x) << (PositionComponentBits * 2))
					| ((uint64_t)quantizePosition(pos.y) << PositionComponentBits)
					|  (uint64_t)quantizePosition(pos.z);

	for (int a = 0; a < PositionBytes; a++)
		dest[a] = (enet_uint8)(packed >> ((PositionBytes - 1 - a) * 8));
}

void getPosition(enet_uint8 const* src, glm::vec3& pos)
{
	uint64_t packed = 0;
	for (int a = 0; a < PositionBytes; a++)
		packed = (packed << 8) | src[a];

	pos.x = dequantizePosition((uint32_t)((packed >> (PositionComponentBits * 2)) & positionMaxStep));
	pos.y = dequantizePosition((uint32_t)((packed >> PositionComponentBits) & positionMaxStep));
	pos.z = dequantizePosition((uint32_t)(packed & positionMaxStep));
}

/*
	Most of the time an object has not gone far since the last full position we sent for it, so instead of 8 bytes off
	the origin we can send 10 signed bits a component off of that last full position - a "keyframe" - for 4 bytes,
	with the low 2 bits of dest[3] spare. Same 1/64th of a stud a step, so a delta costs no precision at all

	Deltas are always measured from the keyframe and never from the previous delta, which is what makes them safe on an
	unreliable channel: losing one costs that one snapshot and nothing after it. Losing the keyframe itself is the case
	that matters, so the sender stamps a generation onto both (see DynamicExtra_PositionDelta) and the receiver drops
	deltas belonging to a keyframe it never got, until the sender's next one
*/
static const int positionDeltaMinStep = -512;
static const int positionDeltaMaxStep = 511;

bool positionDeltaFits(const glm::vec3& delta)
{
	//A NaN fails every comparison and so lands on a keyframe, which is the safe answer
	return std::fabs(delta.x) <= PositionDeltaMaxOffset
		&& std::fabs(delta.y) <= PositionDeltaMaxOffset
		&& std::fabs(delta.z) <= PositionDeltaMaxOffset;
}

static int quantizePositionDelta(float value)
{
	float steps = value * PositionStepsPerStud;

	if (!(steps > (float)positionDeltaMinStep))
		return positionDeltaMinStep;
	if (steps > (float)positionDeltaMaxStep)
		return positionDeltaMaxStep;

	return (int)std::lround(steps);
}

static float dequantizePositionDelta(int steps)
{
	return (float)steps / PositionStepsPerStud;
}

void addPositionDelta(enet_uint8* dest, const glm::vec3& delta)
{
	uint32_t packed = ((uint32_t)(quantizePositionDelta(delta.x) & 1023) << 22)
					| ((uint32_t)(quantizePositionDelta(delta.y) & 1023) << 12)
					| ((uint32_t)(quantizePositionDelta(delta.z) & 1023) << 2);

	for (int a = 0; a < PositionDeltaBytes; a++)
		dest[a] = (enet_uint8)(packed >> ((PositionDeltaBytes - 1 - a) * 8));
}

//Pulls one 10 bit component out and sign extends it, since it was stored as two's complement
static float unpackPositionDelta(uint32_t packed, int shift)
{
	int steps = (int)((packed >> shift) & 1023);
	if (steps & 512)
		steps -= 1024;

	return dequantizePositionDelta(steps);
}

void getPositionDelta(enet_uint8 const* src, glm::vec3& delta)
{
	uint32_t packed = 0;
	for (int a = 0; a < PositionDeltaBytes; a++)
		packed = (packed << 8) | src[a];

	delta.x = unpackPositionDelta(packed, 22);
	delta.y = unpackPositionDelta(packed, 12);
	delta.z = unpackPositionDelta(packed, 2);
}

//Velocity is hereby bounded to -127 to 127 in each component with 1/255 precision
//TODO: Maybe have more bits for magnitude and less for precision
void addVelocity(enet_uint8* dest, const glm::vec3& vel)
{
	//std::fabs, not a bare abs: whether that picks up the float overload or the int one that throws the fraction
	//away depends on which headers LandOfDran.h happened to drag in
	//First byte stores sign in upper bit and left of the decimal value in lower 7 bits
	//Second byte stores right of the decimal value in 8 bits
	//Repeat for each component for 6 bytes
	int xLeft = (int)std::floor(std::fabs(vel.x));
	dest[0] = (vel.x < 0) ? 128 : 0;
	dest[0] |= std::min(xLeft,127);
	int xRight = (std::fabs(vel.x) - xLeft) * 255;
	dest[1] = xRight;

	int yLeft = (int)std::floor(std::fabs(vel.y));
	dest[2] = (vel.y < 0) ? 128 : 0;
	dest[2] |= std::min(yLeft, 127);
	int yRight = (std::fabs(vel.y) - yLeft) * 255;
	dest[3] = yRight;

	int zLeft = (int)std::floor(std::fabs(vel.z));
	dest[4] = (vel.z < 0) ? 128 : 0;
	dest[4] |= std::min(zLeft, 127);
	int zRight = (std::fabs(vel.z) - zLeft) * 255;
	dest[5] = zRight;
}

void getVelocity(enet_uint8 const* src, glm::vec3& vel)
{
	vel.x = src[1];
	vel.x /= 255.0f;
	vel.x += (src[0] & 0b01111111);
	vel.x *= ((src[0] & 0b10000000) ? -1 : 1);

	vel.y = src[3];
	vel.y /= 255.0f;
	vel.y += (src[2] & 0b01111111);
	vel.y *= ((src[2] & 0b10000000) ? -1 : 1);

	vel.z = src[5];
	vel.z /= 255.0f;
	vel.z += (src[4] & 0b01111111);
	vel.z *= ((src[4] & 0b10000000) ? -1 : 1);
}

/*
	Three signed 10 bit components - a sign bit over a 9 bit magnitude - at AngularVelocityStepsPerRadian steps to a
	radian a second, so -63.875 to 63.875 packed into 4 bytes with the low 2 bits of dest[3] spare

	Precision here matters much less than it does for rotation itself, which is sent separately as a full quaternion,
	and the resend threshold in Dynamic::requiresNetUpdate is ~0.6 radians a second anyway

	The previous version spent 6 integer bits and 4 fraction bits a component and had nowhere left to put a sign, so
	every spin arrived at the client positive - objects thrown with a counter-clockwise spin spun clockwise on the
	client's own physics body until the next rotation snapshot yanked them back
*/
static const int angularVelocityMaxStep = 511;
static const int angularVelocitySignBit = 512;

static uint16_t quantizeAngularVelocity(float value)
{
	float magnitude = std::fabs(value) * AngularVelocityStepsPerRadian;

	//As in quantizePosition, a NaN fails the comparison and clamps instead of reaching lround
	int steps = magnitude < (float)angularVelocityMaxStep ? (int)std::lround(magnitude) : angularVelocityMaxStep;

	return (uint16_t)(steps | (value < 0.0f ? angularVelocitySignBit : 0));
}

static float dequantizeAngularVelocity(uint16_t raw)
{
	float value = (float)(raw & angularVelocityMaxStep) / AngularVelocityStepsPerRadian;
	return (raw & angularVelocitySignBit) ? -value : value;
}

void addAngularVelocity(enet_uint8* dest, const glm::vec3& vel)
{
	uint32_t packed = ((uint32_t)quantizeAngularVelocity(vel.x) << 22)
					| ((uint32_t)quantizeAngularVelocity(vel.y) << 12)
					| ((uint32_t)quantizeAngularVelocity(vel.z) << 2);

	for (int a = 0; a < AngularVelocityBytes; a++)
		dest[a] = (enet_uint8)(packed >> ((AngularVelocityBytes - 1 - a) * 8));
}

void getAngularVelocity(enet_uint8 const* src, glm::vec3& vel)
{
	uint32_t packed = 0;
	for (int a = 0; a < AngularVelocityBytes; a++)
		packed = (packed << 8) | src[a];

	vel.x = dequantizeAngularVelocity((uint16_t)((packed >> 22) & 1023));
	vel.y = dequantizeAngularVelocity((uint16_t)((packed >> 12) & 1023));
	vel.z = dequantizeAngularVelocity((uint16_t)((packed >> 2) & 1023));
}
