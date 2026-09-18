#include "AudioSystem.h"
#include "ReverbPresets.h"
#include "SoundFile.h"
#include "../SimObjects/Dynamic.h"
#include "../SimObjects/Vehicle.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>
#include <AL/efx.h>

//How many meters one world unit (a stud) sounds like, sets how long echoes take to come back
static constexpr float metersPerUnit = 0.5f;
static constexpr float speedOfSound = 343.0f;

//Auto reverb eases toward what the space calls for over roughly this long, and is sent to OpenAL this often
static constexpr float reverbSmoothingMS = 400.0f;
static constexpr float reverbApplyIntervalMS = 50.0f;

//Low-pass filter amounts for a sound behind a thick wall, and for being underwater, multiplied together when both apply
static constexpr float occludedGain = 0.25f;
static constexpr float occludedGainHF = 0.05f;
static constexpr float underwaterGain = 0.7f;
static constexpr float underwaterGainHF = 0.1f;
static constexpr float occlusionSmoothingMS = 80.0f;

//Positioned sounds are at full volume within their sound type's fullVolumeDistance (AudioSystem::defaultFullVolumeDistance
//unless Lua gave another), then lose about 10 dB each time the distance doubles
static constexpr float fullVolumeDistance = AudioSystem::defaultFullVolumeDistance;
static constexpr float distanceRolloff = 1.66f;
//How much high end distant sounds lose on top of that, OpenAL's air absorption treating a stud as a meter
static constexpr float airAbsorption = 1.5f;
//Voices fall off the same way, but from farther out so people standing near each other can talk comfortably
static constexpr float voiceFullVolumeDistance = 10.0f;

//Doppler effect, with sound traveling speedOfSound studs a second. Sounds on a Dynamic use its physics velocity. A listener that
//isn't given a velocity gets one from how it moves, measured over at least velocitySampleMS and smoothed over roughly velocitySmoothingMS,
//where moving faster than maxDopplerSpeed is a teleport or camera snap rather than real movement
//A sound and the listener closing in on or pulling apart slower than dopplerFloorSpeed don't shift at all, so walking toward music
//leaves it alone. The shift eases in from there and is the real amount from dopplerFullSpeed up
static constexpr float dopplerFloorSpeed = 12.0f;
static constexpr float dopplerFullSpeed = 30.0f;
static constexpr float dopplerStrength = 1.0f;
static constexpr float maxDopplerSpeed = 150.0f;
static constexpr float velocitySampleMS = 50.0f;
static constexpr float velocitySmoothingMS = 100.0f;
static constexpr float underwaterFadeMS = 250.0f;

static const EFXEAXREVERBPROPERTIES underwaterReverb = EFX_REVERB_PRESET_UNDERWATER;

//Rain with no open sky above, as a share of its volume out in the open and how much high end it keeps, see AudioSystem::updateRain
static constexpr float rainIndoorGain = 0.12f;
static constexpr float rainIndoorGainHF = 0.08f;
static constexpr float rainExposureSmoothingMS = 500.0f;

//Where it's drawn, or where its body is if it hasn't been drawn yet
static glm::vec3 soundPositionOf(const Dynamic& dynamic)
{
	return dynamic.renderedTransformInitialized ? dynamic.renderedPosition : b2g3(dynamic.getPosition());
}

SoundLocation SoundLocation::at(const glm::vec3& position)
{
	SoundLocation ret;
	ret.kind = Fixed;
	ret.position = position;
	return ret;
}

SoundLocation SoundLocation::on(const std::shared_ptr<Dynamic>& dynamic)
{
	SoundLocation ret;
	ret.kind = Attached;
	ret.dynamic = dynamic;
	if (dynamic)
		ret.position = soundPositionOf(*dynamic);
	return ret;
}

SoundLocation SoundLocation::onVehicle(const std::shared_ptr<Vehicle>& vehicle)
{
	SoundLocation ret;
	ret.kind = Attached;
	ret.vehicle = vehicle;
	if (vehicle)
		ret.position = vehicle->renderedPosition;
	return ret;
}

bool SoundLocation::follow()
{
	if (kind != Attached)
		return true;

	if (std::shared_ptr<Dynamic> target = dynamic.lock())
	{
		position = soundPositionOf(*target);
		return true;
	}

	if (std::shared_ptr<Vehicle> target = vehicle.lock())
	{
		position = target->renderedPosition;
		return true;
	}

	return false;
}

const btRigidBody* SoundLocation::body() const
{
	if (kind != Attached)
		return nullptr;

	if (std::shared_ptr<Dynamic> target = dynamic.lock())
		return target->body;

	std::shared_ptr<Vehicle> target = vehicle.lock();
	return target ? target->body : nullptr;
}

//The old client's list, see ReverbPresets.h for the names
static bool getReverbPreset(const std::string& name, EFXEAXREVERBPROPERTIES& result)
{
	static const std::map<std::string, EFXEAXREVERBPROPERTIES> presets =
	{
		{ "generic",		EFX_REVERB_PRESET_GENERIC },
		{ "paddedcell",		EFX_REVERB_PRESET_PADDEDCELL },
		{ "auditorium",		EFX_REVERB_PRESET_AUDITORIUM },
		{ "concerthall",	EFX_REVERB_PRESET_CONCERTHALL },
		{ "cave",			EFX_REVERB_PRESET_CAVE },
		{ "forest",			EFX_REVERB_PRESET_FOREST },
		{ "plain",			EFX_REVERB_PRESET_PLAIN },
		{ "underwater",		EFX_REVERB_PRESET_UNDERWATER },
		{ "drugged",		EFX_REVERB_PRESET_DRUGGED },
		{ "dizzy",			EFX_REVERB_PRESET_DIZZY },
		{ "psychotic",		EFX_REVERB_PRESET_PSYCHOTIC },
		{ "outhouse",		EFX_REVERB_PRESET_PREFAB_OUTHOUSE },
		{ "heaven",			EFX_REVERB_PRESET_MOOD_HEAVEN },
		{ "hell",			EFX_REVERB_PRESET_MOOD_HELL },
		{ "memory",			EFX_REVERB_PRESET_MOOD_MEMORY },
		{ "dustyroom",		EFX_REVERB_PRESET_DUSTYROOM },
		{ "waterroom",		EFX_REVERB_PRESET_SMALLWATERROOM },
		{ "racer",			EFX_REVERB_PRESET_DRIVING_INCAR_RACER },
		{ "tunnel",			EFX_REVERB_PRESET_DRIVING_TUNNEL }
	};

	auto found = presets.find(name);
	if (found == presets.end())
		return false;

	result = found->second;
	return true;
}

