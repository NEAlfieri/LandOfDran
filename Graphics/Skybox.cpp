#include "Skybox.h"
#include "../Utility/ContentFiles.h"

#include "../External/stb_image.h"
#include "../Utility/FileFunctions.h"

#include <glm/gtc/constants.hpp>

//Direction through a point on an OpenGL cube map face, s and t from -1 to 1 along its first column and row, see the cube map table in the OpenGL spec
//Keep in sync with faceDirection in skyPrefilter.frag
static glm::vec3 cubeFaceDirection(int face, float s, float t)
{
	switch (face)
	{
		case 0: return glm::vec3(1, -t, -s);
		case 1: return glm::vec3(-1, -t, s);
		case 2: return glm::vec3(s, 1, t);
		case 3: return glm::vec3(s, -1, -t);
		case 4: return glm::vec3(s, -t, 1);
		default: return glm::vec3(-s, -t, -1);
	}
}

namespace
{
	//An RGB image as floats, top row first
	struct FloatImage
	{
		int width = 0;
		int height = 0;
		std::vector<float> pixels;

		bool load(const std::string& wantedPath)
		{
			//A skybox image a server sent us is loaded out of the download folder, see Utility/ContentFiles.h
			const std::string path = contentPath(wantedPath);

			int channels = 0;
			float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 3);
			if (!data)
				return false;

			//stbi_loadf turns screen colors into linear ones, the old game's skybox images were drawn as is
			if (!stbi_is_hdr(path.c_str()))
			{
				stbi_image_free(data);
				stbi_uc* bytes = stbi_load(path.c_str(), &width, &height, &channels, 3);
				if (!bytes)
					return false;
				pixels.resize((size_t)width * height * 3);
				for (size_t a = 0; a < pixels.size(); a++)
					pixels[a] = bytes[a] / 255.0f;
				stbi_image_free(bytes);
				return true;
			}

			pixels.assign(data, data + (size_t)width * height * 3);
			stbi_image_free(data);
			return true;
		}

		glm::vec3 texel(int x, int y, bool wrapX) const
		{
			x = wrapX ? ((x % width) + width) % width : std::clamp(x, 0, width - 1);
			y = std::clamp(y, 0, height - 1);
			const float* p = &pixels[3 * ((size_t)y * width + x)];
			return glm::vec3(p[0], p[1], p[2]);
		}

		//u and v from 0 to 1, v = 0 is the top row
		glm::vec3 sample(float u, float v, bool wrapX) const
		{
			float x = u * width - 0.5f;
			float y = v * height - 0.5f;
			int x0 = (int)std::floor(x);
			int y0 = (int)std::floor(y);
			float fx = x - x0;
			float fy = y - y0;
			return glm::mix(glm::mix(texel(x0, y0, wrapX), texel(x0 + 1, y0, wrapX), fx), glm::mix(texel(x0, y0 + 1, wrapX), texel(x0 + 1, y0 + 1, wrapX), fx), fy);
		}
	};
}

void Skybox::clearSlot(Slot& slot)
{
	if (slot.cubeMap)
		glDeleteTextures(1, &slot.cubeMap);
	slot = Slot();
	for (glm::vec3& coefficient : slot.irradiance)
		coefficient = glm::vec3(0);
}

