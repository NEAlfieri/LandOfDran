#pragma once

#include "../LandOfDran.h"
#include "Material.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//How many instances for an instanced mesh we allocate space for at a time, don't set to 0
#define InstanceBufferPageSize 10

/*
	Determines what order the various per-vertex variables
	are passed to the OpenGL shaders i.e. the 0 in:
	layout(location = 0) in vec3 positions;
	Used in glVertexAttribPointer
*/
enum LayoutSlot
{
	ModelSpace = 0,			//vec3 - Per-vertex position in model space
	NormalVector = 1,		//vec3 - Normal in model space
	TangentVector = 2,		//vec3
	BitangentVector = 3,	//vec3
	TextureCoords = 4,		//vec2 - uvs
	PreColor = 5,			//vec4 - Optional per-brick or per-mesh color before material application (per-instance)
	InstanceFlags = 6,		//int  - Contains additional per-instance info such as decal used, or mouse-picking info
	ModelTransform = 7,	//mat4 - Contains the model matrix if rendering instanced models (per-instance), occupies locations 7-10
	//ModelTransform occupies locations 7,8,9,10 (a mat4 needs 4 attribute locations), so the next free slot is 11
	HighlightColor = 11,		//vec4 - Outline/highlight color for the whole model instance, alpha <= 0 means no highlight (per-instance)
	HighlightThickness = 12,	//float - How far (in world units) to extrude the outline pass along vertex normals (per-instance)
	/*
		vec3 - Per-vertex, used only by the outline pass to decide which way to push a vertex out.
		NormalVector above is frequently a hard per-face normal (adjacent faces don't share vertices with
		matching normals), which would tear the extruded outline shell apart at every edge. This is instead
		an average of the per-face normals of every vertex that shares a position, so the shell stays welded.
	*/
	SmoothNormal = 13
};

//Number of GL buffer objects a Mesh allocates, must be at least SmoothNormal + 1
#define MeshBufferCount 14

#define MeshFlag_UsePickingColor  256     //Render the mesh with colors for mouse picking
//Bits 0-7 would then be the mouse picking ID if that flag is enabled
#define MeshFlag_UseDecal		  131072  //Should we render a decal on top of our current material
//Bits 9-16 would then be the decal ID
#define MeshFlag_SkipCameraMatrix 262144  //Skip camera projection and view matricies in vertex shader
#define MeshFlag_DecalCutout	  524288  //Only the opaque part of the mesh's decal is drawn, none of it without a decal, and it casts no shadow or outline

class Mesh;
class Model;
class Node;

/*
	Describes one of a Model's animations
	These are generally created in Lua on the server after loading the model
*/
struct Animation
{
	//Models are imported with 'one' node based animation by assimp
	//These animation structs represent a 'slice' in time of the 'one' big animation

	//What time in the model's animation reel does it start
	float startTime = 0;
	//What time in the model's animation reed does it end
	float endTime = 0;
	//Stacks with AnimationPlayback::animationSpeed
	float defaultSpeed = 0.04f;
	//So the server can play animations by ID
	int serverID = -1;
	//I suppose if you were hard coding animations you could use this, but you shouldn't
	std::string name = "";
	//How fast do we interpolate from no transform to the first frames of the animation
	float fadeInMS = 0;
	//How fast do we interpolate from the last frames of the animation back to idle pose
	float fadeOutMS = 0;

	/*
		Client only, by node index: whether this slice moves the node or holds it away from its rest pose
		Nodes an animation leaves alone keep whatever animations under it are doing, i.e. walking legs while the arm grabs
		Filled in by Model::addAnimation
	*/
	std::vector<bool> affectedNodes;
};

/*
	Describes the current playing of an animation by an instance
	These are created when you play an animation and stored in an instance
	And destroyed when the animation stops (if its not a looping animation)
*/
struct AnimationPlayback
{
	//Which animation is this playing
	const Animation * animation = nullptr;

	/*
		This is incremented by speed* deltaT and the animation frame rounded down
		and the one from rounding up will be interpolated to create the final 
		contribution of this particular animation to the final meshes' transforms
	*/
	float animationTime = 0;

	//Stacks with Animation::defaultSpeed
	float animationSpeed = 1;

