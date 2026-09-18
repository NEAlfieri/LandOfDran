#pragma once

#include "../LandOfDran.h"
#include "Material.h"
#include "ShaderSpecification.h"
#include "../Bricks/BrickHolder.h"

#include <deque>

//One mark left on the world, like a bullet hole, see Lua's addDecal
struct WorldDecal
{
	//Index into the decal types, the same on the server and its clients
	uint16_t typeID = 0;
	glm::vec3 position = glm::vec3(0);
	//Out of the surface it's on
	glm::vec3 normal = glm::vec3(0, 1, 0);
	//Studs along each side of its square
	float size = 1.0f;
	//Radians it's turned about its normal, so a row of holes doesn't look stamped
	float roll = 0.0f;
	//The brick it's on and goes away with, NO_ID for one that isn't on a brick
	netIDType brickID = NO_ID;

	//Bytes of one in a WorldDecal packet, see write
	static constexpr unsigned int recordBytes = sizeof(uint16_t) + sizeof(float) * 8 + sizeof(netIDType);

	void write(unsigned char* data) const;
	static WorldDecal read(const unsigned char* data);
};

/*
	Client only: every decal in the world, and the one buffer of flat squares they're drawn from
	The squares go through model.frag like everything else, so a decal's material gets the sun, shadows,
	point lights and rain the brick under it gets, and its normal map is what makes a hole look dented in
	There's no lifetime, the server says how many can exist at once and the oldest make room, see add
*/
class WorldDecals
{
	struct DecalType
	{
		std::string name = "";
		//nullptr for a material this client doesn't have, its decals just aren't drawn
		Material* material = nullptr;
	};

	//By the server's ID
	std::vector<DecalType> types;

	//Oldest first
	std::deque<WorldDecal> decals;

	//First vertex and vertex count in the buffer of each type's triangles, remade by update
	struct TypeDraw
	{
		GLint first = 0;
		GLsizei count = 0;
	};
	std::vector<TypeDraw> draws;

	GLuint vao = 0;
	GLuint vbo = 0;

	//Position, normal, tangent, bitangent, uv, color, which is what decal.vert reads
	static constexpr int vertexFloats = 18;

	//Appends one decal's triangles, nudged onto and cut to the face of its brick if it has one
	void appendVertices(std::vector<float>& vertices, const WorldDecal& decal, const Brick* brick) const;

	public:

	//A type the server told us about, the material is a descriptor like a model's, from the game's folder
	void setType(uint16_t id, const std::string& name, const std::string& materialPath, std::shared_ptr<TextureManager> textures);

	void add(const WorldDecal& decal) { decals.push_back(decal); }

	//Drops decals whose brick is gone
	void prune(const BrickHolder* bricks);

	//Drops the oldest until no more than maxDecals are left, the server does the same to its own, see ServerProgramData::decals
	void trim(unsigned int maxDecals);

	void clear();

	size_t size() const { return decals.size(); }

	//Once a frame: prunes, and remakes the buffer, since their bricks can be repainted under them
	void update(const BrickHolder* bricks);

	//After everything opaque, they blend over what's there and leave depth alone
	void render(std::shared_ptr<ShaderManager> shaders, const glm::mat4* lightSpaceMatricies, int shadowSoftness, bool coloredShadows) const;

	WorldDecals();
	~WorldDecals();
};
