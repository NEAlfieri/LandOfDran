#include "Program.h"

void Program::registerSamplerUniforms()
{
    use();

    /*
        How I organize texture samplers *could* change in the future
        But changes aren't often, and every shader uses the same layout all of the time
        Even if a given shader doesn't necesairly use every sampler
    */
    if(glGetUniformLocation(handle, "PBRArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "PBRArray"),           PBRArray);

    if(glGetUniformLocation(handle, "DecalArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "DecalArray"),         DecalArray);

    if (glGetUniformLocation(handle, "ShadowArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "ShadowArray"),        ShadowArray);

    if (glGetUniformLocation(handle, "TintDepthArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "TintDepthArray"), TintDepthArray);

    if (glGetUniformLocation(handle, "TintColorArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "TintColorArray"), TintColorArray);

    if (glGetUniformLocation(handle, "Refraction") != -1)
        glUniform1i(glGetUniformLocation(handle, "Refraction"), Refraction);

    if (glGetUniformLocation(handle, "Reflection") != -1)
        glUniform1i(glGetUniformLocation(handle, "Reflection"), Reflection);

    if (glGetUniformLocation(handle, "PointShadowArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "PointShadowArray"), PointShadowArray);

    if (glGetUniformLocation(handle, "ScreenCopy") != -1)
        glUniform1i(glGetUniformLocation(handle, "ScreenCopy"), ScreenCopy);

    if (glGetUniformLocation(handle, "ParticleTexture") != -1)
        glUniform1i(glGetUniformLocation(handle, "ParticleTexture"), ParticleTexture);

    if (glGetUniformLocation(handle, "PointTintDepthArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "PointTintDepthArray"), PointTintDepthArray);

    if (glGetUniformLocation(handle, "PointTintColorArray") != -1)
        glUniform1i(glGetUniformLocation(handle, "PointTintColorArray"), PointTintColorArray);

    if (glGetUniformLocation(handle, "SkyDay") != -1)
        glUniform1i(glGetUniformLocation(handle, "SkyDay"), SkyDay);

    if (glGetUniformLocation(handle, "SkyNight") != -1)
        glUniform1i(glGetUniformLocation(handle, "SkyNight"), SkyNight);

    if (glGetUniformLocation(handle, "RainMap") != -1)
        glUniform1i(glGetUniformLocation(handle, "RainMap"), RainMap);

    if (glGetUniformLocation(handle, "GodRayMask") != -1)
        glUniform1i(glGetUniformLocation(handle, "GodRayMask"), GodRayMask);

    //Enable others as they are actually added
    /*glUniform1i(glGetUniformLocation(handle, "BRDF"), BRDF);
    glUniform1i(glGetUniformLocation(handle, "HeightMap"),          HeightMap);
    glUniform1i(glGetUniformLocation(handle, "ShadowNearMap"),      ShadowNearMap);
    glUniform1i(glGetUniformLocation(handle, "ShadowFarMap"),       ShadowFarMap);
    glUniform1i(glGetUniformLocation(handle, "ShadowColorMap"),     ShadowColorMap);
    glUniform1i(glGetUniformLocation(handle, "ShadowNearTransMap"), ShadowNearTransMap);
    glUniform1i(glGetUniformLocation(handle, "CubeMapEnvironment"), CubeMapEnvironment);
    glUniform1i(glGetUniformLocation(handle, "CubeMapRadiance"),    CubeMapRadiance);
    glUniform1i(glGetUniformLocation(handle, "CubeMapIrradiance"),  CubeMapIrradiance);*/
}

void Program::bindUniformBlock(const std::string &glslName, GLuint UBOhandle, int index) const
{
    //Not every program declares every block, e.g. shadow and outline shaders have no EnvironmentUniforms
    GLuint blockIndex = glGetUniformBlockIndex(handle, glslName.c_str());
    if (blockIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(handle, blockIndex, index);
    glBindBufferBase(GL_UNIFORM_BUFFER, index, UBOhandle);
}

//Passes an array of matricies to a shader, was origionally used for skeletal animation
void UniformMatArray(std::string location, Program* prog, glm::mat4* array, unsigned int size)
{
    for (unsigned int a = 0; a < size; a++)
    {
        GLint loc = prog->getUniformLocation(location + "[" + std::to_string(a) + "]");
        if (loc == -1)
            continue;
        glm::mat4 tmp = array[a];
        glUniformMatrix4fv(loc, 1, GL_FALSE, &tmp[0][0]);
    }
}

void UniformMat(GLint location, glm::mat4 mat)
{
    glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
}

void UniformVec3(GLint location, glm::vec3 vec)
{
    glUniform3f(location, vec.x, vec.y, vec.z);
}

void UniformVec4(GLint location, glm::vec4 vec)
{
    glUniform4f(location, vec.r, vec.g, vec.b, vec.a);
}

GLint Program::registerUniformVec3(std::string name, bool useDefault, glm::vec3 defaul)
{
    GLint uniform = getUniformLocation(name);
    uniforms.push_back(uniform);
    useDefaults.push_back(useDefault);
    defaultVec3s.push_back(defaul);
    defaultTypes.push_back(IsVec3);
    defaultMats.push_back(glm::mat4(1.0));
    defaultInts.push_back(0);
    defaultFloats.push_back(0);
    return uniform;
}

GLint Program::registerUniformMat(std::string name, bool useDefault, glm::mat4 defaul)
{
    GLint uniform = getUniformLocation(name);
    uniforms.push_back(uniform);
    defaultMats.push_back(defaul);
    useDefaults.push_back(useDefault);
    defaultTypes.push_back(IsMatrix);
    defaultInts.push_back(0);
    defaultFloats.push_back(0);
    defaultVec3s.push_back(glm::vec3(0, 0, 0));
    return uniform;
}

GLint Program::registerUniformBool(std::string name, bool useDefault, bool defaul)
{
    return registerUniformInt(name, useDefault, defaul);
}

GLint Program::registerUniformInt(std::string name, bool useDefault, int   defaul)
{
    GLint uniform = getUniformLocation(name);

    uniforms.push_back(uniform);

    defaultInts.push_back(defaul);
    useDefaults.push_back(useDefault);
    defaultTypes.push_back(IsInt);
    defaultMats.push_back(glm::mat4(1.0));
    defaultFloats.push_back(0);
    defaultVec3s.push_back(glm::vec3(0, 0, 0));

    return uniform;
}

GLint Program::registerUniformFloat(std::string name, bool useDefault, float defaul)
{
    scope("program::registerUniformFloat");
    GLint uniform = getUniformLocation(name);

    uniforms.push_back(uniform);
    defaultFloats.push_back(defaul);
    useDefaults.push_back(useDefault);
    defaultTypes.push_back(IsFloat);
    defaultMats.push_back(glm::mat4(1.0));
    defaultInts.push_back(0);
    defaultVec3s.push_back(glm::vec3(0, 0, 0));

    return uniform;
}

void Program::bindShader(Shader* toBind)
{
    scope("program::bindShader");

    switch (toBind->shaderType)
    {
    case GL_VERTEX_SHADER:
        vertex = toBind;
        break;
    case GL_FRAGMENT_SHADER:
        fragment = toBind;
        break;
    case GL_TESS_CONTROL_SHADER:
        tessControl = toBind;
        break;
    case GL_TESS_EVALUATION_SHADER:
        tessEval = toBind;
        break;
    case GL_GEOMETRY_SHADER:
        geometry = toBind;
        break;

    default:
        error("Invalid shader type!");
        return;
    }
}

void Program::compile()
{
    scope("program::compile");

    debug("Creating program!");

    handle = glCreateProgram();
    if (handle == 0)
    {
        error("glCreateProgram failed: " + std::to_string(glGetError()));
        return;
    }

    if (vertex)
        glAttachShader(handle, vertex->handle);
    if (fragment)
        glAttachShader(handle, fragment->handle);
    if (tessEval)
        glAttachShader(handle, tessEval->handle);
    if (tessControl)
        glAttachShader(handle, tessControl->handle);
    if (geometry)
        glAttachShader(handle, geometry->handle);

    glLinkProgram(handle);
    GLint programSuccess = GL_FALSE;
    glGetProgramiv(handle, GL_LINK_STATUS, &programSuccess);

    if (programSuccess != GL_TRUE)
    {
        error("Could not create program! ");

        int length, actualLength;
        glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &length);
        char* data = new char[length];
        glGetProgramInfoLog(handle, length, &actualLength, data);

        if (length < 1)
            error("Could not get error log for program compilation.");
        else
        {
            std::ofstream programLog("Logs/shaderProgram.log");

            if (programLog.is_open())
            {
                programLog << data;
                error(data);
                programLog.close();
            }
            else
                error("Could not open Logs/shaderProgram.log for write!");
        }

        delete[] data;

        return;
    }

    info("Program created succesfully.");

    valid = true;

    registerSamplerUniforms();
}

void Program::resetUniforms() const
{
    for (unsigned int a = 0; a < uniforms.size(); a++)
    {
        if (useDefaults[a])
        {
            if (uniforms[a] != -1)
            {
                switch (defaultTypes[a])
                {
                case IsMatrix:
                    glUniformMatrix4fv(uniforms[a], 1, GL_FALSE, &defaultMats.at(a)[0][0]);
                    break;
                case IsFloat:
                    glUniform1f(uniforms[a], defaultFloats[a]);
                    break;
                case IsInt:
                    glUniform1i(uniforms[a], defaultInts[a]);
                    break;
                case IsVec3:
                    glUniform3f(uniforms[a], defaultVec3s[a].x, defaultVec3s[a].y, defaultVec3s[a].z);
                    break;
                }
            }
        } 
    }
}

void Program::use(bool reset) const
{
    if (!valid)
    {
        //error("Cannot use uncompiled shader program!");
        return;
    }
    glUseProgram(handle);
    if (reset)
        resetUniforms();
}

GLint Program::getUniformLocation(const std::string &name) const
{
    scope("program::getUniformLocation");
    GLint ret = glGetUniformLocation(handle, name.c_str());
    //if(ret == -1)
      //  error(name + " not found, or probably just not used in the shader anywhere!");
    return ret;
}