/*
	Reverb for a space with surfaces averageDistance away (world units) that closes in enclosure (0-1) of the way around
	Bigger spaces echo longer and later, closed in spaces echo louder, and nearby walls give strong early reflections
	Out in the open there's barely any
*/
static EFXEAXREVERBPROPERTIES reverbForSpace(float averageDistance, float enclosure)
{
	EFXEAXREVERBPROPERTIES reverb = EFX_REVERB_PRESET_GENERIC;

	float size = std::max(averageDistance * metersPerUnit, 0.1f);
	float closedIn = std::clamp(enclosure, 0.0f, 1.0f);

	reverb.flDecayTime = std::clamp((0.3f + size * 0.12f) * (0.4f + 0.6f * closedIn), 0.1f, 20.0f);
	reverb.flReflectionsDelay = std::clamp(size * 2.0f / speedOfSound, 0.0f, 0.3f);
	reverb.flLateReverbDelay = std::clamp(size / speedOfSound + 0.005f, 0.0f, 0.1f);
	reverb.flGain = 0.02f + 0.3f * closedIn * std::sqrt(closedIn);
	reverb.flReflectionsGain = std::clamp(0.05f + 1.2f * closedIn * std::clamp(6.0f / size, 0.2f, 1.0f), 0.0f, 3.16f);
	reverb.flLateReverbGain = 1.2589f * (0.3f + 0.7f * closedIn);
	reverb.flDensity = std::clamp(0.3f + size / 15.0f, 0.3f, 1.0f);

	return reverb;
}

//Moves every parameter of into amount (0-1) of the way to toward
static void blendReverb(EFXEAXREVERBPROPERTIES& into, const EFXEAXREVERBPROPERTIES& toward, float amount)
{
	auto blend = [amount](float& value, float target) { value += (target - value) * amount; };

	blend(into.flDensity, toward.flDensity);
	blend(into.flDiffusion, toward.flDiffusion);
	blend(into.flGain, toward.flGain);
	blend(into.flGainHF, toward.flGainHF);
	blend(into.flGainLF, toward.flGainLF);
	blend(into.flDecayTime, toward.flDecayTime);
	blend(into.flDecayHFRatio, toward.flDecayHFRatio);
	blend(into.flDecayLFRatio, toward.flDecayLFRatio);
	blend(into.flReflectionsGain, toward.flReflectionsGain);
	blend(into.flReflectionsDelay, toward.flReflectionsDelay);
	blend(into.flLateReverbGain, toward.flLateReverbGain);
	blend(into.flLateReverbDelay, toward.flLateReverbDelay);
	blend(into.flEchoTime, toward.flEchoTime);
	blend(into.flEchoDepth, toward.flEchoDepth);
	blend(into.flModulationTime, toward.flModulationTime);
	blend(into.flModulationDepth, toward.flModulationDepth);
	blend(into.flAirAbsorptionGainHF, toward.flAirAbsorptionGainHF);
	blend(into.flHFReference, toward.flHFReference);
	blend(into.flLFReference, toward.flLFReference);
	blend(into.flRoomRolloffFactor, toward.flRoomRolloffFactor);
	for (int a = 0; a < 3; a++)
	{
		blend(into.flReflectionsPan[a], toward.flReflectionsPan[a]);
		blend(into.flLateReverbPan[a], toward.flLateReverbPan[a]);
	}

	if (amount >= 0.5f)
		into.iDecayHFLimit = toward.iDecayHFLimit;
}

void AudioSystem::placeSource(ALuint source, const SoundLocation& where)
{
	bool positioned = where.kind != SoundLocation::Flat;
	glm::vec3 position = positioned ? where.position : glm::vec3(0);

	//Relative to the listener at 0,0,0 is how OpenAL plays something with no direction
	alSourcei(source, AL_SOURCE_RELATIVE, positioned ? AL_FALSE : AL_TRUE);
	alSource3f(source, AL_POSITION, position.x, position.y, position.z);
	alSource3f(source, AL_VELOCITY, 0, 0, 0);

	if (canSpatializeStereo)
		alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, positioned ? AL_TRUE : AL_FALSE);
}

//How fast a sound is moving, for the Doppler effect. The physics velocity the server sends, rather than how the smoothed drawn
//position moves, which would turn teleports into fast glides and overshoot while the drawn position catches up
static glm::vec3 soundVelocity(const SoundLocation& where)
{
	if (where.kind != SoundLocation::Attached)
		return glm::vec3(0);

	//A vehicle's body on clients only follows where it's drawn, so it goes by the velocity the server sent
	if (std::shared_ptr<Vehicle> vehicle = where.vehicle.lock())
		return vehicle->serverVelocity;

	//Someone riding a vehicle has their body parked out of the physics world with no velocity, so they move with whatever carries them
	if (std::shared_ptr<Dynamic> dynamic = where.dynamic.lock())
	{
		if (std::shared_ptr<Vehicle> vehicle = dynamic->ridingVehicle.lock())
			return vehicle->serverVelocity;
	}

	const btRigidBody* body = where.body();
	if (!body)
		return glm::vec3(0);

	const btVector3& velocity = body->getLinearVelocity();
	return glm::vec3(velocity.x(), velocity.y(), velocity.z());
}

