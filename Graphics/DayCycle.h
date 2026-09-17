#pragma once

#include "../LandOfDran.h"

#include <cctype>

//Sky, fog, and light colors for one part of the day
//Sky and fog colors are final screen colors, light and ambient colors are HDR (tone mapped in model.frag)
struct SkyKeyframe
{
	glm::vec3 skyColor;
	glm::vec3 fogColor;
	glm::vec3 lightColor;
	glm::vec3 ambientColor;
};

//Index into DayCycle::phases
enum DayPhase
{
	NightPhase = 0,
	DawnPhase = 1,
	DaytimePhase = 2,
	DuskPhase = 3
};

//Names Lua uses for each DayPhase, in order
static constexpr const char* dayPhaseNames[4] = { "night", "dawn", "day", "dusk" };

//A DayPhase from its name, ignoring case, or -1 if there isn't one by that name
inline int dayPhaseFromName(std::string name)
{
	for (char& c : name)
		c = (char)std::tolower((unsigned char)c);

	for (int a = 0; a < 4; a++)
	{
		if (name == dayPhaseNames[a])
			return a;
	}

	return -1;
}

/*
	The look of the day/night cycle, which server Lua can change (see setSkyColor, setAmbientColor, and friends in LuaFunctions/OtherFunctions.cpp)
	Sent to clients with the rest of the world state, see WorldStateUpdatePacket
*/
struct DayCycle
{
	SkyKeyframe phases[4];

	//Fog starts at fogStart and fully hides everything past fogEnd
	float fogStart = 150;
	float fogEnd = 290;

	/*
		World height the fog fills up to, which only matters for how far up a skybox it reaches: a sky ray
		leaves the fog once it climbs past this height, and how far it traveled to get there fogs it the same
		way distance fogs the world. 0 leaves the sky clear, tall enough hides a whole skybox behind the fog
	*/
	float fogHeight = 40;

	//The camera sees 1000 units, and grass, water, and shadows all stretch out to wherever fog ends
	static constexpr float maxFogEnd = 900;

	//Past this the whole sky is fog anyway, since a ray that high up has gone further than any fog distance
	static constexpr float maxFogHeight = 2000;

	//Floats in phases plus the fog distances and height, as sent over the network
	static constexpr int networkFloats = 4 * 4 * 3 + 3;

	DayCycle()
	{
		//Night's light color is the moon. Day's ambient matches the ambient model.frag had before the day/night cycle
		phases[NightPhase] =	{ glm::vec3(0.01, 0.015, 0.04),	glm::vec3(0.03, 0.04, 0.08),	glm::vec3(0.5, 0.6, 1.0) * 0.35f,	glm::vec3(0.015, 0.018, 0.03) };
		phases[DawnPhase] =		{ glm::vec3(0.25, 0.3, 0.5),	glm::vec3(0.95, 0.6, 0.35),		glm::vec3(1.0, 0.55, 0.25) * 6.0f,	glm::vec3(0.25, 0.2, 0.22) };
		phases[DaytimePhase] =	{ glm::vec3(0.25, 0.45, 0.85),	glm::vec3(0.65, 0.78, 0.92),	glm::vec3(1.0, 0.7, 0.5) * 15.0f,	glm::vec3(1.0, 0.7, 0.5) * 0.45f };
		phases[DuskPhase] =		{ glm::vec3(0.22, 0.22, 0.45),	glm::vec3(0.9, 0.45, 0.25),		glm::vec3(1.0, 0.45, 0.2) * 6.0f,	glm::vec3(0.24, 0.17, 0.18) };
	}
};
