#include "RenderTarget.h"

void RenderTarget::useLayer(int layer)
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthResult->getHandle(), 0, layer);
	if (colorResult)
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorResult->getHandle(), 0, layer);
	glViewport(0, 0, settings.width, settings.height);
	glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, settings.clearColor.a);
	glClear(GL_DEPTH_BUFFER_BIT | (colorResult ? GL_COLOR_BUFFER_BIT : 0));
}

void RenderTarget::copyFromScreen() const
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer);
	glBlitFramebuffer(0, 0, settings.width, settings.height, 0, 0, settings.width, settings.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::use()
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glViewport(0, 0, settings.width, settings.height); 
	glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, settings.clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

RenderTarget::RenderTarget(const RenderTargetSettings& _settings,std::shared_ptr<TextureManager> textures)
{
	scope("RenderTarget::RenderTarget");
	debug("Setting up a render target");

	settings = _settings;

	if (!settings.useColor && !settings.useDepth)
	{
		error("Neither depth nor color selected for frame buffer!");
		return;
	}

	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

	if (settings.useColor)
	{
		colorResult = textures->createBlankTexture(settings.width, settings.height, settings.channels, settings.layers);
		colorResult->setFilter(settings.magFilter, settings.minFilter);
		//Shadow targets, whose filters shouldn't wrap around to the other side, see below
		if (settings.depthCompare)
			colorResult->setWrapping(GL_CLAMP_TO_EDGE);
		else if (settings.colorWrap != GL_REPEAT)
			colorResult->setWrapping(settings.colorWrap);
		colorResult->addToFramebuffer();
		drawBuffers = new GLenum[1];
		drawBuffers[0] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, drawBuffers);
	}
	else
		glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (settings.useDepth)
	{
		depthResult = textures->createBlankShadow32(settings.width, settings.height, settings.layers);
		depthResult->setFilter(settings.magFilter, settings.minFilter);
		if (settings.depthCompare)
		{
			//Filter samples reaching past the edge shouldn't wrap around to the other side
			depthResult->setWrapping(GL_CLAMP_TO_EDGE);
			GLenum type = (settings.layers == 1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;
			glBindTexture(type, depthResult->getHandle());
			glTexParameteri(type, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(type, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
			glBindTexture(type, 0);
		}
		depthResult->addToFramebuffer(GL_DEPTH_ATTACHMENT);
	}
	else if (settings.useDepthBuffer)
	{
		//Render buffers allow quicker write access to depth buffer if we don't need to read to it
		//We still want a depth buffer since we will do depth testing when rendering to this frame buffer
		glGenRenderbuffers(1, &renderBuffer);
		//A generated name isn't a render buffer object until it's bound, without this the storage call and the
		//attachment below both fail and the frame buffer comes back incomplete
		glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, settings.width, settings.height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		error("Error creating frame buffer!");
	else
		valid = true;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

RenderTarget::~RenderTarget()
{
	glDeleteFramebuffers(1, &frameBuffer);
	glDeleteRenderbuffers(1, &renderBuffer);

	if (colorResult)
		colorResult->markForCleanup();

	if (depthResult)
		depthResult->markForCleanup();

	if (drawBuffers)
		delete drawBuffers;
}