	//Will still render / affect the instance, just won't progress from its current frame
	bool animationPaused = false;

	//Destroy this animation when we're done with it
	bool animationLooping = false;

	//This animation has finished or was stopped and is fading out, animationFadeOut will count towards 0
	bool animationEnding = false;

	bool animationStarting = false;

	//If animationEnding is true this value will go to 0 over time then the animation will be deleted
	float animationFadeOut = 1.0;
};

/*
	Contains the information needed to render one instance of a Model
	One set of data for each instanced layout binding point for each mesh of the model
*/
class ModelInstance
{	 
	friend class Mesh;

	Model* type = nullptr;

	//See note for struct AnimationPlayback
	std::vector<AnimationPlayback> playingAnimations;

	/*
 		These allow you to manually tweak the rotation of a given node
   		For instance if you want to make a player's head tilt when they look up or down
     	*/
	std::vector<glm::quat>  NodeRotationFixes;
	//If NodeRotationFixes should be used for that given node
	std::vector<bool> 	UseNodeRotationFix;

	//The transform for the entire model instance
	glm::mat4 wholeModelTransform = glm::mat4(1.0);

	//Used to hide this entire instance, i.e. we don't want to see our own player model most of the time
	bool hidden = false;

	//A hidden instance with this set still casts shadows, see setHidden
	bool hiddenCastsShadow = false;

	/*
		These are calculated in calculateMeshTransforms per Mesh based on:
  		The node hierarchy as imported from Assimp with any baked in hierarchical transformations
    	Any currently playing animations
      	Any manually applied NodeRotationFixes
		wholeModelTransform
 	*/
	std::vector<glm::mat4>		MeshTransforms;
	std::vector<unsigned int>	MeshFlags;
	std::vector<glm::vec4>		MeshColors;
	std::vector<bool>			MeshColorUsed; //Mostly for use with server to not send default mesh colors over

	/*
		Outline/highlight effect for the whole model instance (not per-mesh, unlike MeshColors above)
		but still uploaded per-mesh since that's how the instanced buffers are laid out.
		Alpha <= 0 means no highlight is currently applied.
	*/
	glm::vec4 highlightColor = glm::vec4(0, 0, 0, 0);
	float highlightThickness = 0;
	bool highlightUpdated = true;

	//Were any of the above properties changed since last frame:

	/*
		Used for node rotation fixes changes
		And also set true briefly when animationsPlaying goes to 0...
		...this gives us one frame where we set the transforms based on animationDefaultTime
	*/
	bool transformUpdated = true;

	//Not actually included in anything updated at the moment
	bool wholeModelTransformUpdated = true;

	//Does not include wholeModelTransform atm, does include flags, colors, and node rotation fixes
	bool anythingUpdated = true;

	//... while these are per-mesh
	std::vector<bool> flagsUpdated;
	std::vector<bool> colorsUpdated;

	//What offset to use for Mesh buffers for instanced data when calling performMeshBufferUpdates
	unsigned int bufferOffset = 0;

	//Handles moving animations forward over time and removing completed ones
	void progressAnimations(float deltaT);

	//Calculates what the transform of a given node for this instance should be taking animations into account
	glm::mat4 calculateNodeTransform(Node const * const node);

	//Set a flag or a custom color on a single mesh instance of the model instance
	void setFlags(int meshId, unsigned int flags);

	public:

	//How many different meshes there are with colors transforms flags, etc
	int getNumMeshes() const { return MeshTransforms.size(); }

	//World space middle of one of its meshes as of the last calculateMeshTransforms and setModelTransform, see Mesh::center
	glm::vec3 getMeshCenter(int meshIdx) const;

	//Where one of its meshes sits inside the model, baseScale and any playing animation applied, but not
	//setModelTransform's whole model transform. What a draw that puts the model somewhere of its own needs, see Mesh::renderOnce
	const glm::mat4& getMeshTransform(int meshIdx) const { return MeshTransforms[meshIdx]; }

	//World space rotation of one of its meshes, with any scale taken out, as of the same updates as getMeshCenter
	glm::quat getMeshRotation(int meshIdx) const;

