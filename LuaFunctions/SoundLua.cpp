#include "SoundLua.h"
#include "ClientLua.h"
#include "../Audio/ReverbPresets.h"

template <typename T>
static void put(std::vector<unsigned char>& bytes, const T& value)
{
	size_t at = bytes.size();
	bytes.resize(at + sizeof(T));
	memcpy(bytes.data() + at, &value, sizeof(T));
}

//One length byte then up to 255 characters
static void putString(std::vector<unsigned char>& bytes, const std::string& text)
{
	size_t length = std::min(text.length(), (size_t)255);
	bytes.push_back((unsigned char)length);
	bytes.insert(bytes.end(), text.begin(), text.begin() + length);
}

//See readSoundLocation in OneShotSound.cpp
static void putLocation(std::vector<unsigned char>& bytes, SoundLocationKind kind, const glm::vec3& position, const std::shared_ptr<Dynamic>& dynamic, const std::shared_ptr<Vehicle>& vehicle = nullptr)
{
	bytes.push_back(kind);

	if (kind == SoundLocationFixed)
	{
		put(bytes, position.x);
		put(bytes, position.y);
		put(bytes, position.z);
	}
	else if (kind == SoundLocationDynamic)
		put(bytes, (netIDType)dynamic->getID());
	else if (kind == SoundLocationVehicle)
		put(bytes, (netIDType)vehicle->getID());
}

static ENetPacket* makePacket(const std::vector<unsigned char>& bytes, PacketChannel channel)
{
	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(channel));
}

static ENetPacket* makeSoundTypePacket(size_t id, const ServerProgramData::RegisteredSound& sound)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(AddSoundType);
	put(bytes, (uint16_t)id);
	bytes.push_back(sound.isMusic ? 1 : 0);
	putString(bytes, sound.name);
	putString(bytes, sound.filePath);
	return makePacket(bytes, JoinNegotiation);
}

//Sent unreliably, a lost click isn't worth resending late
static ENetPacket* makeOneShotPacket(int soundID, float pitch, float volume, SoundLocationKind kind, const glm::vec3& position, const std::shared_ptr<Dynamic>& dynamic, const std::shared_ptr<Vehicle>& vehicle = nullptr)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(OneShotSound);
	put(bytes, (uint16_t)soundID);
	put(bytes, pitch);
	put(bytes, volume);
	putLocation(bytes, kind, position, dynamic, vehicle);
	return makePacket(bytes, Unreliable);
}

static ENetPacket* makeLoopStartPacket(const ServerProgramData::ActiveSoundLoop& loop, const std::shared_ptr<Dynamic>& dynamic)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(SoundLoop);
	bytes.push_back(SoundLoopStart);
	put(bytes, loop.id);
	put(bytes, loop.soundID);
	put(bytes, loop.pitch);
	put(bytes, loop.volume);
	putLocation(bytes, loop.kind, loop.position, dynamic, loop.vehicle.lock());
	return makePacket(bytes, OtherReliable);
}

static ENetPacket* makeLoopStopPacket(unsigned int id)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(SoundLoop);
	bytes.push_back(SoundLoopStop);
	put(bytes, id);
	return makePacket(bytes, OtherReliable);
}

static ENetPacket* makeAudioEffectPacket(const std::string& preset)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(AudioEffect);
	putString(bytes, preset);
	return makePacket(bytes, OtherReliable);
}

void sendSoundTypes(const ServerProgramData* pd, JoinedClient* client)
{
	for (size_t a = 0; a < pd->soundTypes.size(); a++)
		client->send(makeSoundTypePacket(a, pd->soundTypes[a]), JoinNegotiation);
}

void sendSoundState(const ServerProgramData* pd, JoinedClient* client)
{
	for (const ServerProgramData::ActiveSoundLoop& loop : pd->soundLoops)
	{
		std::shared_ptr<Dynamic> dynamic = loop.dynamic.lock();
		if (loop.kind == SoundLocationDynamic && !dynamic)
			continue;
		if (loop.kind == SoundLocationVehicle && loop.vehicle.expired())
			continue;

		client->send(makeLoopStartPacket(loop, dynamic), OtherReliable);
	}

	//Clients start out on auto
	if (pd->reverbPreset != "auto")
		client->send(makeAudioEffectPacket(pd->reverbPreset), OtherReliable);
}

