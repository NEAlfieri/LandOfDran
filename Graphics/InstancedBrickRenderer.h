#pragma once

#include "../LandOfDran.h"
#include "../Bricks/Brick.h"
#include "../Bricks/BrickTypes.h"
#include "Material.h"
#include "ShaderSpecification.h"

/*
	Draws every basic brick as one instance of a shared cube, and every special brick as an instance of its type's shape,
	grouped into chunks so edits only re-upload one chunk and chunks outside the view are skipped
	Each face keeps its own texture mapping, so neighbouring bricks show seams like the old game
*/
class InstancedBrickRenderer
{
	//Consecutive special bricks of one type in a chunk's special instance buffer
	struct SpecialRun
	{
		int type;
		GLsizei first;
		GLsizei count;
	};

	struct Chunk
	{
		int64_t key = 0;
		std::vector<Brick*> bricks;
		bool dirty = false;

		//Where this chunk's bounds sit in chunkList, or -1 for a brick group's chunk, which isn't in it
		int listIndex = -1;

		//World space bounds of every brick in the chunk, as of the last rebuild, or the bounds in its own space for a brick group
		glm::vec3 min = glm::vec3(0);
		glm::vec3 max = glm::vec3(0);

		//Index 0 is opaque bricks, 1 is transparent bricks
		GLuint vao[2] = { 0, 0 };
		GLuint instanceBuffer[2] = { 0, 0 };
		GLsizei count[2] = { 0, 0 };

		//Special bricks, sorted by type, same indexing
		GLuint specialVao[2] = { 0, 0 };
		GLuint specialInstanceBuffer[2] = { 0, 0 };
		std::vector<SpecialRun> specialRuns[2];
	};

	//Bricks drawn together with their own transform, see addBrickGroup, its chunk points into bricks
	struct BrickGroup
	{
		Chunk* chunk = nullptr;
		std::vector<Brick> bricks;
	};

	//A VAO and instance buffer to draw drawInstances with
	struct InstanceSet
	{
		GLuint vao;
		GLsizei count;
	};

	//Special bricks to draw with drawSpecial
	struct SpecialSet
	{
		GLuint vao;
		GLuint instanceBuffer;
		const std::vector<SpecialRun>* runs;
	};

	//One entry per chunk, laid out flat so culling reads memory in order
	struct ChunkBounds
	{
		glm::vec3 min = glm::vec3(0);
		glm::vec3 max = glm::vec3(0);
		Chunk* chunk = nullptr;
	};

	std::unordered_map<int64_t, Chunk*> chunks;

	/*
		The same chunks as the map above, for the passes that walk every one of them. A big build is thousands
		of chunks and each is looked at by three shadow cascades, the camera, the rain map and every point light
		cube face, every frame. Walking the map for that chases a pointer per chunk and misses the cache almost
		every time; this is one contiguous array of just the bounds the frustum test needs
	*/
	std::vector<ChunkBounds> chunkList;

	std::vector<Chunk*> dirtyChunks;

	std::unordered_map<int, BrickGroup*> groups;
	int nextGroup = 0;

	//See getGeneration
	unsigned int generation = 0;

	//Counted by the shadow passes, read and cleared once a frame, see getShadowStats
	mutable int shadowChunksDrawn = 0;
	mutable int shadowChunksTested = 0;

	GLuint cubeBuffer = 0;

	//One instance, for the ghost brick and loose bricks
	GLuint singleVao = 0;
	GLuint singleInstanceBuffer = 0;
	GLuint singleSpecialVao = 0;
	GLuint singleSpecialInstanceBuffer = 0;

	//Non-owning
	const BrickTypes* types = nullptr;

	//Every special type's shape one after another, see SpecialBrickType::vertices
	GLuint specialMeshBuffer = 0;
	//Vertex each special type starts at in specialMeshBuffer
	std::vector<GLint> specialTypeOffsets;

	Material* topMaterial = nullptr;
	Material* bottomMaterial = nullptr;
	Material* sideMaterial = nullptr;
	Material* rampMaterial = nullptr;
	//Print faces are drawn plain until prints are supported
	Material* printMaterial = nullptr;

	GLint tileByStudsUniform = -1;
	GLint brickTransformUniform = -1;
	GLint glowUniform = -1;

	//specialMesh in brick.vert and brickShadowCascade.vert, for each program that uses them
	GLint specialMeshUniform = -1;
	//The same two in the depth pre-pass program, which shares brick.vert, see renderDepth
	GLint brickDepthSpecialMeshUniform = -1;
	GLint brickDepthTransformUniform = -1;
	GLint shadowSpecialMeshUniform = -1;
	GLint tintSpecialMeshUniform = -1;

	//brickTransform and skipPoint in brickShadowCascade.vert, for each program that uses it
	GLint shadowTransformUniform = -1;
	GLint tintTransformUniform = -1;
	GLint shadowSkipPointUniform = -1;
	GLint tintSkipPointUniform = -1;

	void createInstancedVao(GLuint& vao, GLuint& instanceBuffer) const;
	void createSpecialVao(GLuint& vao, GLuint& instanceBuffer) const;

