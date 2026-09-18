#pragma once

#include "../LandOfDran.h"
#include "Program.h"

/*
	Model matrix and material data for non-instanced rendering for a uniform buffer object
	Padding exists to conform to an std140 layout used by OpenGL
	OpenGL Size: 272 bytes. Every shader that declares the BasicUniforms block has to list the same
	members in the same order, and both glBufferData calls for it have to use that size, or programs
	silently read the wrong offsets
*/
struct BasicUniforms					
{														//base			aligned
	//Model matrix:
	glm::mat4 TranslationMatrix = glm::mat4(1.0);		//16*4			0
	glm::mat4 RotationMatrix = glm::mat4(1.0);			//16*4			64
	glm::mat4 ScaleMatrix = glm::mat4(1.0);				//16*4			128

	//Material uniforms:
	GLint useAlbedo = 0;								//4				192		after revisions, we got lucky here
	GLint useNormal = 0;								//4				196		there's nothing oddly sized that requires padding
	GLint useMetalness = 0;								//4				200
	GLint useRoughness = 0;								//4				204
	GLint useHeight = 0;								//4				208
	GLint useAO = 0;									//4				212
	
	GLint nonInstanced = 0;								//4				216
	GLint cameraSpacePosition = 0;						//4				220

	//Texture coordinates of a decal's top left and bottom right corners on the mesh being drawn, see Mesh::decalArea
	glm::vec4 DecalArea = glm::vec4(0, 0, 1, 1);		//16			224

	/*
		What a material without a texture for one of these is drawn with, from a plain number in its
		descriptor instead of a file name, see Material. The defaults are what the shaders used to
		hard code, so a material that names no constants looks exactly as it did
	*/
	glm::vec4 ConstantAlbedo = glm::vec4(1, 1, 1, 1);	//16			240
	//Metalness, occlusion, roughness, in the order the MOHR layer packs them. The fourth is unused
	glm::vec4 ConstantMOR = glm::vec4(0, 1, 0.5, 0);	//16			256
};

/*
	Camera specific information for a uniform buffer object
	Size: 208 bytes
*/
struct CameraUniforms
{
	//Camera uniforms:
	glm::mat4 CameraProjection = glm::mat4(1.0);		//16*4			0
	glm::mat4 CameraView = glm::mat4(1.0);				//16*4			64
	glm::mat4 CameraAngle = glm::mat4(1.0);				//16*4			128
	glm::vec3 CameraPosition = glm::vec3(0.0);			//16			192   vec3 is technically only 12 bytes, insert 4 bytes padding
	float padding1 = 0;
	glm::vec3 CameraDirection = glm::vec3(0, 0, 1);		//16			208   padding doesn't matter here cause ints have alignment of 4 bytes
};

/*
	Time of day, lighting, fog, and water information for a uniform buffer object
	Every vec3 is followed by a float so std140 needs no extra padding
	Size: 160 bytes
*/
struct EnvironmentUniforms
{
	glm::vec3 SunDirection = glm::vec3(0, 1, 0);		//12			0
	float FogDistanceMin = 150;							//4				12
	glm::vec3 LightDirection = glm::vec3(0, 1, 0);		//12			16
	float FogDistanceMax = 290;							//4				28
	glm::vec3 LightColor = glm::vec3(1, 1, 1);			//12			32
	float WaveTime = 0;									//4				44
	glm::vec3 SkyColor = glm::vec3(0, 0, 1);			//12			48
	float WaterLevel = 0;								//4				60
	glm::vec3 FogColor = glm::vec3(1, 1, 1);			//12			64
	//Height of the highest flat surface, grass or water, sky.frag uses it to find the visible horizon
	float HorizonHeight = 0;							//4				76
	//World space plane for gl_ClipDistance[0], only matters while GL_CLIP_DISTANCE0 is enabled
	glm::vec4 ClipPlane = glm::vec4(0, 0, 0, 0);		//16			80
	glm::vec3 AmbientColor = glm::vec3(0, 0, 0);		//12			96
	//1 normally, fades to 0 as the sun or moon reaches the horizon
	float ShadowStrength = 1;							//4				108
	//Rain, all 0 while it isn't raining and nothing is wet, see Rain::passUniforms
	float RainIntensity = 0;							//4				112
	float RainWetness = 0;								//4				116
	//World heights the rain map's depths of 0 and 1 are at
	float RainMapTop = 0;								//4				120
	float RainMapBottom = 0;							//4				124
	//xy the world x and z of the rain map's corner, z its width, w the world size of one of its texels
	glm::vec4 RainMapArea = glm::vec4(0, 0, 1, 1);		//16			128
	//World height the fog reaches up to, the sky fades into the fog color below it, see DayCycle::fogHeight
	float FogHeight = 40;								//4				144
	float padding1 = 0;									//4				148
	float padding2 = 0;									//4				152
	float padding3 = 0;									//4				156
};

/*
	Lights placed by server Lua for a uniform buffer object, filled by PointLights::update
	Size: 4624 bytes, every member is float aligned so the struct matches std140 as is
*/
struct PointLightUniforms
{
	//Most lights lighting the scene at once, and most of those with shadows, model.frag's array sizes have to match
	static constexpr int maxLit = 32;
	static constexpr int maxShadowed = 8;
	static constexpr int byteSize = 16 + 16 * 3 * maxLit + 64 * 6 * maxShadowed;