//-1 if there isn't one
int findSoundType(const std::string& name)
{
	for (size_t a = 0; a < LUA_pd->soundTypes.size(); a++)
		if (LUA_pd->soundTypes[a].name == name)
			return (int)a;

	return -1;
}

void playSoundAt(const std::string& name, const glm::vec3& position, float pitch, float volume)
{
	if (!LUA_pd || !LUA_server)
		return;

	int soundID = findSoundType(name);
	if (soundID == -1)
		return;

	pitch = std::clamp(pitch, 0.05f, 10.0f);
	volume = std::clamp(volume, 0.0f, 1.0f);
	LUA_server->broadcast(makeOneShotPacket(soundID, pitch, volume, SoundLocationFixed, position, nullptr), Unreliable);
}

void playSoundOn(const std::string& name, const std::shared_ptr<Dynamic>& dynamic, float pitch, float volume)
{
	if (!LUA_pd || !LUA_server || !dynamic)
		return;

	int soundID = findSoundType(name);
	if (soundID == -1)
		return;

	pitch = std::clamp(pitch, 0.05f, 10.0f);
	volume = std::clamp(volume, 0.0f, 1.0f);
	LUA_server->broadcast(makeOneShotPacket(soundID, pitch, volume, SoundLocationDynamic, glm::vec3(0), dynamic), Unreliable);
}

void playSoundOnVehicle(const std::string& name, const std::shared_ptr<Vehicle>& vehicle, float pitch, float volume)
{
	if (!LUA_pd || !LUA_server || !vehicle)
		return;

	int soundID = findSoundType(name);
	if (soundID == -1)
		return;

	pitch = std::clamp(pitch, 0.05f, 10.0f);
	volume = std::clamp(volume, 0.0f, 1.0f);
	LUA_server->broadcast(makeOneShotPacket(soundID, pitch, volume, SoundLocationVehicle, glm::vec3(0), nullptr, vehicle), Unreliable);
}

bool soundTypeExists(const std::string& name)
{
	return LUA_pd && findSoundType(name) != -1;
}

bool isMusicSoundType(const std::string& name)
{
	int soundID = LUA_pd ? findSoundType(name) : -1;
	return soundID != -1 && LUA_pd->soundTypes[soundID].isMusic;
}

std::string findMusicByName(const std::string& name)
{
	if (!LUA_pd)
		return "";

	//Blockland's music names are its file names with spaces for underscores
	auto simplify = [](const std::string& text)
	{
		std::string simple = lowercase(text);
		std::replace(simple.begin(), simple.end(), '_', ' ');
		return simple;
	};

	std::string wanted = simplify(name);
	if (wanted.empty())
		return "";

	for (const ServerProgramData::RegisteredSound& sound : LUA_pd->soundTypes)
	{
		if (sound.isMusic && simplify(sound.name) == wanted)
			return sound.name;
	}

	return "";
}

//Loops on a Dynamic end when it's destroyed, clients stop them on their own
static void forgetEndedSoundLoops()
{
	std::erase_if(LUA_pd->soundLoops, [](const ServerProgramData::ActiveSoundLoop& loop)
	{
		return (loop.kind == SoundLocationDynamic && loop.dynamic.expired()) || (loop.kind == SoundLocationVehicle && loop.vehicle.expired());
	});
}

struct SoundArgs
{
	int soundID = -1;
	bool positioned = false;
	glm::vec3 position = glm::vec3(0);
	float pitch = 1.0f;
	float volume = 1.0f;
};

