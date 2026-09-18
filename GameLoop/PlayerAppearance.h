#pragma once

#include "../LandOfDran.h"

/*
	How a player wants their player model to look, picked in the appearance editor (see Interface/AppearanceEditor.h)
	Their game sends it to the server as they connect (see Networking/PacketsFromClient/AppearanceChoice.cpp)
	and the server keeps it until Lua puts it on a player with client:applyAppearance
*/
struct PlayerAppearance
{
	//File name of an image in Assets/faces, empty for no face
	std::string face = "";

	//File name of an image in Assets/shirts, empty for no shirt
	std::string shirt = "";

	//Mesh names, lower case like settings keep them, and the color each part is painted
	std::vector<std::pair<std::string, glm::vec3>> colors;

	//File name of a hat descriptor in partsFolder, empty for no hat, and the color it's painted, alpha 0 for its own look
	std::string hat = "";
	glm::vec4 hatColor = glm::vec4(0);

	//How big the hat is compared to its descriptor's size, from the editor's slider, between minHatScale and maxHatScale
	float hatScale = 1.0f;
	static constexpr float minHatScale = 0.5f;
	static constexpr float maxHatScale = 1.5f;

	//Where hats live: model descriptors with attach lines, worn on the player model's Head, see Model::attachMesh
	static constexpr const char* partsFolder = "Assets/brickhead/parts/";

	//The slot a hat goes in on a dynamic, see Dynamic::setPart
	static constexpr const char* hatSlot = "hat";

	//Longest face, shirt, or mesh name sent either way
	static constexpr unsigned int maxNameLength = 64;

	//Most painted parts one appearance sends
	static constexpr unsigned int maxColors = 64;
};