glm::vec3 AudioSystem::dopplerVelocity(const SoundLocation& where) const
{
	//Played relative to the listener, where OpenAL ignores the listener's velocity anyway
	if (where.kind == SoundLocation::Flat)
		return glm::vec3(0);

	glm::vec3 toListener = listenerPosition - where.position;
	float distance = glm::length(toListener);
	if (distance < 0.001f)
		return glm::vec3(0);

	//Only how fast they close in on each other bends pitch, so the source gets just that, along the line between them, and the listener stays still
	glm::vec3 axis = toListener / distance;
	float closing = glm::dot(soundVelocity(where) - listenerMoving, axis);
	float speed = std::abs(closing);
	float kept = speed * glm::smoothstep(dopplerFloorSpeed, dopplerFullSpeed, speed);
	return axis * std::copysign(kept, closing);
}

glm::vec3 AudioSystem::trackVelocity(Motion& motion, const glm::vec3& position, float deltaT)
{
	if (!motion.tracked)
	{
		motion.tracked = true;
		motion.lastPosition = position;
		motion.sinceSampleMS = 0;
		return motion.velocity;
	}

	//Interpolated positions wobble a little from frame to frame, which at hundreds of frames a second would look like huge speeds
	motion.sinceSampleMS += deltaT;
	if (motion.sinceSampleMS < velocitySampleMS)
		return motion.velocity;

	float sampleMS = motion.sinceSampleMS;
	glm::vec3 moved = (position - motion.lastPosition) / (sampleMS / 1000.0f);
	motion.lastPosition = position;
	motion.sinceSampleMS = 0;

	//Teleports and the like would otherwise swoop the pitch
	if (glm::length(moved) > maxDopplerSpeed)
	{
		motion.velocity = glm::vec3(0);
		return motion.velocity;
	}

	motion.velocity += (moved - motion.velocity) * (1.0f - std::exp(-sampleMS / velocitySmoothingMS));
	return motion.velocity;
}

void AudioSystem::connectReverb(bool on)
{
	if (!effectSlot)
		return;

	reverbConnected = on;
	ALint slot = on ? (ALint)effectSlot : AL_EFFECTSLOT_NULL;

	for (int a = 0; a < generalSourceCount; a++)
		alSource3i(generalSources[a], AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL);
	for (int a = 0; a < loopSourceCount; a++)
		alSource3i(loopSources[a], AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL);
	for (int a = 0; a < voiceSourceCount; a++)
		alSource3i(voiceSources[a], AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL);

	//The muffling filters go back on the new sends the next time each source's filter is applied
	for (Occlusion& occlusion : generalOcclusion)
		occlusion.appliedGain = -1;
	for (Occlusion& occlusion : loopOcclusion)
		occlusion.appliedGain = -1;
	for (Occlusion& occlusion : voiceOcclusion)
		occlusion.appliedGain = -1;
}

void AudioSystem::updateReverbConnection()
{
	connectReverb(reverbMode == ReverbPreset || (reverbMode == ReverbAuto && environmentalReverb));
}

void AudioSystem::applyReverb(const EFXEAXREVERBPROPERTIES& reverb)
{
	if (!effect || !effectSlot)
		return;

	if (useEaxReverb)
	{
		alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
		alEffectf(effect, AL_EAXREVERB_DENSITY, reverb.flDensity);
		alEffectf(effect, AL_EAXREVERB_DIFFUSION, reverb.flDiffusion);
		alEffectf(effect, AL_EAXREVERB_GAIN, reverb.flGain);
		alEffectf(effect, AL_EAXREVERB_GAINHF, reverb.flGainHF);
		alEffectf(effect, AL_EAXREVERB_GAINLF, reverb.flGainLF);
		alEffectf(effect, AL_EAXREVERB_DECAY_TIME, reverb.flDecayTime);
		alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, reverb.flDecayHFRatio);
		alEffectf(effect, AL_EAXREVERB_DECAY_LFRATIO, reverb.flDecayLFRatio);
		alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb.flReflectionsGain);
		alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb.flReflectionsDelay);
		alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb.flReflectionsPan);
		alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb.flLateReverbGain);
		alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb.flLateReverbDelay);
		alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb.flLateReverbPan);
		alEffectf(effect, AL_EAXREVERB_ECHO_TIME, reverb.flEchoTime);
		alEffectf(effect, AL_EAXREVERB_ECHO_DEPTH, reverb.flEchoDepth);
		alEffectf(effect, AL_EAXREVERB_MODULATION_TIME, reverb.flModulationTime);
		alEffectf(effect, AL_EAXREVERB_MODULATION_DEPTH, reverb.flModulationDepth);
		alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb.flAirAbsorptionGainHF);
		alEffectf(effect, AL_EAXREVERB_HFREFERENCE, reverb.flHFReference);
		alEffectf(effect, AL_EAXREVERB_LFREFERENCE, reverb.flLFReference);
		alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb.flRoomRolloffFactor);
		alEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb.iDecayHFLimit);
	}
	else
	{
		alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
		alEffectf(effect, AL_REVERB_DENSITY, reverb.flDensity);
		alEffectf(effect, AL_REVERB_DIFFUSION, reverb.flDiffusion);
		alEffectf(effect, AL_REVERB_GAIN, reverb.flGain);
		alEffectf(effect, AL_REVERB_GAINHF, reverb.flGainHF);
		alEffectf(effect, AL_REVERB_DECAY_TIME, reverb.flDecayTime);
		alEffectf(effect, AL_REVERB_DECAY_HFRATIO, reverb.flDecayHFRatio);
		alEffectf(effect, AL_REVERB_REFLECTIONS_GAIN, reverb.flReflectionsGain);
		alEffectf(effect, AL_REVERB_REFLECTIONS_DELAY, reverb.flReflectionsDelay);
		alEffectf(effect, AL_REVERB_LATE_REVERB_GAIN, reverb.flLateReverbGain);
		alEffectf(effect, AL_REVERB_LATE_REVERB_DELAY, reverb.flLateReverbDelay);
		alEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF, reverb.flAirAbsorptionGainHF);
		alEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, reverb.flRoomRolloffFactor);
		alEffecti(effect, AL_REVERB_DECAY_HFLIMIT, reverb.iDecayHFLimit);
	}

	//A slot copies the effect's settings when it's attached, so it has to be attached again after every change
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, (ALint)effect);
}