/*
	Reads name[, x, y, z][, pitch, volume] from stack index first up to the top of the stack
	Without allowPosition only name[, pitch, volume] fits. Logs usage and returns false for anything else
*/
static bool readSoundArgs(lua_State* L, int first, bool allowPosition, const std::string& usage, SoundArgs& args)
{
	int top = lua_gettop(L);
	int numbers = top - first;

	bool fits = top >= first && lua_type(L, first) == LUA_TSTRING &&
		(numbers == 0 || numbers == 2 || (allowPosition && (numbers == 3 || numbers == 5)));
	for (int a = first + 1; fits && a <= top; a++)
		fits = lua_isnumber(L, a);

	if (!fits)
	{
		error("Expected " + usage);
		return false;
	}

	std::string name = lua_tostring(L, first);
	args.soundID = findSoundType(name);
	if (args.soundID == -1)
	{
		error("No sound type named " + name + ", see newSoundType");
		return false;
	}

	int next = first + 1;
	if (numbers == 3 || numbers == 5)
	{
		args.positioned = true;
		args.position = glm::vec3(lua_tonumber(L, next), lua_tonumber(L, next + 1), lua_tonumber(L, next + 2));
		next += 3;
	}

	if (next < top)
	{
		args.pitch = std::clamp((float)lua_tonumber(L, next), 0.05f, 10.0f);
		args.volume = std::clamp((float)lua_tonumber(L, next + 1), 0.0f, 1.0f);
	}

	return true;
}

static int LUA_newSoundType(lua_State* L)
{
	scope("(LUA) newSoundType");

	int args = lua_gettop(L);
	if ((args != 2 && args != 3) || lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING)
	{
		error("Expected newSoundType(name, filePath[, isMusic])");
		lua_settop(L, 0);
		return 0;
	}

	ServerProgramData::RegisteredSound sound;
	sound.name = lua_tostring(L, 1);
	sound.filePath = lua_tostring(L, 2);
	sound.isMusic = args == 3 && lua_toboolean(L, 3);
	lua_settop(L, 0);

	if (sound.name.length() < 1 || sound.name.length() > 255 || sound.filePath.length() < 1 || sound.filePath.length() > 255)
	{
		error("Sound names and file paths have to be 1-255 characters");
		return 0;
	}

	if (findSoundType(sound.name) != -1)
	{
		error("There's already a sound type named " + sound.name);
		return 0;
	}

	if (LUA_pd->soundTypes.size() >= 65535)
	{
		error("Too many sound types, can't add " + sound.name);
		return 0;
	}

	if (!std::filesystem::exists(sound.filePath))
	{
		error("Sound file " + sound.filePath + " doesn't exist, not adding sound type " + sound.name);
		return 0;
	}

	LUA_pd->soundTypes.push_back(sound);

	//Anyone already here, anyone who joins later gets it in sendSoundTypes
	LUA_server->broadcast(makeSoundTypePacket(LUA_pd->soundTypes.size() - 1, sound), JoinNegotiation);

	return 0;
}

static int LUA_playSound(lua_State* L)
{
	scope("(LUA) playSound");

	SoundArgs args;
	bool valid = readSoundArgs(L, 1, true, "playSound(name[, x, y, z][, pitch, volume])", args);
	lua_settop(L, 0);
	if (!valid)
		return 0;

	SoundLocationKind kind = args.positioned ? SoundLocationFixed : SoundLocationFlat;
	LUA_server->broadcast(makeOneShotPacket(args.soundID, args.pitch, args.volume, kind, args.position, nullptr), Unreliable);

	return 0;
}

int LUA_clientPlaySound(lua_State* L)
{
	scope("(LUA) client:playSound");

	SoundArgs args;
	if (lua_gettop(L) < 1 || !readSoundArgs(L, 2, true, "client:playSound(name[, x, y, z][, pitch, volume])", args))
	{
		lua_settop(L, 0);
		return 0;
	}

	lua_settop(L, 1);
	std::shared_ptr<JoinedClient> client = popClientLua(L);
	if (!client)
		return 0;

	SoundLocationKind kind = args.positioned ? SoundLocationFixed : SoundLocationFlat;
	client->send(makeOneShotPacket(args.soundID, args.pitch, args.volume, kind, args.position, nullptr), Unreliable);

	return 0;
}

int LUA_dynamicPlaySound(lua_State* L)
{
	scope("(LUA) dynamic:playSound");

	SoundArgs args;
	if (lua_gettop(L) < 1 || !readSoundArgs(L, 2, false, "dynamic:playSound(name[, pitch, volume])", args))
	{
		lua_settop(L, 0);
		return 0;
	}

	lua_settop(L, 1);
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	LUA_server->broadcast(makeOneShotPacket(args.soundID, args.pitch, args.volume, SoundLocationDynamic, glm::vec3(0), dynamic), Unreliable);

	return 0;
}

