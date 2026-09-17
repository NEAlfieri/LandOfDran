#pragma once

#include "Texture.h"

class RenderTarget
{
	GLuint frameBuffer = 0;
	GLuint renderBuffer = 0;
	GLenum* drawBuffers = nullptr;
	Texture* colorResult = nullptr;
	Texture* depthResult = nullptr;

	bool valid = false;

	public:

	bool isValid() const { return valid;  }

	struct RenderTargetSettings
	{
		int width = 800;
		int height = 800;
		int channels = 3;
		int layers = 1;
		bool useColor = true;
		bool useDepth = true;
		glm::vec4 clearColor = glm::vec4(0, 0, 0, 0);
		GLenum minFilter = GL_LINEAR;
		GLenum magFilter = GL_LINEAR;
		//Depth result is read with a sampler2DShadow / sampler2DArrayShadow, which filters comparisons instead of depths
		bool depthCompare = false;
		//Without a depth result there's still a depth render buffer to depth test against, unless this is off
		bool useDepthBuffer = true;
		//How the color result reads outside itself. GL_CLAMP_TO_BORDER reads black, which a pass sampling
		//past the edge of the screen wants, see the god ray mask in LoopClient::renderGodRays
		GLenum colorWrap = GL_REPEAT;
	} settings;

	//What was drawn into it, to show somewhere that takes a plain texture, like an ImGui image. Its rows run
	//bottom to top the way OpenGL fills them, so anything expecting the top row first has to flip its V coordinate
	Texture* getColorResult() const { return colorResult; }

	void bindDepthResult(TextureLocations loc) const { if (!depthResult) return; depthResult->bind(loc); }
	void bindColorResult(TextureLocations loc) const { if (!colorResult) return; colorResult->bind(loc); }

	RenderTarget(const RenderTargetSettings& _settings, std::shared_ptr<TextureManager> textures);
	~RenderTarget();

	void use();

	//Renders into just one layer of a layered depth result, clearing only that layer
	void useLayer(int layer);

	//Copies what's on the screen so far into the color result, resolving any multisampling, the target has to be the size of the screen
	//Leaves the screen selected afterward
	void copyFromScreen() const;
};