void AudioSystem::stopLoopSource(Loop& loop, bool rememberOffset)
{
	if (loop.source == -1)
		return;

	ALuint source = loopSources[loop.source];
	if (rememberOffset)
		alGetSourcei(source, AL_SAMPLE_OFFSET, &loop.sampleOffset);
	else
		loop.sampleOffset = 0;

	alSourceStop(source);
	alSourcei(source, AL_BUFFER, 0);

	loopSourceUsed[loop.source] = false;
	loop.source = -1;
}

void AudioSystem::applyDirectFilter(ALuint source, Occlusion& occlusion)
{
	if (!directFilter)
		return;

	float gain = glm::mix(1.0f, occludedGain, occlusion.current) * glm::mix(1.0f, underwaterGain, underwaterAmount);
	float gainHF = glm::mix(1.0f, occludedGainHF, occlusion.current) * glm::mix(1.0f, underwaterGainHF, underwaterAmount);

	if (std::abs(gain - occlusion.appliedGain) < 0.003f && std::abs(gainHF - occlusion.appliedGainHF) < 0.003f)
		return;

	//Like effect slots, a source copies the filter's settings when it's attached
	alFilterf(directFilter, AL_LOWPASS_GAIN, gain);
	alFilterf(directFilter, AL_LOWPASS_GAINHF, gainHF);
	alSourcei(source, AL_DIRECT_FILTER, (ALint)directFilter);

	//The reverb hears the sound through the same wall or water, or its echo would give a muffled sound away
	if (reverbConnected)
		alSource3i(source, AL_AUXILIARY_SEND_FILTER, (ALint)effectSlot, 0, (ALint)directFilter);

	occlusion.appliedGain = gain;
	occlusion.appliedGainHF = gainHF;
}

void AudioSystem::startOcclusion(ALuint source, Occlusion& occlusion, const SoundLocation& where)
{
	occlusion = Occlusion();

	if (occlusionOn && occlusionTest && where.kind != SoundLocation::Flat)
	{
		occlusion.target = occlusionTest(listenerPosition, where.position, where.body());
		occlusion.current = occlusion.target;
	}

	applyDirectFilter(source, occlusion);
}

void AudioSystem::updateOcclusion(ALuint source, Occlusion& occlusion, const SoundLocation& where, float deltaT)
{
	if (occlusionOn && occlusionTest && where.kind != SoundLocation::Flat)
	{
		occlusion.sinceCheckMS += deltaT;
		if (occlusion.sinceCheckMS >= occlusionIntervalMS)
		{
			occlusion.sinceCheckMS = 0;
			occlusion.target = occlusionTest(listenerPosition, where.position, where.body());
		}
	}
	else
		occlusion.target = 0;

	occlusion.current += (occlusion.target - occlusion.current) * (1.0f - std::exp(-deltaT / occlusionSmoothingMS));
	applyDirectFilter(source, occlusion);
}

void AudioSystem::addSoundType(int id, const std::string& name, const std::string& filePath, bool isMusic, float soundFullVolumeDistance)
{
	scope("AudioSystem::addSoundType");

	if (id < 0)
		return;

	if ((size_t)id >= sounds.size())
		sounds.resize(id + 1);

	SoundType& sound = sounds[id];

	//The server can send the whole list and a newly added sound at the same time to someone who's just joining
	if (sound.name == name && sound.filePath == filePath && sound.isMusic == isMusic)
	{
		sound.fullVolumeDistance = soundFullVolumeDistance;
		return;
	}

	if (sound.buffer)
	{
		//OpenAL won't delete a buffer that a source still has
		for (int a = 0; a < generalSourceCount; a++)
		{
			ALint attached;
			alGetSourcei(generalSources[a], AL_BUFFER, &attached);
			if ((ALuint)attached != sound.buffer)
				continue;

			alSourceStop(generalSources[a]);
			alSourcei(generalSources[a], AL_BUFFER, 0);
		}

		for (Loop& loop : loops)
			if (loop.soundID == id)
				stopLoopSource(loop, false);

		alDeleteBuffers(1, &sound.buffer);
		sound.buffer = 0;
	}

	sound.name = name;
	sound.filePath = filePath;
	sound.isMusic = isMusic;
	sound.fullVolumeDistance = soundFullVolumeDistance;

	if (!valid)
		return;

	//File paths come from the server, don't let one reach outside the game's folder
	if (!isPathInsideGameFolder(filePath))
	{
		error("Not loading sound " + name + ", its file isn't inside the game folder: " + filePath);
		return;
	}

	std::vector<int16_t> samples;
	int channels = 0, sampleRate = 0;
	if (!decodeSoundFile(filePath, samples, channels, sampleRate))
		return;

	alGenBuffers(1, &sound.buffer);
	alBufferData(sound.buffer, channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, samples.data(), (ALsizei)(samples.size() * sizeof(int16_t)), sampleRate);

	ALenum alError = alGetError();
	if (alError != AL_NO_ERROR)
	{
		error("OpenAL error " + std::to_string(alError) + " loading sound " + filePath);
		alDeleteBuffers(1, &sound.buffer);
		sound.buffer = 0;
		return;
	}

	debug("Loaded sound " + name + " from " + filePath);
}

int AudioSystem::findSound(const std::string& name) const
{
	for (size_t a = 0; a < sounds.size(); a++)
		if (sounds[a].name == name)
			return (int)a;

	return -1;
}