//Shared by startSoundLoop and dynamic:startSoundLoop, pushes the new loop's ID
static void startSoundLoop(lua_State* L, const SoundArgs& args, SoundLocationKind kind, const std::shared_ptr<Dynamic>& dynamic)
{
	forgetEndedSoundLoops();

	ServerProgramData::ActiveSoundLoop loop;
	loop.id = LUA_pd->nextSoundLoopID++;
	loop.soundID = (uint16_t)args.soundID;
	loop.kind = kind;
	loop.position = args.position;
	loop.dynamic = dynamic;
	loop.pitch = args.pitch;
	loop.volume = args.volume;
	LUA_pd->soundLoops.push_back(loop);

	LUA_server->broadcast(makeLoopStartPacket(loop, dynamic), OtherReliable);

	lua_pushinteger(L, loop.id);
}

bool startSoundLoopAt(const std::string& name, const glm::vec3& position, float pitch, float volume, unsigned int& loopID)
{
	if (!LUA_pd || !LUA_server)
		return false;

	int soundID = findSoundType(name);
	if (soundID == -1)
		return false;

	forgetEndedSoundLoops();

	ServerProgramData::ActiveSoundLoop loop;
	loop.id = LUA_pd->nextSoundLoopID++;
	loop.soundID = (uint16_t)soundID;
	loop.kind = SoundLocationFixed;
	loop.position = position;
	loop.pitch = std::clamp(pitch, 0.05f, 10.0f);
	loop.volume = std::clamp(volume, 0.0f, 1.0f);
	LUA_pd->soundLoops.push_back(loop);

	LUA_server->broadcast(makeLoopStartPacket(loop, nullptr), OtherReliable);

	loopID = loop.id;
	return true;
}

void stopSoundLoopByID(unsigned int loopID)
{
	if (!LUA_pd || !LUA_server)
		return;

	//Nothing to do if it already ended with its Dynamic
	auto loop = std::find_if(LUA_pd->soundLoops.begin(), LUA_pd->soundLoops.end(), [loopID](const ServerProgramData::ActiveSoundLoop& loop) { return loop.id == loopID; });
	if (loop == LUA_pd->soundLoops.end())
		return;

	LUA_pd->soundLoops.erase(loop);
	LUA_server->broadcast(makeLoopStopPacket(loopID), OtherReliable);
}

bool startSoundLoopOnVehicle(const std::string& name, const std::shared_ptr<Vehicle>& vehicle, float pitch, float volume, unsigned int& loopID)
{
	if (!LUA_pd || !LUA_server || !vehicle)
		return false;

	int soundID = findSoundType(name);
	if (soundID == -1)
		return false;

	forgetEndedSoundLoops();

	ServerProgramData::ActiveSoundLoop loop;
	loop.id = LUA_pd->nextSoundLoopID++;
	loop.soundID = (uint16_t)soundID;
	loop.kind = SoundLocationVehicle;
	loop.vehicle = vehicle;
	loop.pitch = std::clamp(pitch, 0.05f, 10.0f);
	loop.volume = std::clamp(volume, 0.0f, 1.0f);
	LUA_pd->soundLoops.push_back(loop);

	LUA_server->broadcast(makeLoopStartPacket(loop, nullptr), OtherReliable);

	loopID = loop.id;
	return true;
}

bool isSoundLoopPlaying(unsigned int loopID)
{
	if (!LUA_pd)
		return false;

	forgetEndedSoundLoops();
	return std::any_of(LUA_pd->soundLoops.begin(), LUA_pd->soundLoops.end(), [loopID](const ServerProgramData::ActiveSoundLoop& loop) { return loop.id == loopID; });
}

static int LUA_startSoundLoop(lua_State* L)
{
	scope("(LUA) startSoundLoop");

	SoundArgs args;
	bool valid = readSoundArgs(L, 1, true, "startSoundLoop(name[, x, y, z][, pitch, volume])", args);
	lua_settop(L, 0);
	if (!valid)
		return 0;

	startSoundLoop(L, args, args.positioned ? SoundLocationFixed : SoundLocationFlat, nullptr);
	return 1;
}

