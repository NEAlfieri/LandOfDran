#pragma once

#include "../LandOfDran.h"
#include "Shader.h"

#include <glm/glm.hpp>

//Helper function to pass a 4x4 matrix to a shader
void UniformMat(GLint location, glm::mat4 mat);

//Helper function to pass a vec3 to a shader
void UniformVec3(GLint location, glm::vec3 vec);

//Helper function to pass a vec4 to a shader
void UniformVec4(GLint location, glm::vec4 vec);

/*
	Used for glActiveTexture and glUniformI for shader sampler2D uniforms
*/
enum TextureLocations
{
	PBRArray = 0,
	DecalArray = 1,
	ShadowArray = 2,
	TintDepthArray = 3,
	TintColorArray = 4,
	Refraction = 5,
	Reflection = 6,
	PointShadowArray = 7,
	ScreenCopy = 8,
	ParticleTexture = 9,
	PointTintDepthArray = 10,
	PointTintColorArray = 11,
	//Cube maps of the day and night skies, see Skybox
	SkyDay = 12,
	SkyNight = 13,
	//Depth from straight above the area around the camera, see Rain
	RainMap = 14,
	//Where the sky still shows around the sun, see LoopClient::renderGodRays
	GodRayMask = 15,
	//Add as needed:
	/*BRDF = 2,
	HeightMap = 3,
	ShadowNearMap = 4,
	ShadowFarMap = 7,
	ShadowColorMap = 8,
	ShadowNearTransMap = 9,
	CubeMapEnvironment = 10,
	CubeMapRadiance = 11,
	CubeMapIrradiance = 12;*/
};

enum UniformType
{
	IsMatrix = 0,
	IsFloat = 1,
	IsInt = 2,
	IsVec3 = 3
};

/*
	Contains a handle to a single OpenGL shader program
	As well as a list of attached shaders, uniform locations, and uniform defaults
*/
class Program
{
	private:
		//Has the program been succesfully compiled and linked yet?
		bool valid = false;

		//Various shaders that may or may not be used and linked to this program:
		Shader* vertex = 0;
		Shader* fragment = 0;
		Shader* tessEval = 0;
		Shader* tessControl = 0;
		Shader* geometry = 0;

		//Handle to the OpenGL program object itself
		GLuint handle = 0;

		//Obtained by glGetUniformLocation with this program
		std::vector<GLint> uniforms;
		//Should we set the uniform to anything specific by default each time we use this program
		std::vector<bool>  useDefaults;
		//Which of the below vectors will contain a non-dummy default value for that uniform:
		std::vector<UniformType>	defaultTypes;

		//These vectors will all be the same size cause it's easier that way, if a given index is not of a given type, it will just have a dummy value inserted:
		std::vector<glm::mat4>	defaultMats;
		std::vector<float>		defaultFloats;
		std::vector<int>		defaultInts;
		std::vector<glm::vec3>	defaultVec3s;

		/*
			Sets int uniforms for GLSL sampler2Ds to their values corresponding to the
			values in TextureLocations enum.
			Currently run only once on program initial linkage.
		*/
		void registerSamplerUniforms();

	public:
		//Add a 4x4 matrix uniform to this program, returns glGetUniformLocation for later binding. Can optinally set a default value to be passed for each time the program is used.
		GLint registerUniformMat(std::string name, bool useDefault = false, glm::mat4 defaul = glm::mat4(1.0));
		//Add a int uniform to this program, returns glGetUniformLocation for later binding. Can optinally set a default value to be passed for each time the program is used.
		GLint registerUniformInt(std::string name, bool useDefault = false, int   defaul = 0);
		//Add a boolean (int) uniform to this program, returns glGetUniformLocation for later binding. Can optinally set a default value to be passed for each time the program is used.
		GLint registerUniformBool(std::string name, bool useDefault = false, bool  defaul = false);
		//Add a float uniform to this program, returns glGetUniformLocation for later binding. Can optinally set a default value to be passed for each time the program is used.
		GLint registerUniformFloat(std::string name, bool useDefault = false, float defaul = 0);
		//Add a glm::vec3 uniform to this program, returns glGetUniformLocation for later binding. Can optinally set a default value to be passed for each time the program is used.
		GLint registerUniformVec3(std::string name, bool useDefault = false, glm::vec3 defaul = glm::vec3(0, 0, 0));

		//Set any uniform with a default value to its default value
		void resetUniforms() const;

		//Add a shader to this program, must be called before compile
		void bindShader(Shader* toBind);

		//Create the OpenGL program from the shaders already bound
		void compile();

		//Was compile() called and did it succeed
		bool isCompiled() const { return valid; };
		
		//Calls glUseProgram and also resetUniforms by default
		void use(bool reset = true) const;

		//Helper function to get the location of a temporary or debug uniform
		GLint getUniformLocation(const std::string &name) const;

		//Allows us to bind a uniform buffer object to this program
		void bindUniformBlock(const std::string &glslName, GLuint UBOhandle, int index) const;
};
