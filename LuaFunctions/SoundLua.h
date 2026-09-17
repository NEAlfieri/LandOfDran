#pragma once

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include "../Networking/Server.h"
#include "../GameLoop/ServerProgramData.h"

extern Server* LUA_server;
extern ServerProgramData* LUA_pd;

//Every registered sound type, sent to a client right after their connection is accepted
void sendSoundTypes(const ServerProgramData* pd, JoinedClient* client);

//Loops that are playing and the reverb preset, sent once a client finishes loading
void sendSoundState(const ServerProgramData* pd, JoinedClient* client);

//For sounds the server plays on its own, like splashes: plays a registered sound once for everyone, silent if there's none by that name
void playSoundAt(const std::string& name, const glm::vec3& position, float pitch, float volume);

//The ID of a sound type by name, -1 if there isn't one
int findSoundType(const std::string& name);

//Same, following a dynamic around while it plays
void playSoundOn(const std::string& name, const std::shared_ptr<Dynamic>& dynamic, float pitch, float volume);

//For music bricks: starts a loop of a registered sound at a position for everyone, false if there's no sound by that name
bool startSoundLoopAt(const std::string& name, const glm::vec3& position, float pitch, float volume, unsigned int& loopID);

//Stops a loop from startSoundLoop or startSoundLoopAt, nothing happens if it already ended
void stopSoundLoopByID(unsigned int loopID);

//Same as startSoundLoopAt, following a vehicle, and ending when it's removed
bool startSoundLoopOnVehicle(const std::string& name, const std::shared_ptr<Vehicle>& vehicle, float pitch, float volume, unsigned int& loopID);

bool isSoundLoopPlaying(unsigned int loopID);

bool soundTypeExists(const std::string& name);

//Whether there's a sound type by this name that was registered as music, which players can pick in the wrench dialog
bool isMusicSoundType(const std::string& name);

//For Blockland imports: the name of a music sound type matching a Blockland music name, ignoring case and treating underscores as spaces, "" if there's none
std::string findMusicByName(const std::string& name);

//dynamic: and client: methods, registered in getDynamicFunctions and registerClientFunctions
int LUA_dynamicPlaySound(lua_State* L);
int LUA_dynamicStartSoundLoop(lua_State* L);
int LUA_clientPlaySound(lua_State* L);
int LUA_clientSetAudioEffect(lua_State* L);

//newSoundType, playSound, startSoundLoop, stopSoundLoop, setAudioEffect, setVoiceRange, getVoiceRange
void registerSoundFunctions(lua_State* L);