	//Returns true if color has been specifically set, or false if it's default
	bool getMeshColor(int meshIdx, glm::vec4& color) const;

	bool getHidden() const { return hidden; }

	//InstanceFlags bit model.vert and highlight.vert skip drawing for, while the shadow shaders still draw it
	static constexpr unsigned int hiddenCastingShadowFlag = 262144;

	/*
		Hidden instances aren't drawn at all, unless castShadow is true, then they still cast shadows
		i.e. your own player in first person
	*/
	void setHidden(bool _hidden, bool castShadow = false);

	//Change the position/scale/rotation for the whole model instance
	void setModelTransform(glm::mat4 &&transform);
	//For little tweaks like making a player's head swivel up and down based on where they look
	void setNodeRotation(int nodeId,const glm::quat &rotation);
	void setColor(int meshId,glm::vec4 color);
	//Applies a decal to an instance of a mesh on a model, pass -1 as decalId to remove
	void setDecal(int meshId, int decalId);
	//Calls setDecal with decalId = -1
	void removeDecal(unsigned int meshId);

	//World space transform of one of its meshes' own space: setModelTransform's transform over getMeshTransform, as of the same updates as getMeshCenter
	glm::mat4 getMeshWorldTransform(int meshIdx) const { return wholeModelTransform * MeshTransforms[meshIdx]; }

	//See setHidden
	bool getHiddenCastsShadow() const { return hiddenCastsShadow; }

	/*
		Puts an instance of a model that's worn on this one, like a hat on a player, where its attach lines say, see
		Model::attachMesh, and hides it along with this. Call once a frame after this instance's transforms are up to
		date and before the part's model is updated. False, hiding the part, if this model has no mesh by that name
		scale is on top of the part's own size, see Model::getAttachTransform
	*/
	bool placeAttachment(ModelInstance* part, float scale = 1.0f) const;

	//Returns true if a highlight/outline is currently applied (color.a > 0), false otherwise. color/thickness are always written either way
	bool getHighlight(glm::vec4& color, float& thickness) const;
	//Applies an outline/highlight effect to the whole model instance, pass color.a <= 0 (or call clearHighlight) to remove it
	void setHighlight(const glm::vec4& color, float thickness);
	//Calls setHighlight with a fully transparent color, removing any current highlight
	void clearHighlight();

	/*
		Every ModelInstance that currently has an active highlight (color.a > 0), maintained by setHighlight/clearHighlight
		and this instance's destructor. LoopClient::renderEverything walks this directly to draw the highlight/outline
		X-ray pass, rather than looping over every SimObject to find the handful that are actually highlighted.
	*/
	static std::vector<ModelInstance*> highlightedInstances;

	//Renders just this one instance (all of its meshes) with the outline shader, using its buffer offset via
	//glDrawElementsInstancedBaseInstance. Assumes the outline shader/GL state (stencil, cull face, maskPass uniform,
	//etc) has already been set up by the caller - see LoopClient::renderEverything for the full per-instance sequence
	void renderSelfOutline() const;

	/*
		Calculates the transform of each indivdual node based on things, see above
		Also handles playing of animations
		Call once per frame if the object is animated or has moved
		Set debugLayer to 0 to do a hierarchical std::cout print of transforms
	*/
	void calculateMeshTransforms(float deltaT,glm::mat4 currentTransform = glm::mat4(1.0),Node const * currentNode = 0,int debugLayer = -1);

	/*
		Calls glBufferSubData on mesh buffers that need updating, and sets updated flags to false
		Call this once per instance per frame, after any calls to set* but before rendering
	*/
	void performMeshBufferUpdates();

	//Calls calculateMeshTransforms and performMeshBufferUpdates
	void update(float deltaT,bool debug = false);

	//If the animation with that id is playing
	bool isPlaying(int id) const;
	//Will do nothing if animation with that id is already playing
	void playAnimation(int id,bool loop);
	//Plays an animation once from its start, even if it was already partway through
	void restartAnimation(int id);
	/*
		Does nothing if animation with id is not playing
		Otherwise will fade out over 200Ms or so then be removed from playing anims list
	*/
	void stopAnimation(int id);

	ModelInstance(Model * _type); 
	~ModelInstance();
};

