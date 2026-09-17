#pragma once

#include "../LandOfDran.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx-presets.h>

#include <optional>
#include <set>

//Not included, ClientProgramData includes this and Dynamic's headers lead back to ClientProgramData
class Dynamic;
class Vehicle;
class btRigidBody;

//Where a sound plays from: nowhere in particular (same volume in both ears), a fixed point, or following a Dynamic or Vehicle
struct SoundLocation
{
	enum Kind
	{
		Flat,
		Fixed,
		Attached
	} kind = Flat;

	glm::vec3 position = glm::vec3(0);
	//An attached sound follows one of these
	std::weak_ptr<Dynamic> dynamic;
	std::weak_ptr<Vehicle> vehicle;

	static SoundLocation flat() { return SoundLocation(); }
	static SoundLocation at(const glm::vec3& position);
	static SoundLocation on(const std::shared_ptr<Dynamic>& dynamic);
	static SoundLocation onVehicle(const std::shared_ptr<Vehicle>& vehicle);

	//Moves position to where what it's attached to is drawn, false once that's gone (position stays where it was last)
	bool follow();

	//The physics body of the Dynamic or Vehicle it follows, nullptr if it doesn't follow one
	const btRigidBody* body() const;
};

/*
	Client only: plays the sounds the server registered, through OpenAL
	Ported from the old client's audioPlayer: a pool of sources for one-shot sounds, and a smaller pool for looping sounds
	where only the loops closest to the listener get one, the rest pausing until they're close enough again
	On top of the old client: reverb that follows the space around the listener, muffling underwater, and muffling
	sounds with something in the way (occlusion)
	Lives for the whole program, clear() forgets everything from the server we just left
*/
class AudioSystem
{
public:

	static constexpr int generalSourceCount = 64;
	static constexpr int loopSourceCount = 16;
	static constexpr int voiceSourceCount = 8;

	/*
		0 when nothing is between the listener and a sound, up to 1 when it's completely blocked, see AcousticProbe::occlusion
		sourceBody is the body of a Dynamic the sound follows, if any, which shouldn't count as blocking it
	*/
	typedef std::function<float(const glm::vec3& listener, const glm::vec3& source, const btRigidBody* sourceBody)> OcclusionTest;

private:

	ALCdevice* device = nullptr;
	ALCcontext* context = nullptr;
	bool valid = false;

	//AL_SOFT_source_spatialize, lets stereo sounds have a position too (OpenAL leaves stereo unpositioned otherwise)
	bool canSpatializeStereo = false;

	struct SoundType
	{
		std::string name = "";
		std::string filePath = "";
		bool isMusic = false;
		//0 if the file couldn't be loaded, playing it does nothing
		ALuint buffer = 0;
	};

	//Index is the ID the server gave the sound
	std::vector<SoundType> sounds;

	//How muffled a source is by something in the way
	struct Occlusion
	{
		float target = 0;
		float current = 0;
		float sinceCheckMS = 0;
		//What the source's low-pass filter was last set to, -1 to make sure it gets set
		float appliedGain = -1;
		float appliedGainHF = -1;
	};

	//Where the listener was and how fast it's been moving, for the Doppler effect when update isn't given its velocity, see trackVelocity
	struct Motion
	{
		glm::vec3 lastPosition = glm::vec3(0);
		glm::vec3 velocity = glm::vec3(0);
		float sinceSampleMS = 0;
		bool tracked = false;
	};

	ALuint generalSources[generalSourceCount] = {};
	SoundLocation generalLocations[generalSourceCount];
	Occlusion generalOcclusion[generalSourceCount];
	//Where the search for a free source starts, so a burst of sounds doesn't keep checking the same busy ones
	int lastUsedGeneralSource = 0;

	struct Loop
	{
		unsigned int id = 0;
		int soundID = -1;
		SoundLocation where;
		float pitch = 1.0f;
		float volume = 1.0f;
		//Index into loopSources, -1 while it's too far away to have one
		int source = -1;
		//Where playback left off when it lost its source, so it picks up there later
		ALint sampleOffset = 0;
	};

	std::vector<Loop> loops;
	ALuint loopSources[loopSourceCount] = {};
	bool loopSourceUsed[loopSourceCount] = {};
	Occlusion loopOcclusion[loopSourceCount];