void Skybox::loadSlot(Slot& slot, const std::string& path, bool imageBasedLighting)
{
	scope("Skybox::loadSlot");

	clearSlot(slot);
	if (path.empty())
		return;

	if (!isPathInsideGameFolder(path))
	{
		error("Server sent a skybox path outside the game folder: " + path);
		return;
	}

	Uint32 startMS = SDL_GetTicks();

	//Every face in the order OpenGL numbers them, RGB top row first
	std::vector<float> faces[6];
	int size = 0;
	bool hdr = lowercase(std::filesystem::path(path).extension().string()) == ".hdr";

	auto fillFaces = [&faces, &size](const std::function<glm::vec3(const glm::vec3&)>& directionColor)
	{
		for (int face = 0; face < 6; face++)
		{
			faces[face].resize((size_t)size * size * 3);
			for (int t = 0; t < size; t++)
			{
				for (int s = 0; s < size; s++)
				{
					glm::vec3 direction = glm::normalize(cubeFaceDirection(face, (s + 0.5f) / size * 2.0f - 1.0f, (t + 0.5f) / size * 2.0f - 1.0f));
					glm::vec3 color = directionColor(direction);
					memcpy(&faces[face][3 * ((size_t)t * size + s)], &color[0], sizeof(float) * 3);
				}
			}
		}
	};

	if (hdr)
	{
		FloatImage image;
		if (!image.load(path))
		{
			error("Could not load skybox " + path);
			return;
		}

		//Equirectangular, straight up is the top row, the same way the old game's rectToCube.frag read it
		size = hdrFaceSize;
		fillFaces([&image](const glm::vec3& direction)
		{
			float u = std::atan2(direction.z, direction.x) / glm::two_pi<float>() + 0.5f;
			float v = 0.5f - std::asin(std::clamp(direction.y, -1.0f, 1.0f)) / glm::pi<float>();
			return image.sample(u, v, true);
		});
	}
	else
	{
		//path_0.png top, _1 +x, _2 -x, _3 +z, _4 -z, and _5 the bottom if there is one, else the top again like in the old game
		FloatImage images[6];
		for (int a = 0; a < 5; a++)
		{
			std::string facePath = path + "_" + std::to_string(a) + ".png";
			if (!images[a].load(facePath))
			{
				error("Could not load skybox face " + facePath);
				return;
			}
		}
		bool hasBottom = std::filesystem::exists(path + "_5.png") && images[5].load(path + "_5.png");

		size = std::clamp(images[0].width, 1, 2048);
		fillFaces([&images, hasBottom](const glm::vec3& direction)
		{
			//Same as the texture coordinates of the old game's skybox cube, see OldClient/graphics/environment.cpp
			glm::vec3 across = glm::abs(direction);
			if (across.y >= across.x && across.y >= across.z)
			{
				glm::vec3 onFace = direction / across.y;
				return images[direction.y < 0 && hasBottom ? 5 : 0].sample((onFace.z + 1.0f) * 0.5f, (onFace.x + 1.0f) * 0.5f, false);
			}
			if (across.x >= across.z)
			{
				glm::vec3 onFace = direction / across.x;
				return images[direction.x > 0 ? 1 : 2].sample((onFace.y + 1.0f) * 0.5f, (onFace.z + 1.0f) * 0.5f, false);
			}
			glm::vec3 onFace = direction / across.z;
			return images[direction.z > 0 ? 3 : 4].sample((onFace.x + 1.0f) * 0.5f, (onFace.y + 1.0f) * 0.5f, false);
		});
	}

	slot.kind = hdr ? SkyboxHDR : SkyboxImages;
	slot.lit = hdr && imageBasedLighting && shaders->skyPrefilterShader->isCompiled();

	glGenTextures(1, &slot.cubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, slot.cubeMap);
	for (int face = 0; face < 6; face++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, hdr ? GL_RGBA16F : GL_RGB8, size, size, 0, GL_RGB, GL_FLOAT, faces[face].data());
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);

	if (!slot.lit)
	{
		//Only ever drawn at full size
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		info("Loaded skybox " + path + " in " + std::to_string(SDL_GetTicks() - startMS) + " ms");
		return;
	}

	for (int level = 1; level <= reflectionLevels; level++)
		for (int face = 0; face < 6; face++)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, GL_RGBA16F, size >> level, size >> level, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, reflectionLevels);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	//Lighting only sees the clamped sky, see lightingClamp
	for (std::vector<float>& face : faces)
	{
		for (size_t a = 0; a < face.size(); a += 3)
		{
			float brightest = std::max(face[a], std::max(face[a + 1], face[a + 2]));
			if (brightest > lightingClamp)
			{
				float scale = lightingClamp / brightest;
				face[a] *= scale;
				face[a + 1] *= scale;
				face[a + 2] *= scale;
			}
		}
	}

	//Irradiance as 9 spherical harmonics, projected from every texel weighted by the solid angle it covers
	//See Ramamoorthi and Hanrahan, An Efficient Representation for Irradiance Environment Maps
	static constexpr float basisScale[9] = { 0.282095f, 0.488603f, 0.488603f, 0.488603f, 1.092548f, 1.092548f, 0.315392f, 1.092548f, 0.546274f };
	const float convolution[9] = { glm::pi<float>(), glm::two_pi<float>() / 3.0f, glm::two_pi<float>() / 3.0f, glm::two_pi<float>() / 3.0f,
		glm::quarter_pi<float>(), glm::quarter_pi<float>(), glm::quarter_pi<float>(), glm::quarter_pi<float>(), glm::quarter_pi<float>() };
	glm::dvec3 projected[9];
	for (glm::dvec3& coefficient : projected)
		coefficient = glm::dvec3(0);
	for (int face = 0; face < 6; face++)
	{
		for (int t = 0; t < size; t++)
		{
			for (int s = 0; s < size; s++)
			{
				float faceS = (s + 0.5f) / size * 2.0f - 1.0f;
				float faceT = (t + 0.5f) / size * 2.0f - 1.0f;
				glm::vec3 d = glm::normalize(cubeFaceDirection(face, faceS, faceT));
				double solidAngle = 4.0 / ((double)size * size * std::pow(1.0 + faceS * faceS + faceT * faceT, 1.5));
				const float* p = &faces[face][3 * ((size_t)t * size + s)];
				glm::dvec3 radiance = glm::dvec3(p[0], p[1], p[2]) * solidAngle;

				//Keep in sync with skyIrradiance in model.frag
				const float basis[9] = { 1.0f, d.y, d.z, d.x, d.x * d.y, d.y * d.z, 3.0f * d.z * d.z - 1.0f, d.x * d.z, d.x * d.x - d.y * d.y };
				for (int a = 0; a < 9; a++)
					projected[a] += radiance * (double)(basisScale[a] * basis[a]);
			}
		}
	}
	for (int a = 0; a < 9; a++)
		slot.irradiance[a] = glm::vec3(projected[a]) * basisScale[a] * convolution[a];

	//A full chain of plain mip levels to blur from, see skyPrefilter.frag
	GLuint source = 0;
	glGenTextures(1, &source);
	glBindTexture(GL_TEXTURE_CUBE_MAP, source);
	for (int face = 0; face < 6; face++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, size, size, 0, GL_RGB, GL_FLOAT, faces[face].data());
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	prefilter(source, slot.cubeMap);
	glDeleteTextures(1, &source);

	info("Loaded skybox " + path + " with image based lighting in " + std::to_string(SDL_GetTicks() - startMS) + " ms");
}