class Mesh
{
	friend class Model;
	friend class ModelInstance;

	//For collision meshes, binding points, other stuff that might be included with models we don't want to see
	bool nonRenderingMesh = false;

	//From a fixedcolor line in the descriptor: keeps its material's look, an instance color is never put on it, like the gold bells of a painted hat
	bool fixedColor = false;

	/*
		True only once vao/buffers/indexBuffer below have actually been allocated via the glGen family of calls.
		Server-side meshes return out of the constructor before ever reaching that point (dedicated
		servers never create a GL context, so those functions are null function pointers there), and
		nonRenderingMesh meshes (e.g. "collision") also skip it - both must leave this false so the
		destructor knows not to call the glDelete family of functions on handles that were never created.
	*/
	bool hasGLResources = false;

	//How many instances worth of space we've allocated in each of the instanced buffers
	unsigned int instancesAllocated = 0;

	//This vector's members are shared between other Mesh's of the same Model
	std::vector<ModelInstance*> instances;

	//Assimp can specifiy a material for this particular mesh
	const Material * material = nullptr;

	//If the model came with normals when loaded
	bool hasNormals = false;

	//Has both tangents *and* bitangents cause why would you have one but not the other?
	bool hasTangents = false;

	//Did the model come with texture coordinates
	bool hasUVs = false;

	//If everything loaded okay and this mesh can be used
	bool valid = false;

	//How many verticies (including duplicates) does this mesh have
	unsigned int vertexCount = 0;

	//Used for seeing what index to use when reading from vectors in ModelInstance for rendering
	size_t meshIndex = 0;

	std::string name = "";

	//Middle of its vertices' bounding box in model space, for emitters following one mesh of a dynamic, client only
	glm::vec3 center = glm::vec3(0);

	//Lowest corner of that bounding box, client only
	glm::vec3 low = glm::vec3(0);

	/*
		Its bounding box in the whole model's space, its node's transform applied but not baseScale, from Model::calculateMeshBounds
		Unlike center/low this is filled server side too, so servers can work out which mesh of a model a point is on, see Model::getMeshAtPoint
		A mesh no node draws, or one with no vertices, is left with boundsHigh below boundsLow
	*/
	glm::vec3 boundsLow = glm::vec3(1);
	glm::vec3 boundsHigh = glm::vec3(-1);

	//Whether calculateMeshBounds found any vertices for it
	bool hasBounds() const { return boundsHigh.x >= boundsLow.x; }

	/*
		Texture coordinates of a decal's top left and bottom right corners on this mesh, client only
		Nothing outside of them gets the decal, like the sides and back of a torso wearing a shirt
		From a decalarea line in the model's .txt, otherwise the whole 0 to 1 range
	*/
	glm::vec4 decalArea = glm::vec4(0, 0, 1, 1);

	/*
		Vertex array object that contains the mesh
		With verticies, and optionally normals, tangents, uvs, etc.
		Can be used for instanced or non-instanced rendering
	*/
	GLuint vao;

	//Buffers correspond to LayoutSlot
	GLuint buffers[MeshBufferCount];

	/*
		Additional buffer that holds the index of the next vertex in the above buffers so
		repeat verticies don't need to be passed to the GPU multiple times
		In other words: GL_ELEMENT_ARRAY_BUFFER Indicies are 16 bit unsigned shorts
	*/
	GLuint indexBuffer;

	//Helper function to remove duplicated code from Mesh::Mesh. 
	//Cannot be used for index buffer. Make sure VAO is already bound
	void fillBuffer(LayoutSlot slot, void* data, int size, int elements);

	Mesh(aiMesh const * const src,Model const * const parent,bool serverSide = false);
	~Mesh();

	public:

	//A slightly quicker version of calling performMeshBufferUpdates on every single instance
	//Instances whose copy of this mesh sits further than cullDistance from cullFrom are left out, 0 for no limit, see Model::updateAll
	void recompileInstances(const glm::vec3& cullFrom = glm::vec3(0), float cullDistance = 0);

	//Render all instances of this particular mesh
	void render(std::shared_ptr<ShaderManager> graphics, bool useMaterials = true) const;

