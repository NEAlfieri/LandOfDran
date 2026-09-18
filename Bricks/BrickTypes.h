#pragma once

#include "../LandOfDran.h"

#include <unordered_set>

class btCollisionShape;

//A named basic brick size, only needed because Blockland saves store bricks by name rather than size
struct BasicBrickType
{
	//As written in types.txt and .bls saves, e.g. "2x4" or "1x1F"
	std::string uiName = "";

	unsigned char width = 1;
	unsigned char height = 1;
	unsigned char length = 1;

	//Icon image next to the type's .blb, "" if there isn't one
	std::string iconPath = "";
};

//Which brick material a special brick face uses, from the TEX: line of each quad in its .blb
enum BrickFaceTexture
{
	BrickTextureTop = 0,		//TEX:TOP, studs
	BrickTextureBottom = 1,		//TEX:BOTTOMLOOP and TEX:BOTTOMEDGE
	BrickTextureSide = 2,		//TEX:SIDE
	BrickTextureRamp = 3,		//TEX:RAMP
	BrickTexturePrint = 4,		//TEX:PRINT, drawn plain since prints aren't supported yet
	BrickTextureCount = 5
};

//Position, normal, tangent, bitangent, uv, vertex color
constexpr int specialVertexFloats = 18;

//What a special brick becomes when bricks around it are sliced into a vehicle, from vehiclePart in its datablock, see SimObjects/Vehicle.h
enum VehiclePart : unsigned char
{
	VehiclePart_None = 0,		//Just part of the body
	VehiclePart_Wheel = 1,		//Taken out and replaced by a wheel that rolls along its long side
	VehiclePart_Steering = 2,	//Each vehicle needs exactly one, the driver stands behind it and it drives the way it faces
	VehiclePart_Seat = 3		//Part of the body, a passenger stands on it while someone else drives
};

/*
	A brick with its own shape from a Blockland .blb file, like a ramp
	Occupies its width x height x length on the grid like a basic brick, and turns in quarter turns around its middle
*/
struct SpecialBrickType
{
	//As written in .bls saves and shown in the brick selector, UTF-8
	std::string uiName = "";

	//From the datablock, for grouping in the brick selector
	std::string category = "";
	std::string subCategory = "";

	//Like Blockland, a datablock without a category (e.g. an opened door) can be loaded from saves but isn't in the brick selector
	bool listed = true;

	std::string blbPath = "";
	//"" if there isn't one
	std::string iconPath = "";

	//Studs, plates, studs before rotation, like Brick
	unsigned char width = 1;
	unsigned char height = 1;
	unsigned char length = 1;

	//Some faces have see-through vertex colors, so the whole brick is drawn with transparent bricks
	bool hasTransparency = false;

	//Whether it's a wheel or steering wheel for vehicles
	VehiclePart vehiclePart = VehiclePart_None;

	//Our own vehicleSpawn datablock field: its wrench dialog picks a vehicle that's kept spawned above it, see BrickAttachments::vehicleSpawnName
	bool vehicleSpawn = false;

	/*
		Triangles in world units, centered on the brick's middle, specialVertexFloats each
		Sorted by BrickFaceTexture, group n is groupCount[n] vertices starting at groupFirst[n]
		A vertex color alpha of 0 means the face takes the brick's color
	*/
	std::vector<float> vertices;
	int groupFirst[BrickTextureCount] = {};
	int groupCount[BrickTextureCount] = {};

	//The .blb's collision boxes, or a convex hull of its shape if it lists none, centered like vertices
	btCollisionShape* shape = nullptr;
	std::vector<btCollisionShape*> childShapes;

	int vertexCount() const { return (int)(vertices.size() / specialVertexFloats); }

	SpecialBrickType() = default;
	SpecialBrickType(const SpecialBrickType&) = delete;
	~SpecialBrickType();
};

//Blockland's files are Latin-1 (e.g. the degree sign in "45° Ramp 2x"), anything that isn't already valid UTF-8 is converted from it
std::string blocklandTextToUtf8(const std::string& text);

/*
	Brick types from a folder like Assets/brick/types:
	fxDTSBrickData datablocks in bricks.txt files (Blockland add-on syntax, just brickFile, uiName, iconName, category, subCategory)
	name .blb files and their icons,
	test.cs maps more names to basic .blb files, and any other .blb goes by its file name
	Each .blb's first two lines give its size and whether it's a basic box or a special brick
*/
class BrickTypes
{
	std::vector<BasicBrickType> basicTypes;

	//Lowercase name to index in basicTypes
	std::unordered_map<std::string, size_t> basicByName;

	//Sorted by category, then sub category, then name
	std::vector<std::unique_ptr<SpecialBrickType>> specialTypes;

	//Lowercase ui name, .blb file name, and .blb file name without extension to index in specialTypes
	std::unordered_map<std::string, size_t> specialByName;

	public:

	void load(const std::string& typesFolder);

	/*
		Size of a basic brick by name, case-insensitive. Names without a .blb fall back to Blockland's naming:
		"2x4" is one brick (3 plates) tall, "1x2F" is one plate tall, "1x1x5" is 5 bricks tall
		Returns false for special and unknown bricks
	*/
	bool getBasicSize(const std::string& name, int& width, int& height, int& length) const;

	const std::vector<BasicBrickType>& getBasicTypes() const { return basicTypes; }

	//Index of a special type by ui name or .blb file name (with or without extension), case-insensitive, -1 if there isn't one
	int findSpecial(const std::string& name) const;

	size_t getSpecialCount() const { return specialTypes.size(); }

	//nullptr if index is out of range
	const SpecialBrickType* getSpecial(int index) const;
};