	//Voice chat, see VoiceChat: each plays buffers queued onto it as they're decoded
	ALuint voiceSources[voiceSourceCount] = {};
	bool voiceSourceUsed[voiceSourceCount] = {};
	SoundLocation voiceLocations[voiceSourceCount];
	Occlusion voiceOcclusion[voiceSourceCount];
	//Buffers voice sources finished playing, filled again instead of making new ones. allVoiceBuffers is every one made, to delete at the end
	std::vector<ALuint> spareVoiceBuffers;
	std::vector<ALuint> allVoiceBuffers;
	float voiceVolume = 1.0f;

	//Stop requests for loops we haven't started yet (a start waiting on its Dynamic to arrive), so the start is dropped instead
	std::set<unsigned int> stoppedBeforeStarting;

	//Loops are music bricks, engines and ambience, so they use the music volume on top of their own
	float musicVolume = 0.5f;

	//EFX reverb and low-pass filter, all 0 if the OpenAL implementation doesn't have ALC_EXT_EFX
	ALuint effect = 0;
	ALuint effectSlot = 0;
	ALuint directFilter = 0;
	bool useEaxReverb = false;
	//Whether sources are sending to effectSlot, which then also gets each source's muffling filter, see applyDirectFilter
	bool reverbConnected = false;

	//Auto follows the listener's space and water (see setListenerSpace), Preset holds one Lua picked, Off has none
	enum ReverbMode
	{
		ReverbAuto,
		ReverbPreset,
		ReverbOff
	} reverbMode = ReverbAuto;

	//What the reverb slot has, in auto it glides toward what the space calls for
	EFXEAXREVERBPROPERTIES currentReverb;
	float sinceReverbAppliedMS = 0;

	//Settings, see setEnvironmentOptions
	bool environmentalReverb = true;
	bool occlusionOn = true;
	float occlusionIntervalMS = 150.0f;
	OcclusionTest occlusionTest = nullptr;

	float spaceDistance = 40.0f;
	float spaceEnclosure = 0.0f;
	bool underwater = false;
	//0-1, fades in and out instead of switching all at once
	float underwaterAmount = 0.0f;

	glm::vec3 listenerPosition = glm::vec3(0);
	Motion listenerMotion;
	//The listener's velocity this update, given or tracked
	glm::vec3 listenerMoving = glm::vec3(0);

	//Velocity to give a source for the Doppler effect: how fast it and the listener close in on each other, less a little so slow movement doesn't shift pitch
	glm::vec3 dopplerVelocity(const SoundLocation& where) const;

	//Sets the source up to play from where, including whether it's positioned at all
	void placeSource(ALuint source, const SoundLocation& where);

	//Velocity from how far position moved since the last call, smoothed, ignoring jumps too fast to be real movement
	static glm::vec3 trackVelocity(Motion& motion, const glm::vec3& position, float deltaT);

	//Routes every source through the reverb slot, or takes them all off it
	void connectReverb(bool on);

	//Reverb is on for a preset, and in auto when environmental reverb is turned on
	void updateReverbConnection();

	void applyReverb(const EFXEAXREVERBPROPERTIES& reverb);

	void stopLoopSource(Loop& loop, bool rememberOffset);

	//For a source that just started playing where: checks right away, so a sound behind a wall doesn't start out clear
	void startOcclusion(ALuint source, Occlusion& occlusion, const SoundLocation& where);

	//Rechecks every occlusionIntervalMS, glides toward the result, and sets the source's filter from it and underwaterAmount
	void updateOcclusion(ALuint source, Occlusion& occlusion, const SoundLocation& where, float deltaT);

	void applyDirectFilter(ALuint source, Occlusion& occlusion);

	//Takes buffers a voice source finished playing off it, into spareVoiceBuffers
	void recycleVoiceBuffers(ALuint source);

	//Rain: a flat looping source of its own outside the loop pool, see setRain
	static constexpr const char* rainSoundPath = "Assets/music/dragon-studio-calming-rain-loop-398653.mp3";
	ALuint rainSource = 0;
	//Decoded the first time it rains, 0 until then
	ALuint rainBuffer = 0;
	//Don't try to decode it again every frame
	bool rainLoadFailed = false;
	bool rainPlaying = false;
	float rainVolume = 0.0f;
	//Glides toward rainExposureTarget, see setRainExposure
	float rainExposure = 1.0f;
	float rainExposureTarget = 1.0f;
	float rainAppliedGain = -1.0f;
	float rainAppliedGainHF = -1.0f;

