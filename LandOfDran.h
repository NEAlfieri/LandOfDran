// LandOfDran.h : Mostly external includes that can be included anywhere and not mess with anything

#include "enet/enet.h"
#include <ctype.h>
#include <iostream>
#include <SDL2/SDL.h>
#define GLM_ENABLE_EXPERIMENTAL //Was only required to get windows-mingw64 build to compile
#include <GL/glew.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <time.h>
#include <string>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <string_view>
#include <complex>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <map>
#include <stack>
#include <memory>
#include "Utility/ExecutableArguments.h"
#include "Utility/StringFunctions.h"
#include "Utility/Logger.h"
#include "Utility/FileFunctions.h"
#include "Utility/SettingManager.h"
#include "Networking/PacketEnums.h"
#include "Graphics/GraphicsHelpers.h"
#include <chrono>

//Used in the titlebar for the window, and for making sure client and server version match in mutliplayer
#define GAME_VERSION 50

//Default port for land of dran
#define DEFAULT_PORT 8765

//How loud a looping sound can be set, the music on a brick or vehicle above all, where 1 is the file as it was recorded
//A loop fades with distance like anything else, so the extra is for music meant to fill more than the room it's in
#define MAX_LOOP_VOLUME 2.0f

//Target time between dedicated server ticks, in milliseconds. Matches ObjHolder's snapshot broadcast throttle so the server doesn't do physics work it won't send out yet
#define SERVER_TICK_MS 25.0

//In-game seconds from one midnight to the next, at a Lua setTimeScale of 1
#define DAY_LENGTH_SECONDS 1000.0
