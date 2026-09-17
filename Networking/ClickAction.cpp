#include "ClickAction.h"

/*
	Steps are packed by kind, so a sound doesn't pay for the fields only a light uses. Everything is
	written little endian in the same order both sides read it in, like the rest of the packets here.
*/
static unsigned int stepSize(const ClickActionStep& step)
{
	//Every step starts with when it happens and what it is
	unsigned int size = sizeof(uint16_t) + 1;

	switch (step.kind)
	{
		case ClickStep_Sound:
			return size + sizeof(uint16_t) + sizeof(float) * 2;
		case ClickStep_Animation:
			return size + 1;
		case ClickStep_Emitter:
			return size + sizeof(uint16_t) * 2 + sizeof(float) * 3;
		case ClickStep_Light:
			return size + sizeof(uint16_t) + sizeof(float) * 3 + sizeof(float) * 3 + sizeof(float) * 2;
	}

	return size;
}

unsigned int ClickAction::packedSize() const
{
	unsigned int size = sizeof(netIDType) + sizeof(uint32_t) + sizeof(uint16_t) + 1 + 1;

	for (const ClickActionStep& step : steps)
		size += stepSize(step);

	return size;
}

template<typename T> static void writeValue(enet_uint8* dest, unsigned int& at, const T& value)
{
	memcpy(dest + at, &value, sizeof(T));
	at += sizeof(T);
}

template<typename T> static bool readValue(const enet_uint8* source, unsigned int& at, unsigned int length, T& value)
{
	if (at + sizeof(T) > length)
		return false;

	memcpy(&value, source + at, sizeof(T));
	at += sizeof(T);
	return true;
}

unsigned int ClickAction::writeTo(enet_uint8* dest) const
{
	unsigned int at = 0;

	writeValue(dest, at, itemID);
	writeValue(dest, at, generation);
	writeValue(dest, at, repeatMS);
	dest[at++] = repeatLimit;
	dest[at++] = (unsigned char)std::min((size_t)maxClickActionSteps, steps.size());

	for (unsigned int a = 0; a < steps.size() && a < maxClickActionSteps; a++)
	{
		const ClickActionStep& step = steps[a];

		writeValue(dest, at, step.atMS);
		dest[at++] = (unsigned char)step.kind;

		switch (step.kind)
		{
			case ClickStep_Sound:
				writeValue(dest, at, step.soundID);
				writeValue(dest, at, step.pitch);
				writeValue(dest, at, step.volume);
				break;

			case ClickStep_Animation:
				dest[at++] = step.animationID;
				break;

			case ClickStep_Emitter:
				writeValue(dest, at, step.emitterTypeID);
				writeValue(dest, at, step.forMS);
				writeValue(dest, at, step.offset.x);
				writeValue(dest, at, step.offset.y);
				writeValue(dest, at, step.offset.z);
				break;

			case ClickStep_Light:
				writeValue(dest, at, step.forMS);
				writeValue(dest, at, step.offset.x);
				writeValue(dest, at, step.offset.y);
				writeValue(dest, at, step.offset.z);
				writeValue(dest, at, step.color.x);
				writeValue(dest, at, step.color.y);
				writeValue(dest, at, step.color.z);
				writeValue(dest, at, step.brightness);
				writeValue(dest, at, step.coronaWidth);
				break;
		}
	}

	return at;
}

bool ClickAction::readFrom(const enet_uint8* source, unsigned int length)
{
	unsigned int at = 0;
	steps.clear();

	if (!readValue(source, at, length, itemID)
		|| !readValue(source, at, length, generation)
		|| !readValue(source, at, length, repeatMS))
		return false;

	if (at + 2 > length)
		return false;

	repeatLimit = source[at++];
	unsigned int count = source[at++];

	if (count > maxClickActionSteps)
		return false;

	steps.reserve(count);

	for (unsigned int a = 0; a < count; a++)
	{
		ClickActionStep step;

		if (!readValue(source, at, length, step.atMS))
			return false;

		if (at + 1 > length)
			return false;

		unsigned char kind = source[at++];
		if (kind > ClickStep_Light)
			return false;

		step.kind = (ClickActionStepKind)kind;

		switch (step.kind)
		{
			case ClickStep_Sound:
				if (!readValue(source, at, length, step.soundID)
					|| !readValue(source, at, length, step.pitch)
					|| !readValue(source, at, length, step.volume))
					return false;
				break;

			case ClickStep_Animation:
				if (at + 1 > length)
					return false;
				step.animationID = source[at++];
				break;

			case ClickStep_Emitter:
				if (!readValue(source, at, length, step.emitterTypeID)
					|| !readValue(source, at, length, step.forMS)
					|| !readValue(source, at, length, step.offset.x)
					|| !readValue(source, at, length, step.offset.y)
					|| !readValue(source, at, length, step.offset.z))
					return false;
				break;

			case ClickStep_Light:
				if (!readValue(source, at, length, step.forMS)
					|| !readValue(source, at, length, step.offset.x)
					|| !readValue(source, at, length, step.offset.y)
					|| !readValue(source, at, length, step.offset.z)
					|| !readValue(source, at, length, step.color.x)
					|| !readValue(source, at, length, step.color.y)
					|| !readValue(source, at, length, step.color.z)
					|| !readValue(source, at, length, step.brightness)
					|| !readValue(source, at, length, step.coronaWidth))
					return false;
				break;
		}

		steps.push_back(step);
	}

	return true;
}