void Skybox::prefilter(GLuint source, GLuint target)
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	GLint framebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
	bool depthTest = glIsEnabled(GL_DEPTH_TEST);
	bool cullFace = glIsEnabled(GL_CULL_FACE);
	bool blend = glIsEnabled(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	Program* program = shaders->skyPrefilterShader;
	program->use();
	glActiveTexture(GL_TEXTURE0 + SkyDay);
	glBindTexture(GL_TEXTURE_CUBE_MAP, source);
	glUniform1i(program->getUniformLocation("source"), SkyDay);
	glUniform1f(program->getUniformLocation("sourceSize"), (float)hdrFaceSize);
	GLint roughnessUniform = program->getUniformLocation("roughness");
	GLint faceUniform = program->getUniformLocation("face");

	//skyPrefilter.vert makes a fullscreen triangle from gl_VertexID, but core profile still needs a VAO bound
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	for (int level = 1; level <= reflectionLevels; level++)
	{
		int levelSize = std::max(1, hdrFaceSize >> level);
		glViewport(0, 0, levelSize, levelSize);
		glUniform1f(roughnessUniform, (float)level / reflectionLevels);

		for (int face = 0; face < 6; face++)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, target, level);
			if (level == 1 && face == 0 && glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				error("Couldn't render into a skybox's reflection levels, reflections will be black");
			glUniform1i(faceUniform, face);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glDeleteFramebuffers(1, &fbo);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glActiveTexture(GL_TEXTURE0);

	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	if (depthTest)
		glEnable(GL_DEPTH_TEST);
	if (cullFace)
		glEnable(GL_CULL_FACE);
	if (blend)
		glEnable(GL_BLEND);
}

void Skybox::update(const std::string& dayPath, const std::string& nightPath, bool imageBasedLighting)
{
	bool lightingChanged = imageBasedLighting != loadedWithLighting;
	loadedWithLighting = imageBasedLighting;

	const std::string newPaths[2] = { dayPath, nightPath };
	for (int a = 0; a < 2; a++)
	{
		if (newPaths[a] == paths[a] && !(lightingChanged && slots[a].kind == SkyboxHDR))
			continue;

		paths[a] = newPaths[a];
		loadSlot(slots[a], paths[a], imageBasedLighting);
	}
}

void Skybox::bind() const
{
	glActiveTexture(GL_TEXTURE0 + SkyDay);
	glBindTexture(GL_TEXTURE_CUBE_MAP, slots[0].cubeMap ? slots[0].cubeMap : emptyCubeMap);
	glActiveTexture(GL_TEXTURE0 + SkyNight);
	glBindTexture(GL_TEXTURE_CUBE_MAP, slots[1].cubeMap ? slots[1].cubeMap : emptyCubeMap);
	//So the next texture loaded doesn't replace either
	glActiveTexture(GL_TEXTURE0);
}

void Skybox::passUniforms(std::shared_ptr<ShaderManager> shaders, float nightAmount) const
{
	SkyUniforms& uniforms = shaders->skyUniforms;
	for (int a = 0; a < 9; a++)
	{
		uniforms.SkyIrradiance[a] = glm::vec4(slots[0].irradiance[a], 0);
		uniforms.SkyIrradiance[9 + a] = glm::vec4(slots[1].irradiance[a], 0);
	}
	uniforms.SkyboxBlend = nightAmount;
	uniforms.DaySkybox = slots[0].kind;
	uniforms.NightSkybox = slots[1].kind;
	uniforms.SkyLightDay = slots[0].lit ? 1.0f - nightAmount : 0.0f;
	uniforms.SkyLightNight = slots[1].lit ? nightAmount : 0.0f;
	uniforms.SkyReflectionLevels = (float)reflectionLevels;
	shaders->updateSkyUBO();
}

Skybox::Skybox(std::shared_ptr<ShaderManager> _shaders)
{
	shaders = _shaders;

	//Filters across the edges of cube map faces, so blurry reflections have no seams
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	for (Slot& slot : slots)
		clearSlot(slot);

	const float black[3] = { 0, 0, 0 };
	glGenTextures(1, &emptyCubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, emptyCubeMap);
	for (int face = 0; face < 6; face++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_FLOAT, black);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	passUniforms(shaders, 0.0f);
}

Skybox::~Skybox()
{
	for (Slot& slot : slots)
		clearSlot(slot);
	glDeleteTextures(1, &emptyCubeMap);
}