	//Starts, pauses, loads, and muffles the rain sound
	void updateRain(float deltaT);

public:

	bool isValid() const { return valid; }

	//Loads the file right away. A second packet for the same ID replaces it, unless it's the same sound again
	void addSoundType(int id, const std::string& name, const std::string& filePath, bool isMusic);

	//-1 if the server hasn't registered a sound with that name
	int findSound(const std::string& name) const;

	//Names of the sound types the server registered as music, in ID order, for the wrench dialog
	std::vector<std::string> getMusicNames() const
	{
		std::vector<std::string> names;
		for (const SoundType& sound : sounds)
		{
			if (sound.isMusic && !sound.name.empty())
				names.push_back(sound.name);
		}
		return names;
	}

	//Names of the sound types that aren't music, in ID order, for the wrench dialog's horn list
	std::vector<std::string> getEffectNames() const
	{
		std::vector<std::string> names;
		for (const SoundType& sound : sounds)
		{
			if (!sound.isMusic && !sound.name.empty())
				names.push_back(sound.name);
		}
		return names;
	}

	void playSound(int soundID, const SoundLocation& where, float pitch = 1.0f, float volume = 1.0f);

	//For sounds the client plays on its own, like brick clicks. Silent if the server didn't register one by that name
	void playSound(const std::string& name, const SoundLocation& where = SoundLocation::flat(), float pitch = 1.0f, float volume = 1.0f);

	//id is chosen by the server, and used to stop the loop later
	void startLoop(unsigned int id, int soundID, const SoundLocation& where, float pitch, float volume);
	void stopLoop(unsigned int id);

	//"auto" follows the listener's space, "none" turns reverb off, or a name from reverbPresetNames
	void setReverb(const std::string& preset);

	//audio/reverbquality and audio/occlusionquality: 0 off, 1-3 low to high (occlusion quality sets how often it's rechecked)
	void setEnvironmentOptions(int reverbQuality, int occlusionQuality);

	void setOcclusionTest(const OcclusionTest& test) { occlusionTest = test; }

	//Whether reverb currently follows setListenerSpace, so there's no need to measure the space otherwise
	bool wantsListenerSpace() const { return valid && effectSlot && reverbMode == ReverbAuto && environmentalReverb; }

	//From AcousticProbe::measure
	void setListenerSpace(float averageDistance, float enclosure);

	//Muffles everything and switches auto reverb toward the underwater preset
	void setUnderwater(bool isUnderwater) { underwater = isUnderwater; }

	//Both 0-1, master scales everything, music scales loops
	void setVolumes(float master, float music);

	//0-1, how hard it's raining, 0 pauses the rain sound. Uses the music volume like other ambience
	void setRain(float intensity) { rainVolume = std::clamp(intensity, 0.0f, 1.0f); }

	//0-1 from AcousticProbe::skyExposure, the rain sound gets quieter and more muffled the less open sky there is
	void setRainExposure(float exposure) { rainExposureTarget = std::clamp(exposure, 0.0f, 1.0f); }

	//Call every frame: moves the listener, follows moving sounds, updates reverb and muffling, and hands loop sources to the closest loops
	//listenerVelocity is for the Doppler effect, like that of the player the camera follows. Without one, it comes from how the listener moves
	void update(const glm::vec3& listenerPosition, const glm::vec3& listenerDirection, std::optional<glm::vec3> listenerVelocity, float deltaT);

	/*
		Voice chat: a source that plays samples as they're queued onto it, positioned, muffled, and echoing like any other sound
		Returns which voice source it got, -1 if they're all taken
	*/
	int openVoice(const SoundLocation& where);
	void moveVoice(int voice, const SoundLocation& where);
	//Adds mono samples after what the voice source has left to play, starting it again if it ran out
	void queueVoice(int voice, const int16_t* samples, int sampleCount, int sampleRate);
	//How many queueVoice calls' worth haven't finished playing yet, 0 once it's run out
	int queuedVoiceBuffers(int voice) const;
	void closeVoice(int voice);

	//audio/voicevolume, 0-1, on top of the master volume
	void setVoiceVolume(float volume);

	//Stops everything and forgets the server's sounds, loops, and reverb preset, for leaving a server
	void clear();

	AudioSystem();
	~AudioSystem();
};