int LUA_dynamicStartSoundLoop(lua_State* L)
{
	scope("(LUA) dynamic:startSoundLoop");

	SoundArgs args;
	if (lua_gettop(L) < 1 || !readSoundArgs(L, 2, false, "dynamic:startSoundLoop(name[, pitch, volume])", args))
	{
		lua_settop(L, 0);
		return 0;
	}

	lua_settop(L, 1);
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid dynamic object passed, was it deleted already?");
		return 0;
	}

	startSoundLoop(L, args, SoundLocationDynamic, dynamic);
	return 1;
}

static int LUA_stopSoundLoop(lua_State* L)
{
	scope("(LUA) stopSoundLoop");

	if (lua_gettop(L) != 1 || !lua_isinteger(L, 1))
	{
		error("Expected stopSoundLoop(loopID)");
		lua_settop(L, 0);
		return 0;
	}

	unsigned int id = (unsigned int)lua_tointeger(L, 1);
	lua_settop(L, 0);

	stopSoundLoopByID(id);

	return 0;
}

//Validates and lower cases the preset at stack index, empty string if it isn't one
static std::string readReverbPreset(lua_State* L, int index, const std::string& usage)
{
	if (lua_type(L, index) != LUA_TSTRING)
	{
		error("Expected " + usage);
		return "";
	}

	std::string preset = lua_tostring(L, index);
	if (!isReverbPreset(preset))
	{
		error("Unknown audio effect " + preset + ", see LuaAPI.md for the list");
		return "";
	}

	if (isAutoReverb(preset))
		return "auto";
	return isNoReverb(preset) ? "none" : normalizeReverbPreset(preset);
}

static int LUA_setAudioEffect(lua_State* L)
{
	scope("(LUA) setAudioEffect");

	std::string preset = lua_gettop(L) == 1 ? readReverbPreset(L, 1, "setAudioEffect(preset)") : "";
	if (lua_gettop(L) != 1)
		error("Expected setAudioEffect(preset)");
	lua_settop(L, 0);

	if (preset.empty())
		return 0;

	LUA_pd->reverbPreset = preset;
	LUA_server->broadcast(makeAudioEffectPacket(preset), OtherReliable);

	return 0;
}

int LUA_clientSetAudioEffect(lua_State* L)
{
	scope("(LUA) client:setAudioEffect");

	std::string preset = lua_gettop(L) == 2 ? readReverbPreset(L, 2, "client:setAudioEffect(preset)") : "";
	if (lua_gettop(L) != 2)
		error("Expected client:setAudioEffect(preset)");

	if (preset.empty())
	{
		lua_settop(L, 0);
		return 0;
	}

	lua_settop(L, 1);
	std::shared_ptr<JoinedClient> client = popClientLua(L);
	if (!client)
		return 0;

	client->send(makeAudioEffectPacket(preset), OtherReliable);

	return 0;
}

static int LUA_setVoiceRange(lua_State* L)
{
	scope("(LUA) setVoiceRange");

	if (lua_gettop(L) != 1 || !lua_isnumber(L, 1))
	{
		error("Expected setVoiceRange(studs)");
		lua_settop(L, 0);
		return 0;
	}

	LUA_pd->voiceRange = std::max((float)lua_tonumber(L, 1), 0.0f);
	lua_settop(L, 0);

	return 0;
}

static int LUA_getVoiceRange(lua_State* L)
{
	lua_settop(L, 0);
	lua_pushnumber(L, LUA_pd->voiceRange);
	return 1;
}

void registerSoundFunctions(lua_State* L)
{
	lua_register(L, "newSoundType", LUA_newSoundType);
	lua_register(L, "playSound", LUA_playSound);
	lua_register(L, "startSoundLoop", LUA_startSoundLoop);
	lua_register(L, "stopSoundLoop", LUA_stopSoundLoop);
	lua_register(L, "setAudioEffect", LUA_setAudioEffect);
	lua_register(L, "setVoiceRange", LUA_setVoiceRange);
	lua_register(L, "getVoiceRange", LUA_getVoiceRange);
}