	GLint PointLightCount = 0;							//4				0
	float PointShadowTexelScale = 0;					//4				4
	float padding1 = 0;									//4				8
	float padding2 = 0;									//4				12
	glm::vec4 PointLightPositionRange[maxLit];			//16*32			16
	glm::vec4 PointLightColorShadow[maxLit];			//16*32			528
	glm::vec4 PointLightSpotDirection[maxLit];			//16*32			1040
	glm::mat4 PointShadowMatrices[6 * maxShadowed];		//64*48			1552
};

/*
	Skies server Lua picked with setSkybox for a uniform buffer object, filled by Skybox::passUniforms
	Size: 320 bytes
*/
struct SkyUniforms
{
	//Day's 9 irradiance coefficients then night's, xyz only, see Skybox::Slot::irradiance
	glm::vec4 SkyIrradiance[18];						//16*18			0
	//0 while it's day, 1 at night
	float SkyboxBlend = 0;								//4				288
	//SkyboxKind of each
	GLint DaySkybox = 0;								//4				292
	GLint NightSkybox = 0;								//4				296
	//How much each lights the world, only above 0 for a .hdr with image based lighting on, already faded by SkyboxBlend
	float SkyLightDay = 0;								//4				300
	float SkyLightNight = 0;							//4				304
	//Mip level of the roughest reflections
	float SkyReflectionLevels = 0;						//4				308
	float padding1 = 0;									//4				312
	float padding2 = 0;									//4				316
};

/*
	Handles a uniform buffer object and a few other uniform related things for programs
*/
class ShaderManager
{
	private:

	//A handle to the actual OpenGL uniform buffer object
	GLuint basicUBO, cameraUBO, environmentUBO, pointLightUBO, skyUBO;

	public:

	//See note on struct definition, this is passed to a uniform buffer object
	BasicUniforms basicUniforms;

	//See note on struct definition, this is passed to a uniform buffer object
	CameraUniforms cameraUniforms;

	//See note on struct definition, this is passed to a uniform buffer object
	EnvironmentUniforms environmentUniforms;

	//See note on struct definition, this is passed to a uniform buffer object
	PointLightUniforms pointLightUniforms;

	//See note on struct definition, this is passed to a uniform buffer object
	SkyUniforms skyUniforms;

	//Program for blurring a .hdr sky into its reflection mip levels, see Skybox::prefilter
	Program* skyPrefilterShader = new Program();

	//Program for drawing the glow around point lights
	Program* coronaShader = new Program();

	//Program for drawing emitters' particles
	Program* particleShader = new Program();

	//Program for drawing ropes, ribbons lit the way a lit particle is, see Graphics/RopeRenderer.h
	Program* ropeShader = new Program();

	//Program for drawing normal meshes to screen will full PBR based lighting
	Program* modelShader = new Program();

	//Program for drawing bricks to screen
	Program* brickShader = new Program();

	//Program for decals like bullet holes, flat squares in world space lit by model.frag, see Graphics/WorldDecals.h
	Program* decalShader = new Program();

	//Program for the brick depth pre-pass, brick.vert with almost no fragment work, see graphics/depthprepass
	Program* brickDepthShader = new Program();

	//Programs for drawing shadows of normal meshes and bricks into one shadow cascade at a time
	Program* modelShadowCascadeShader = new Program();
	Program* brickShadowCascadeShader = new Program();

	//Program for multiplying transparent bricks' colors into the shadow tint map
	Program* brickShadowTintShader = new Program();

	//Program for drawing the outline/highlight effect on top of normal models
	Program* outlineShader = new Program();

	//Program for drawing the sky behind everything else
	Program* skyShader = new Program();

	//Program for drawing the water surface
	Program* waterShader = new Program();

	//Program for tinting the whole screen while the camera is under the water
	Program* underwaterShader = new Program();

	//Programs for drawing falling rain drops and their splashes, see Rain::render
	Program* rainShader = new Program();
	Program* rainSplashShader = new Program();

	//Programs for the sun disc the god rays come from and for the rays themselves, see LoopClient::renderGodRays
	Program* godRayMaskShader = new Program();
	Program* godRayShader = new Program();

	/*
		Reads a text file to see where we should find the shader files for the above programs
		Returns true if there was an error with at least one shader compilation
	*/
	bool readShaderList(const std::string &filePath);
	
	//Push camera changes to GPU/OpenGL, see struct for further description
	void updateCameraUBO() const;

	//Sends changes to uniformBufferData to the GPU, see struct for further description
	void updateBasicUBO() const;

	//Sends changes to environmentUniforms to the GPU
	void updateEnvironmentUBO() const;

	//Sends changes to pointLightUniforms to the GPU
	void updatePointLightUBO() const;

	//Sends changes to skyUniforms to the GPU
	void updateSkyUBO() const;

	/*
		Associates contained UBOs with their definitions in the OpenGL shaders
		See function definition for what names they should have in shaders
	*/
	void bind(Program* target) const;

	ShaderManager();
	~ShaderManager();
};

