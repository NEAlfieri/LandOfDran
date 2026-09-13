#include "ShaderSpecification.h"

bool ShaderManager::readShaderList(const std::string &filePath)
{
	bool returnValue = false;

	std::ifstream shadersList(filePath.c_str());
	if (!shadersList.is_open())
	{
		error("Could not open " + filePath + " to use as shaders list");
		return true;
	}

	Program* lastProgram = 0;
	std::string lastProgramName = "";

	std::string line = "";
	while (!shadersList.eof())
	{
		getline(shadersList, line);

		if (line.length() < 1)
			continue;

		if (line.substr(0, 1) == "#")
			continue;

		size_t firstTab = line.find("\t");
		size_t secondTab = line.find("\t", firstTab + 1);

		/*
			Each line in shadersList.txt should look like this:
			program-name<tab>shader-type<tab>relative-file-path<end-line>
		*/
		if (firstTab == std::string::npos || secondTab == std::string::npos)
		{
			error("Malformed shadersList.txt line: " + line);
			continue;
		}

		std::string programName = line.substr(0, firstTab);
		std::string shaderType = line.substr(firstTab + 1, secondTab - (firstTab + 1));
		std::string filePath = line.substr(secondTab + 1, line.length() - (secondTab - 1));

		if (programName.length() < 1 || shaderType.length() < 1 || filePath.length() < 1)
		{
			error("Malformed shadersList.txt line: " + line);
			continue;
		}

		if (lastProgramName != programName)
		{
			if (lastProgram != 0)
			{
				//Compile shaders and set sampler2D uniforms
				lastProgram->compile();

				if (!lastProgram->isCompiled())
					returnValue = true;

				//Associate uniforms in UBO block GlobalUniforms with uniformBufferData struct memory
				bind(lastProgram);
			}

			lastProgram = new Program();
			lastProgramName = programName;

			//There are a limited amount of hard-coded shader programs, find the right one
			if (programName == "model")
				modelShader = lastProgram;
			else if (programName == "modelShadow")
				modelShadowShader = lastProgram;
			else if (programName == "brick")
				brickShader = lastProgram;
			else if (programName == "outline")
				outlineShader = lastProgram;
			else
				error("Invalid program name " + programName);
		}

		GLenum shaderEnum = 0;
		if (shaderType == "vert")
			shaderEnum = GL_VERTEX_SHADER;
		else if (shaderType == "frag")
			shaderEnum = GL_FRAGMENT_SHADER;
		else if (shaderType == "geom")
			shaderEnum = GL_GEOMETRY_SHADER;
		else if (shaderType == "tesc")
			shaderEnum = GL_TESS_CONTROL_SHADER;
		else if (shaderType == "tese")
			shaderEnum = GL_TESS_EVALUATION_SHADER;

		if (shaderEnum == 0)
		{
			error("Invalid shader type: " + shaderType);
			continue;
		}
		else
		{
			Shader* tmp = new Shader("Shaders/" + filePath, shaderEnum);
			lastProgram->bindShader(tmp);
		}
	}

	//We check to compile the previous program when we start adding shaders for another program
	//So we need this to compile the very last program in the file
	if (lastProgram && !lastProgram->isCompiled())
	{
		//Compile shaders and set sampler2D uniforms
		lastProgram->compile();

		if (!lastProgram->isCompiled())
			returnValue = true;

		//Associate uniforms in UBO block GlobalUniforms with uniformBufferData struct memory
		bind(lastProgram);
	}

	return returnValue;
}

ShaderManager::ShaderManager()
{
	scope("UniformManager::UniformManager");

	//Basic:

	glGenBuffers(1, &basicUBO);
	if (!basicUBO)
		error("Could not allocate uniform buffer object!");

	//Note, because of OpenGLs std140 padding, we cannot do sizeof(struct basicUniforms) there's basically no way but to manually calculate the padding size
	glBindBuffer(GL_UNIFORM_BUFFER, basicUBO);
	glBufferData(GL_UNIFORM_BUFFER, 224, &basicUniforms, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	//Camera:

	glGenBuffers(1, &cameraUBO);
	if (!cameraUBO)
		error("Could not allocate uniform buffer object!");

	glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
	glBufferData(GL_UNIFORM_BUFFER, 220, &cameraUniforms, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

ShaderManager::~ShaderManager()
{
	glDeleteBuffers(1, &basicUBO);
	glDeleteBuffers(1, &cameraUBO);
}

/*
	Associates contained UBOs with their definitions in the OpenGL shaders
	See function definition for what names they should have in shaders
*/
void ShaderManager::bind(Program* target) const
{
	target->bindUniformBlock("BasicUniforms" , basicUBO , 0);
	target->bindUniformBlock("CameraUniforms", cameraUBO, 1);
}

//Push camera changes to GPU/OpenGL
void ShaderManager::updateCameraUBO() const 
{
	glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
	glBufferData(GL_UNIFORM_BUFFER, 220, &cameraUniforms, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//Push camera changes to GPU/OpenGL
void ShaderManager::updateBasicUBO() const
{
	glBindBuffer(GL_UNIFORM_BUFFER, basicUBO);
	glBufferData(GL_UNIFORM_BUFFER, 224, &basicUniforms, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
 