	/*
		Draws one copy of the mesh with no instance of the model behind it, from the model matrix in
		BasicUniforms instead, which the caller sets along with BasicUniforms::nonInstanced.
		Used for drawing a model somewhere that isn't the world, like an item's picture in the item bar
	*/
	void renderOnce(std::shared_ptr<ShaderManager> graphics) const;

	//Renders exactly one instance of this mesh (by its buffer offset) using the outline shader, assumed already
	//in use. Used for the highlight/outline X-ray effect, one ModelInstance at a time - see ModelInstance::renderSelfOutline
	void renderSingleInstance(unsigned int bufferOffset) const;
};

class Node
{
	friend class Model;
	friend class ModelInstance;

	
	/*
		Generally how animations work
		Is model's are limited to one 'animation' in the sense Assimp loads them
		from models, in particular, FBX models
		However different 'Animations' in the sense of our Animation struct
		Are then defined based on time slices of the one used Assimp animation
		Animations are node based, rather than truely skeltal and don't include scaling keys
		At least for now...
	 
		Position of animation keyframes
	*/
	std::vector<glm::vec3> posFrames;
	//Animation time of associated position keyframe
	std::vector<float> posTimes;
	//Rotation of animation keyframes
	std::vector<glm::quat> rotFrames;
	//Animation time of associated rotation keyframe
	std::vector<float> rotTimes;

	//The node's pos and rot at a time in the animation reel, false if it has no keys covering that time
	bool sample(float time, glm::vec3& pos, glm::quat& rot) const;
	//Literally just gets a certain frame, if frame is out of bounds, rot and pos are unchanged
	void getFrame(float frame, glm::vec3& pos, glm::mat4& rot) const;

	/*
		The transform a node will have, if no animations are playing
		Is overrided entirely if an animation is playing that affects that node
	*/
	glm::mat4 defaultTransform = glm::mat4(1.0);
	glm::vec3 defaultPos = glm::vec3(0, 0, 0);
	glm::quat defaultRot = glm::quat(1, 0, 0, 0); //From defaultTransform

	//Leaves of the tree
	std::vector<Mesh*> meshes;

	//Babies
	std::vector<Node*> children;

	//Node above
	const Node* parent = nullptr;

	//Position in allNodes
	size_t nodeIndex = 0;

	/*
		This is added by Assimp
		The matrix multiplication for each node goes:
		translate(rotationPivot) * rotate(rotation) * translate(-rotationPivot) * translate(pos) * higherNode...
	*/
	glm::vec3 rotationPivot = glm::vec3(0, 0, 0);

	/*
		What ModelInstance::setNodeRotation turns the node around, rotationPivot unless Model's constructor knows better
		A player's Head turns at the bottom of the head, its rotationPivot is down inside the torso and would lift it off the neck
	*/
	glm::vec3 fixPivot = glm::vec3(0, 0, 0);

	//Undoes some of Assimps importing silliness that creates a billion extra nodes, see stripSillyAssimpNodeNames
	void foldNodeInto(aiNode const * const src, Model * parent);

	//Used for setting properties through script
	std::string name = "";

	Node(aiNode const* const src, Model* parent);
	~Node();
};

class Model
{
	friend class ModelInstance;
	friend class Mesh;
	friend class Node;

	std::vector<ModelInstance*> instances;

	//Every mesh the model has stored in a way that doesn't require recusion to access
	std::vector<Mesh*> allMeshes;
	
	//Nodes for non-hierarchical access
	std::vector<Node*> allNodes;

	//Assigned to indivdual meshes...
	std::vector<Material*> allMaterials;

	//Entry point to recursive calculations
	Node* rootNode = nullptr;

	//Was at least one node an Assimp inserted rotation pivot node
	bool rotationPivotsApplied = false;

	/*
		Whether an animated node's defaultTransform is its rest pose, or just wherever the file happened
		to be saved. A DTS shape gives every node a default translation and rotation of its own, apart
		from any sequence, so it is one. An FBX node is only the scene graph as the artist left it: every
		animated node in Brickhead.fbx sits at the origin except the right arm, which sits mid-grab
		because that is the frame the file was saved on. So an FBX keeps no translation and no rotation
		as the rest pose, which is what Model::addAnimation assumes when working out which nodes an
		animation actually moves, and what ModelInstance::calculateNodeTransform falls back to.
	*/
	bool nodeDefaultsAreRestPose = false;