void AudioSystem::playSound(int soundID, const SoundLocation& where, float pitch, float volume)
{
	if (!valid || soundID < 0 || (size_t)soundID >= sounds.size() || !sounds[soundID].buffer)
		return;

	//Find a source that's done playing, starting after the one used last
	int index = lastUsedGeneralSource;
	bool foundFree = false;
	for (int searched = 0; searched < generalSourceCount; searched++)
	{
		index = (index + 1) % generalSourceCount;

		ALint state;
		alGetSourcei(generalSources[index], AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
		{
			foundFree = true;
			break;
		}
	}

	//All of them are busy, cut off the one that's gone longest since being picked
	if (!foundFree)
		index = (lastUsedGeneralSource + 1) % generalSourceCount;

	ALuint source = generalSources[index];
	alSourceStop(source);
	alSourcei(source, AL_BUFFER, sounds[soundID].buffer);
	alSourcef(source, AL_REFERENCE_DISTANCE, sounds[soundID].fullVolumeDistance);
	alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcef(source, AL_PITCH, std::clamp(pitch, 0.05f, 10.0f));
	alSourcef(source, AL_GAIN, std::clamp(volume, 0.0f, 1.0f));

	generalLocations[index] = where;
	generalLocations[index].follow();
	placeSource(source, generalLocations[index]);
	startOcclusion(source, generalOcclusion[index], generalLocations[index]);

	alSourcePlay(source);
	lastUsedGeneralSource = index;

	ALenum alError = alGetError();
	if (alError != AL_NO_ERROR)
		error("OpenAL error " + std::to_string(alError) + " playing sound " + sounds[soundID].name);
}

void AudioSystem::playSound(const std::string& name, const SoundLocation& where, float pitch, float volume)
{
	int soundID = findSound(name);
	if (soundID != -1)
		playSound(soundID, where, pitch, volume);
}

void AudioSystem::startLoop(unsigned int id, int soundID, const SoundLocation& where, float pitch, float volume)
{
	if (stoppedBeforeStarting.erase(id))
		return;

	//Same ID again replaces it
	for (auto loop = loops.begin(); loop != loops.end(); ++loop)
	{
		if (loop->id != id)
			continue;

		stopLoopSource(*loop, false);
		loops.erase(loop);
		break;
	}

	Loop loop;
	loop.id = id;
	loop.soundID = soundID;
	loop.where = where;
	loop.where.follow();
	loop.pitch = std::clamp(pitch, 0.05f, 10.0f);
	loop.volume = std::clamp(volume, 0.0f, 1.0f);
	loops.push_back(loop);

	//It gets a source, if it's close enough, in update
}

void AudioSystem::stopLoop(unsigned int id)
{
	for (auto loop = loops.begin(); loop != loops.end(); ++loop)
	{
		if (loop->id != id)
			continue;

		stopLoopSource(*loop, false);
		loops.erase(loop);
		return;
	}

	stoppedBeforeStarting.insert(id);
}

void AudioSystem::setReverb(const std::string& preset)
{
	scope("AudioSystem::setReverb");

	if (!valid)
		return;

	if (isAutoReverb(preset))
	{
		reverbMode = ReverbAuto;
		updateReverbConnection();
		return;
	}

	if (isNoReverb(preset))
	{
		reverbMode = ReverbOff;
		updateReverbConnection();
		return;
	}

	EFXEAXREVERBPROPERTIES reverb;
	if (!getReverbPreset(normalizeReverbPreset(preset), reverb))
	{
		error("Unknown reverb preset " + preset);
		return;
	}

	reverbMode = ReverbPreset;
	currentReverb = reverb;
	applyReverb(reverb);
	updateReverbConnection();

	ALenum alError = alGetError();
	if (alError != AL_NO_ERROR)
		error("OpenAL error " + std::to_string(alError) + " setting reverb preset " + preset);
}

void AudioSystem::setEnvironmentOptions(int reverbQuality, int occlusionQuality)
{
	static constexpr float intervalsMS[3] = { 250.0f, 150.0f, 100.0f };

	environmentalReverb = reverbQuality > 0;
	occlusionOn = occlusionQuality > 0;
	occlusionIntervalMS = intervalsMS[std::clamp(occlusionQuality, 1, 3) - 1];

	if (valid)
		updateReverbConnection();
}

void AudioSystem::setListenerSpace(float averageDistance, float enclosure)
{
	spaceDistance = averageDistance;
	spaceEnclosure = enclosure;
}

void AudioSystem::updateRain(float deltaT)
{
	if (rainVolume <= 0.0f)
	{
		//Paused rather than stopped, so the next shower picks the loop up where it left off
		if (rainPlaying)
		{
			alSourcePause(rainSource);
			rainPlaying = false;
		}
		return;
	}

	//Decoded the first time it rains, so a server that never rains never loads it
	if (!rainBuffer)
	{
		if (rainLoadFailed)
			return;

		std::vector<int16_t> samples;
		int channels = 0, sampleRate = 0;
		if (!decodeSoundFile(rainSoundPath, samples, channels, sampleRate))
		{
			error("Couldn't load the rain sound " + std::string(rainSoundPath));
			rainLoadFailed = true;
			return;
		}

		alGenBuffers(1, &rainBuffer);
		alBufferData(rainBuffer, channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, samples.data(), (ALsizei)(samples.size() * sizeof(int16_t)), sampleRate);
		alSourcei(rainSource, AL_BUFFER, (ALint)rainBuffer);

		ALenum alError = alGetError();
		if (alError != AL_NO_ERROR)
		{
			error("OpenAL error " + std::to_string(alError) + " loading the rain sound");
			alSourcei(rainSource, AL_BUFFER, 0);
			alDeleteBuffers(1, &rainBuffer);
			rainBuffer = 0;
			rainLoadFailed = true;
			return;
		}
	}

	if (!rainPlaying)
	{
		//Starting from wherever the camera is now, not gliding in from wherever it was when the last rain stopped
		rainExposure = rainExposureTarget;
		alSourcePlay(rainSource);
		rainPlaying = true;
	}
	else
		rainExposure += (rainExposureTarget - rainExposure) * (1.0f - std::exp(-deltaT / rainExposureSmoothingMS));

	//A doorway or open garage still sounds fairly loud, so volume comes back faster than the high end does
	alSourcef(rainSource, AL_GAIN, rainVolume * musicVolume * glm::mix(rainIndoorGain, 1.0f, std::sqrt(rainExposure)));

	if (!directFilter)
		return;

	float gain = glm::mix(1.0f, underwaterGain, underwaterAmount);
	float gainHF = glm::mix(rainIndoorGainHF, 1.0f, rainExposure) * glm::mix(1.0f, underwaterGainHF, underwaterAmount);
	if (std::abs(gain - rainAppliedGain) < 0.003f && std::abs(gainHF - rainAppliedGainHF) < 0.003f)
		return;

	alFilterf(directFilter, AL_LOWPASS_GAIN, gain);
	alFilterf(directFilter, AL_LOWPASS_GAINHF, gainHF);
	alSourcei(rainSource, AL_DIRECT_FILTER, (ALint)directFilter);
	rainAppliedGain = gain;
	rainAppliedGainHF = gainHF;
}

void AudioSystem::setVolumes(float master, float music)
{
	musicVolume = std::clamp(music, 0.0f, 1.0f);

	if (!valid)
		return;

	alListenerf(AL_GAIN, std::clamp(master, 0.0f, 1.0f));

	for (const Loop& loop : loops)
		if (loop.source != -1)
			alSourcef(loopSources[loop.source], AL_GAIN, loop.volume * musicVolume);
}

void AudioSystem::recycleVoiceBuffers(ALuint source)
{
	ALint processed = 0;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
	if (processed <= 0)
		return;

	size_t at = spareVoiceBuffers.size();
	spareVoiceBuffers.resize(at + processed);
	alSourceUnqueueBuffers(source, processed, spareVoiceBuffers.data() + at);
}

int AudioSystem::openVoice(const SoundLocation& where)
{
	if (!valid)
		return -1;

	for (int a = 0; a < voiceSourceCount; a++)
	{
		if (voiceSourceUsed[a])
			continue;

		voiceSourceUsed[a] = true;

		ALuint source = voiceSources[a];
		alSourceStop(source);
		recycleVoiceBuffers(source);
		alSourcei(source, AL_BUFFER, 0);
		alSourcef(source, AL_GAIN, voiceVolume);

		voiceLocations[a] = where;
		voiceLocations[a].follow();
		placeSource(source, voiceLocations[a]);
		startOcclusion(source, voiceOcclusion[a], voiceLocations[a]);

		return a;
	}

	return -1;
}

void AudioSystem::moveVoice(int voice, const SoundLocation& where)
{
	if (!valid || voice < 0 || voice >= voiceSourceCount || !voiceSourceUsed[voice])
		return;

	voiceLocations[voice] = where;
	voiceLocations[voice].follow();
	placeSource(voiceSources[voice], voiceLocations[voice]);
}

void AudioSystem::queueVoice(int voice, const int16_t* samples, int sampleCount, int sampleRate)
{
	if (!valid || voice < 0 || voice >= voiceSourceCount || !voiceSourceUsed[voice] || sampleCount <= 0)
		return;

	ALuint source = voiceSources[voice];

	//Every buffer of a source that ran out counts as processed, taking them off also keeps it from replaying them when it starts again
	recycleVoiceBuffers(source);

	ALuint buffer = 0;
	if (spareVoiceBuffers.empty())
	{
		alGenBuffers(1, &buffer);
		if (!buffer)
		{
			error("OpenAL couldn't make a buffer for voice chat");
			return;
		}
		allVoiceBuffers.push_back(buffer);
	}
	else
	{
		buffer = spareVoiceBuffers.back();
		spareVoiceBuffers.pop_back();
	}

	alBufferData(buffer, AL_FORMAT_MONO16, samples, (ALsizei)(sampleCount * sizeof(int16_t)), sampleRate);
	alSourceQueueBuffers(source, 1, &buffer);

	ALint state;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING)
		alSourcePlay(source);
}