	//Visible opaque or transparent chunks, nearest first for opaque and farthest first for transparent
	void visibleChunks(const std::shared_ptr<ShaderManager>& shaders, bool transparent, std::vector<const Chunk*>& out) const;

	Chunk* getChunk(const Brick* brick);
	void markDirty(Chunk* chunk);
	void rebuild(Chunk* chunk);
	//Uploads a chunk's bricks and works out its bounds
	void upload(Chunk* chunk);
	void destroyChunk(Chunk* chunk);

	//Keeps chunkList in step with chunks
	void addToList(Chunk* chunk);
	void removeFromList(Chunk* chunk);

	//nullptr for basic bricks, and for special types this client never loaded
	const SpecialBrickType* specialType(const Brick& brick) const;

	void setTransform(const glm::mat4& transform) const;
	void uploadSingleInstance(const glm::vec3& corner, const Brick& brick, float alpha) const;
	void uploadSingleSpecialInstance(const glm::vec3& corner, const Brick& brick, float alpha) const;

	//Expects the run's instance buffer to be bound, OpenGL 3.3 has no base instance so the attributes start at the run's first instance instead
	void pointSpecialInstances(GLsizei firstInstance) const;

	//Draws each face group with its material, for every instance set, calling beforeEach(set index) before each draw
	void drawInstances(std::shared_ptr<ShaderManager> shaders, const std::vector<InstanceSet>& sets, const std::function<void(size_t)>& beforeEach = nullptr) const;

	//Like drawInstances for special bricks, turns on specialMesh in brickShader while it draws
	void drawSpecial(std::shared_ptr<ShaderManager> shaders, const std::vector<SpecialSet>& sets, const std::function<void(size_t)>& beforeEach = nullptr) const;

	//Draws a chunk's boxes then its special shapes into a shadow pass, skipping the kinds of bricks not asked for
	void drawChunkShadow(const Chunk* chunk, bool opaque, bool transparent, GLint specialUniform) const;

	public:

	//A brick drawn on its own with any rotation, centered on the transform's origin
	struct LooseBrick
	{
		const Brick* brick;
		glm::mat4 transform;
		float alpha;
	};

	//A brick group to draw, and the transform that puts its bricks' grid in the world
	struct GroupDraw
	{
		int group;
		glm::mat4 transform;
	};

	void addBrick(Brick* brick);
	void removeBrick(Brick* brick);

	//Call after changing a brick's color
	void updateBrick(Brick* brick);

	void clear();

	//Re-uploads chunks changed since the last call, stopping once budgetMS has been spent
	void rebuildDirty(float budgetMS);

	//Bricks drawn together wherever a transform puts them, like a vehicle's, uploaded right away. Returns the ID the calls below take
	int addBrickGroup(const std::vector<Brick>& bricks);
	void removeBrickGroup(int group);

	//Expects shaders->brickShader to be in use, culls chunks against the camera currently in shaders->cameraUniforms
	void render(std::shared_ptr<ShaderManager> shaders, bool transparent) const;

	/*
		Expects shaders->brickDepthShader to be in use: draws the same opaque bricks render would, nearest first,
		filling only depth so the shading pass can throw away everything behind them. Each chunk is one draw of the
		whole cube rather than three by face, since which material a face uses doesn't matter here
	*/
	void renderDepth(std::shared_ptr<ShaderManager> shaders) const;

	//Same as render, for brick groups
	void renderGroups(std::shared_ptr<ShaderManager> shaders, const std::vector<GroupDraw>& draws, bool transparent) const;

	//Expects shaders->brickShader to be in use, draws one translucent brick that brightens and turns more opaque as pulse goes from 0 to 1
	void renderGhost(std::shared_ptr<ShaderManager> shaders, const Brick& ghost, float pulse) const;

	/*
		Expects shaders->brickShader to be in use
		Draws blended since loose bricks fade out, but still writes depth so later transparent geometry can't show through them
	*/
	void renderLoose(std::shared_ptr<ShaderManager> shaders, const std::vector<LooseBrick>& bricks) const;

	/*
		Expects shaders->brickShadowCascadeShader, or brickShadowTintShader with tintProgram set, to be in use
		Draws the chunks that can cast into one shadow cascade, then any brick groups in draws
		A point light's pass passes where it is as skipPoint, which each group gets in its own space
	*/
	void renderShadowCascade(const glm::mat4& lightSpaceMatrix, bool opaque, bool transparent, bool tintProgram = false,
		const std::vector<GroupDraw>* draws = nullptr, const glm::vec3* skipPoint = nullptr) const;

	bool hasTransparentBricks() const;

	//How many chunks this frame's shadow passes drew, and how many they had to look at to decide, for the debug menu
	std::string getShadowStats() const;
	void resetShadowStats() const { shadowChunksDrawn = 0; shadowChunksTested = 0; }

	//Goes up every time a chunk is rebuilt, so anything drawn from the bricks earlier (like point light shadows) knows they've changed
	unsigned int getGeneration() const { return generation; }

	InstancedBrickRenderer(std::shared_ptr<ShaderManager> shaders, std::shared_ptr<TextureManager> textures, const BrickTypes* _types);
	~InstancedBrickRenderer();
};