	/*
		Which way the model points when it's fired as a projectile, the axis Dynamic::faceVelocity turns
		along its velocity. Our own models are built with +Y forward. A DTS shape is built with +Y forward
		in its own Z up world, which the loader's root turn brings round to -Z here, so a Torque bullet
		and its trail would otherwise fly side on.
	*/
	glm::vec3 projectileForward = glm::vec3(0, 1, 0);

	//What frame of animation should be displayed for an instance when we're not playing any animations on it
	float animationDefaultTime = 0;

	//See getHeadNodeIdx
	int headNodeIdx = -1;

	//For use with creating a btRigidBody box shape, calculated in calculateCollisionBox
	glm::vec3 collisionHalfExtents = glm::vec3(1, 1, 1);
	//The offset the visual mesh should have from the collision box, calculated in calculateCollisionBox
	glm::vec3 collisionOffset = glm::vec3(0, 0, 0);

	//Loaded server side, no animations or textures expected
	bool serverSide = false;

	//If a camera is bound to this, offset it by this much
	glm::vec3 eyePosition = glm::vec3(0, 1, 0);

	//Were there no errors on loading this Model:
	bool valid = false;

	//Reads one of a descriptor's attach lines into the members below, false if the line isn't one
	bool parseAttachLine(const std::string& argument, const std::string& value);

	public:

	/*
		For a model worn on another, like a hat in Assets/brickhead/parts on a player's Head, from the attach lines of
		its descriptor: the lower case name of the mesh it hangs on, empty for a model that isn't worn. It's put with
		the bottom of its box on the top middle of that mesh's, moved by attachoffset in that mesh's own units and
		turned by attachrotation (degrees about X, Y, and Z), see ModelInstance::placeAttachment. An attachscale
		line sets baseScale, so a file in other units can be sized to the model it's worn on
	*/
	std::string attachMesh = "";
	glm::vec3 attachOffset = glm::vec3(0);
	glm::quat attachRotation = glm::quat(1, 0, 0, 0);

	bool isAttachment() const { return !attachMesh.empty(); }

	/*
		attachOffset and attachRotation, with the lift that puts the lowest point of its turned box at the origin, for
		placeAttachment. scale is a player's own size for it on top of attachscale: the lift and the offset scale with
		it, so a small hat rests on the head rather than sinking into it as far as a full size one goes over it
	*/
	glm::mat4 getAttachTransform(float scale = 1.0f) const;

	//Any errors that occured during loading?
	bool isValid() const { return valid; }

	std::vector<Animation> animations;

	//Returns -1 on invalid name
	int getMeshIdx(const std::string& name) const;

	//Server ID of the animation added with that name, -1 if there isn't one
	int getAnimationID(const std::string& name) const;

	//Client only: the node named Head, which turns to show where a player is looking, -1 without one
	int getHeadNodeIdx() const { return headNodeIdx; }

	/*
		Where the node with that name sits in the model's own space, baseScale applied, with nothing
		animating and no whole model transform, so it's the rest pose spot the file was exported with.
		Case doesn't matter. False, leaving position alone, if the model has no node by that name

		A Blockland .dts names the spots an add-on cares about, so this is how a script finds them:
		a jeep's wheels hang from hub0 to hub3 and its riders sit on Mount0 and up, see Lua's getTypeNodePosition
	*/
	bool getNodePosition(const std::string& name, glm::vec3& position) const;

	//Same, for names that might not have kept their case, like ones read back from settings
	int getMeshIdxIgnoringCase(const std::string& name) const;

	int getNumMeshes() const { return (int)allMeshes.size(); }

	const std::string& getMeshName(int meshIdx) const { return allMeshes[meshIdx]->name; }

	//False for meshes that are never drawn, like the collision box
	bool isMeshDrawn(int meshIdx) const { return !allMeshes[meshIdx]->nonRenderingMesh; }