int AudioSystem::queuedVoiceBuffers(int voice) const
{
	if (!valid || voice < 0 || voice >= voiceSourceCount || !voiceSourceUsed[voice])
		return 0;

	ALint queued = 0, processed = 0;
	alGetSourcei(voiceSources[voice], AL_BUFFERS_QUEUED, &queued);
	alGetSourcei(voiceSources[voice], AL_BUFFERS_PROCESSED, &processed);
	return queued - processed;
}

void AudioSystem::closeVoice(int voice)
{
	if (!valid || voice < 0 || voice >= voiceSourceCount || !voiceSourceUsed[voice])
		return;

	ALuint source = voiceSources[voice];
	alSourceStop(source);
	recycleVoiceBuffers(source);
	alSourcei(source, AL_BUFFER, 0);

	voiceSourceUsed[voice] = false;
	voiceLocations[voice] = SoundLocation::flat();
}

void AudioSystem::setVoiceVolume(float volume)
{
	voiceVolume = std::clamp(volume, 0.0f, 1.0f);

	if (!valid)
		return;

	for (int a = 0; a < voiceSourceCount; a++)
		if (voiceSourceUsed[a])
			alSourcef(voiceSources[a], AL_GAIN, voiceVolume);
}

void AudioSystem::update(const glm::vec3& position, const glm::vec3& listenerDirection, std::optional<glm::vec3> listenerVelocity, float deltaT)
{
	if (!valid)
		return;

	listenerPosition = position;
	alListener3f(AL_POSITION, position.x, position.y, position.z);

	//Tracked even while given a velocity, so going without one later doesn't start from a stale position
	glm::vec3 moved = trackVelocity(listenerMotion, position, deltaT);
	listenerMoving = listenerVelocity.value_or(moved);
	//Each positioned source gets its speed relative to the listener instead, see dopplerVelocity
	alListener3f(AL_VELOCITY, 0, 0, 0);

	//Up is world up tilted to be perpendicular to where the camera looks
	glm::vec3 forward = glm::length(listenerDirection) > 0.0001f ? glm::normalize(listenerDirection) : glm::vec3(0, 0, -1);
	glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
	glm::vec3 up = glm::length(right) > 0.0001f ? glm::normalize(glm::cross(right, forward)) : glm::vec3(0, 0, -1);
	ALfloat orientation[6] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
	alListenerfv(AL_ORIENTATION, orientation);

	underwaterAmount = std::clamp(underwaterAmount + (underwater ? 1.0f : -1.0f) * deltaT / underwaterFadeMS, 0.0f, 1.0f);

	updateRain(deltaT);

	//Reverb glides toward what the space around the listener calls for
	if (wantsListenerSpace())
	{
		EFXEAXREVERBPROPERTIES target = reverbForSpace(spaceDistance, spaceEnclosure);
		blendReverb(target, underwaterReverb, underwaterAmount);
		blendReverb(currentReverb, target, 1.0f - std::exp(-deltaT / reverbSmoothingMS));

		sinceReverbAppliedMS += deltaT;
		if (sinceReverbAppliedMS >= reverbApplyIntervalMS)
		{
			sinceReverbAppliedMS = 0;
			applyReverb(currentReverb);
		}
	}

	//One-shot sounds: follow the Dynamic they're on (staying where it was last if it's deleted), and muffling
	for (int a = 0; a < generalSourceCount; a++)
	{
		ALint state;
		alGetSourcei(generalSources[a], AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
		{
			generalLocations[a] = SoundLocation::flat();
			continue;
		}

		if (generalLocations[a].kind == SoundLocation::Attached && generalLocations[a].follow())
			alSource3f(generalSources[a], AL_POSITION, generalLocations[a].position.x, generalLocations[a].position.y, generalLocations[a].position.z);

		glm::vec3 sourceVelocity = dopplerVelocity(generalLocations[a]);
		alSource3f(generalSources[a], AL_VELOCITY, sourceVelocity.x, sourceVelocity.y, sourceVelocity.z);

		updateOcclusion(generalSources[a], generalOcclusion[a], generalLocations[a], deltaT);
	}

	//A loop on a Dynamic ends with it, the server forgets it the same way
	for (auto loop = loops.begin(); loop != loops.end();)
	{
		if (loop->where.follow())
		{
			++loop;
			continue;
		}

		stopLoopSource(*loop, false);
		loop = loops.erase(loop);
	}

	//Closest loops first, loops with no position count as right on top of the listener
	auto distanceSquared = [&position](const Loop& loop)
	{
		if (loop.where.kind == SoundLocation::Flat)
			return 0.0f;
		glm::vec3 offset = loop.where.position - position;
		return glm::dot(offset, offset);
	};

	//Stable so loops the same distance away don't keep trading sources every frame
	std::stable_sort(loops.begin(), loops.end(), [&distanceSquared](const Loop& a, const Loop& b) { return distanceSquared(a) < distanceSquared(b); });

	//Too far, pause them where they are to pick up there later
	for (size_t a = loopSourceCount; a < loops.size(); a++)
		stopLoopSource(loops[a], true);

	for (size_t a = 0; a < std::min(loops.size(), (size_t)loopSourceCount); a++)
	{
		Loop& loop = loops[a];

		if (loop.source != -1)
		{
			if (loop.where.kind != SoundLocation::Flat)
			{
				alSource3f(loopSources[loop.source], AL_POSITION, loop.where.position.x, loop.where.position.y, loop.where.position.z);
				glm::vec3 sourceVelocity = dopplerVelocity(loop.where);
				alSource3f(loopSources[loop.source], AL_VELOCITY, sourceVelocity.x, sourceVelocity.y, sourceVelocity.z);
			}
			updateOcclusion(loopSources[loop.source], loopOcclusion[loop.source], loop.where, deltaT);
			continue;
		}

		if (loop.soundID < 0 || (size_t)loop.soundID >= sounds.size() || !sounds[loop.soundID].buffer)
			continue;

		//Only loopSourceCount loops ever get this far, so there's always a free source
		for (int b = 0; b < loopSourceCount; b++)
		{
			if (loopSourceUsed[b])
				continue;

			loopSourceUsed[b] = true;
			loop.source = b;

			ALuint source = loopSources[b];
			alSourcei(source, AL_BUFFER, sounds[loop.soundID].buffer);
			alSourcef(source, AL_REFERENCE_DISTANCE, sounds[loop.soundID].fullVolumeDistance);
			alSourcei(source, AL_LOOPING, AL_TRUE);
			alSourcef(source, AL_PITCH, loop.pitch);
			alSourcef(source, AL_GAIN, loop.volume * musicVolume);
			placeSource(source, loop.where);
			startOcclusion(source, loopOcclusion[b], loop.where);
			alSourcei(source, AL_SAMPLE_OFFSET, loop.sampleOffset);
			alSourcePlay(source);
			break;
		}
	}

	//Voices follow the talker's player, staying where it was last if it's deleted
	for (int a = 0; a < voiceSourceCount; a++)
	{
		if (!voiceSourceUsed[a])
			continue;

		if (voiceLocations[a].kind == SoundLocation::Attached && voiceLocations[a].follow())
			alSource3f(voiceSources[a], AL_POSITION, voiceLocations[a].position.x, voiceLocations[a].position.y, voiceLocations[a].position.z);

		glm::vec3 sourceVelocity = dopplerVelocity(voiceLocations[a]);
		alSource3f(voiceSources[a], AL_VELOCITY, sourceVelocity.x, sourceVelocity.y, sourceVelocity.z);

		updateOcclusion(voiceSources[a], voiceOcclusion[a], voiceLocations[a], deltaT);
	}

	ALenum alError = alGetError();
	if (alError != AL_NO_ERROR)
		error("OpenAL error " + std::to_string(alError) + " updating audio");
}

void AudioSystem::clear()
{
	stoppedBeforeStarting.clear();

	for (Loop& loop : loops)
		stopLoopSource(loop, false);
	loops.clear();

	reverbMode = ReverbAuto;

	if (valid)
	{
		for (int a = 0; a < generalSourceCount; a++)
		{
			alSourceStop(generalSources[a]);
			alSourcei(generalSources[a], AL_BUFFER, 0);
			generalLocations[a] = SoundLocation::flat();
		}

		for (int a = 0; a < voiceSourceCount; a++)
			closeVoice(a);

		//The buffer is the game's own, so it's kept for the next server that rains
		rainVolume = 0.0f;
		if (rainPlaying)
		{
			alSourceStop(rainSource);
			rainPlaying = false;
		}

		updateReverbConnection();

		for (SoundType& sound : sounds)
			if (sound.buffer)
				alDeleteBuffers(1, &sound.buffer);
	}

	sounds.clear();
}

AudioSystem::AudioSystem()
{
	scope("AudioSystem::AudioSystem");

	currentReverb = reverbForSpace(spaceDistance, spaceEnclosure);

	device = alcOpenDevice(nullptr);
	if (!device)
	{
		error("Could not open an audio device, there won't be any sound");
		return;
	}

	context = alcCreateContext(device, nullptr);
	if (!context || !alcMakeContextCurrent(context))
	{
		error("Could not create an OpenAL context, there won't be any sound");
		if (context)
			alcDestroyContext(context);
		alcCloseDevice(device);
		context = nullptr;
		device = nullptr;
		return;
	}

	alGenSources(generalSourceCount, generalSources);
	alGenSources(loopSourceCount, loopSources);
	alGenSources(voiceSourceCount, voiceSources);
	alGenSources(1, &rainSource);

	ALenum alError = alGetError();
	if (alError != AL_NO_ERROR)
	{
		error("OpenAL error " + std::to_string(alError) + " creating sources, there won't be any sound");
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(context);
		alcCloseDevice(device);
		context = nullptr;
		device = nullptr;
		return;
	}

	canSpatializeStereo = alIsExtensionPresent("AL_SOFT_source_spatialize");

	//Gain is (distance / fullVolumeDistance) to the power of -distanceRolloff, never louder than full volume up close
	//OpenAL's default of 1 / distance is how sound spreads out in open air, but with nothing else to mask them far off sounds stay too loud in game
	alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
	for (ALuint source : generalSources)
	{
		alSourcef(source, AL_REFERENCE_DISTANCE, fullVolumeDistance);
		alSourcef(source, AL_ROLLOFF_FACTOR, distanceRolloff);
	}
	for (ALuint source : loopSources)
	{
		alSourcef(source, AL_REFERENCE_DISTANCE, fullVolumeDistance);
		alSourcef(source, AL_ROLLOFF_FACTOR, distanceRolloff);
	}
	for (ALuint source : voiceSources)
	{
		alSourcef(source, AL_REFERENCE_DISTANCE, voiceFullVolumeDistance);
		alSourcef(source, AL_ROLLOFF_FACTOR, distanceRolloff);
		alSourcei(source, AL_LOOPING, AL_FALSE);
	}

	//Rain is all around, not coming from anywhere, and doesn't go through the reverb
	alSourcei(rainSource, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(rainSource, AL_POSITION, 0, 0, 0);
	alSourcef(rainSource, AL_ROLLOFF_FACTOR, 0);
	alSourcei(rainSource, AL_LOOPING, AL_TRUE);

	alDopplerFactor(dopplerStrength);
	alSpeedOfSound(speedOfSound);

	if (alcIsExtensionPresent(device, "ALC_EXT_EFX"))
	{
		alGenEffects(1, &effect);
		alGenAuxiliaryEffectSlots(1, &effectSlot);
		useEaxReverb = alGetEnumValue("AL_EFFECT_EAXREVERB") != 0;

		alError = alGetError();
		if (alError != AL_NO_ERROR)
		{
			error("OpenAL error " + std::to_string(alError) + " creating reverb, reverb won't do anything");
			effect = 0;
			effectSlot = 0;
		}

		alGenFilters(1, &directFilter);
		alFilteri(directFilter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);

		alError = alGetError();
		if (alError != AL_NO_ERROR)
		{
			error("OpenAL error " + std::to_string(alError) + " creating a low-pass filter, sounds won't be muffled");
			directFilter = 0;
		}

		//Distant sounds lose their high end on top of getting quieter, like they do through real air
		for (ALuint source : generalSources)
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, airAbsorption);
		for (ALuint source : loopSources)
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, airAbsorption);
		for (ALuint source : voiceSources)
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, airAbsorption);
	}
	else
		info("OpenAL implementation has no EFX, reverb and muffling won't do anything");

	const ALCchar* deviceName = alcGetString(device, ALC_DEVICE_SPECIFIER);
	info("Audio started on " + std::string(deviceName ? deviceName : "unknown device"));

	valid = true;

	applyReverb(currentReverb);
	updateReverbConnection();
}

