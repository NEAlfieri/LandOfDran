#pragma once

#include "../LandOfDran.h"

struct aiScene;

/*
	One of the animations a DTS shape came with, as a slice of the single animation track
	loadDtsScene lays them all out on, see the note about slices on struct Animation
*/
struct DtsSequence
{
	std::string name = "";
	//Where the sequence sits on the one track every sequence was put on, in frames, like an addAnimation line
	float startTime = 0;
	float endTime = 0;
	//Frames per millisecond that plays it back at the speed it was exported to run at
	float defaultSpeed = 0.04f;
	//Whether the sequence was exported as a looping one
	bool cyclic = false;
};

/*
	Reads a Torque DTS shape, the format Blockland add-ons ship their models in, and builds an
	aiScene out of it by hand so that everything downstream in Model can treat a .dts exactly
	like something Assimp imported from an FBX.

	Only version 24 shapes are read, which is what Blockland's exporter writes.

	DTS is Z-up while our models are Y-up, so the scene gets a root node that rotates the whole
	shape into our space. Only the most detailed detail level is loaded, its meshes named after
	the objects that hold them, and a mesh whose triangles use several materials is split into
	one aiMesh per material. Materials become one aiMaterial each with the texture sitting next
	to the .dts under that name as their diffuse map.

	Any sequences the shape has are put end to end on one animation track, since that is the one
	animation Model expects, and each one's slice of it is appended to sequences.

	Returns nullptr and logs why if the file can't be read. The caller owns the scene returned.
*/
aiScene * loadDtsScene(const std::string &filePath, std::vector<DtsSequence> * sequences = nullptr);

//Whether a path points at a DTS shape, which we read ourselves rather than handing to Assimp
bool isDtsPath(const std::string &filePath);