	//Drawn and not kept its own color by a fixedcolor line, so an instance color shows on it
	bool isMeshPaintable(int meshIdx) const { return isMeshDrawn(meshIdx) && !allMeshes[meshIdx]->fixedColor; }

	//Where a player's face goes: Face1 in the default player model, otherwise Face or Head, -1 without any of them
	int getFaceMeshIdx() const;

	//Where a player's shirt goes: Torso, -1 without one
	int getShirtMeshIdx() const;

	glm::vec3 getEyePosition() const { return eyePosition * baseScale; }

	//The model space axis a projectile of this model flies along, see projectileForward
	glm::vec3 getProjectileForward() const { return projectileForward; }

	bool isServerSide() const { return serverSide;  }

	glm::vec3 getColOffset() const { return collisionOffset * baseScale; }
	glm::vec3 getColHalfExtents() const { return collisionHalfExtents * baseScale; }

	std::string loadedPath = "";

	//Add another animation to this Model, server side can leave id as default -1, client side use server provided id
	void addAnimation(Animation& animation,int id = -1);

	//Outputs the entire hierarchy in depth to std::cout for debugging
	void printHierarchy(Node * node = 0,int layer = 0) const;

	/*
		Calls recompileInstances on each mesh, or the equivlent of calling performMeshBufferUpdates on every instance of this model
		Also calculates every instances mesh transforms too
		cullDistance is graphics/drawdistance: instances further than that from cullFrom (the camera) are left out of
		the buffers entirely, so they're drawn neither in the scene nor in any shadow pass. 0 draws all of them
	*/
	void updateAll(float deltaT, const glm::vec3& cullFrom = glm::vec3(0), float cullDistance = 0);

	/*
		FBX models will be loaded 100x larger than they should be
		Be sure to call performMeshBufferUpdates on instances / recompileAll after changing this
	*/
	glm::vec3 baseScale = glm::vec3(1, 1, 1);

	void setDefaultFrame(float time);

	//Calls render on each mesh
	void render(std::shared_ptr<ShaderManager> graphics,bool useMaterials = true) const;

	//Calls render on just one of its meshes
	void renderMesh(std::shared_ptr<ShaderManager> graphics, int meshIdx) const;

	//Calls renderOnce on just one of its meshes, see Mesh::renderOnce
	void renderMeshOnce(std::shared_ptr<ShaderManager> graphics, int meshIdx) const;

	/*
		The box its drawn meshes fill, baseScale applied, in the model's own space. False for a model with
		nothing drawn in it. Taken from the pose it was loaded in, so an animation having moved a limb since isn't counted
	*/
	bool getDrawnBounds(glm::vec3& low, glm::vec3& high) const;

	//Calls renderSingleInstance(bufferOffset) on each mesh, assumes the outline shader is already bound.
	//Used for the highlight/outline X-ray effect - see ModelInstance::renderSelfOutline
	void renderSingleInstance(unsigned int bufferOffset) const;

	//Calculates collisionHalfExtents and collisionOffset, called in constructor
	void calculateCollisionBox(const aiScene* scene);

	//See getNodePosition, above is the transform every node over this one adds up to
	static bool findNodePosition(Node const * const node, const std::string& lowerName, const glm::mat4& above, glm::vec3& position);

	//Fills every mesh's boundsLow/boundsHigh, called in both constructors
	void calculateMeshBounds(const aiScene* scene);

	/*
		Which mesh a point in the model's own space (before baseScale) is on or nearest to, -1 for a model with nothing paintable
		Meshes that are never drawn, and the see-through face plate over a head, are skipped, and the smallest box wins a tie
		Uses the bounding boxes the model was loaded in, so an animation having moved a limb since isn't taken into account
	*/
	int getMeshAtPoint(const glm::vec3& point) const;

	/*
		File path refers to a text file that describes where the actual model is
		Flags for how to load it, and other text files describing materials
	*/
	Model(std::string filePath, std::shared_ptr<TextureManager> textures,glm::vec3 _baseScale);

	/*
		Server-side loading for collision meshes
		The bool argument mostly exists at the moment to make sure you don't accidently call it because you forgot TextureManager
	*/
	Model(std::string filePath,bool _serverSide,glm::vec3 _baseScale);

	~Model();
};