AudioSystem::~AudioSystem()
{
	if (!valid)
		return;

	clear();

	for (int a = 0; a < generalSourceCount; a++)
		alSourcei(generalSources[a], AL_DIRECT_FILTER, AL_FILTER_NULL);
	for (int a = 0; a < loopSourceCount; a++)
		alSourcei(loopSources[a], AL_DIRECT_FILTER, AL_FILTER_NULL);
	for (int a = 0; a < voiceSourceCount; a++)
		alSourcei(voiceSources[a], AL_DIRECT_FILTER, AL_FILTER_NULL);

	alDeleteSources(generalSourceCount, generalSources);
	alDeleteSources(loopSourceCount, loopSources);
	alDeleteSources(voiceSourceCount, voiceSources);
	alSourcei(rainSource, AL_DIRECT_FILTER, AL_FILTER_NULL);
	alDeleteSources(1, &rainSource);
	if (rainBuffer)
		alDeleteBuffers(1, &rainBuffer);
	if (!allVoiceBuffers.empty())
		alDeleteBuffers((ALsizei)allVoiceBuffers.size(), allVoiceBuffers.data());

	if (directFilter)
		alDeleteFilters(1, &directFilter);
	if (effectSlot)
		alDeleteAuxiliaryEffectSlots(1, &effectSlot);
	if (effect)
		alDeleteEffects(1, &effect);

	alcMakeContextCurrent(nullptr);
	alcDestroyContext(context);
	alcCloseDevice(device);
}
