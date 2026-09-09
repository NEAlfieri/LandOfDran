#include "RenderContext.h"

bool RenderContext::bindImGui() const
{
	return ImGui_ImplSDL2_InitForOpenGL(window, context);
}

//OpenGL callback function
void GLAPIENTRY MessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	debug("OpenGL error, type: " + std::to_string(type) + " severity: " + std::to_string(severity) + " message: " + message);
}

//Sets whether to trap the mouse in the center of the screen and hide it, i.e. first person controls
void RenderContext::setMouseLock(bool locked)
{
	mouseLocked = locked;
	SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
}

//Swaps buffers each frame
void RenderContext::swap() const
{
	if(window)
		SDL_GL_SwapWindow(window);
}

//Sets the screen as the frame/render buffer to draw to
void RenderContext::select() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
}

//Clears the screen
void RenderContext::clear(float r, float g, float b, float a, bool depth) const
{
	glClearColor(r, g, b, a);
	if (depth)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear(GL_COLOR_BUFFER_BIT);
}

//Changes screen resolution
void RenderContext::setSize(unsigned int x, unsigned int y)
{
	//I really thought there would be more here
	//Viewport is handled in select()
	width = x;
	height = y;
}

RenderContext::RenderContext(std::shared_ptr<SettingManager> settings) 
	: context((SDL_GLContext)nullptr) // Exists to suppress warning, SDL_GL_CreateContext can return 0 anyway
{
	scope("RenderContext::RenderContext");

	debug("Creating window");

	width = settings->getInt("graphics/startresolutionx");
	if(width < 0 || width > 10000)
	{
		error("graphics/startresolutionx invalid value " + std::to_string(width));
		return;
	}

	height = settings->getInt("graphics/startresolutiony");
	if(height < 0 || height > 10000)
	{
		error("graphics/startresolutiony invalid value " + std::to_string(height));
		return;
	}

	int flag = settings->getBool("graphics/startfullscreen") ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE;

	int version = GAME_VERSION;
	std::string windowName = "Land of Dran v" + std::to_string(version);

	//Context version/profile (graphics/openglmajor, openglminor, compatibilityprofile) is
	//requested via SDL_GL_SetAttribute in GlobalStartup::globalStartup, which runs before
	//this constructor.
	window = SDL_CreateWindow(windowName.c_str(),SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | flag);
	if(!window)
	{
		error("Failed to create window, SDL_GetError: " + std::string(SDL_GetError()));
		return;
	}

	debug("Creating context");

	context = SDL_GL_CreateContext(window);
	if(!context)
	{
		error("Failed to create GL context, SDL_GetError: " + std::string(SDL_GetError()) + " glGetError: " + std::to_string(glGetError()));
		return;
	}

	//Logged unconditionally (not debug()) since a driver can silently hand back a much
	//older/different context than what was requested (e.g. software rendering fallback),
	//and this is the only direct way to see what we actually got.
	auto glStr = [](GLenum name) -> std::string
	{
		const GLubyte* str = glGetString(name);
		return str ? reinterpret_cast<const char*>(str) : "(null)";
	};
	info("OpenGL version: " + glStr(GL_VERSION) + " | vendor: " + glStr(GL_VENDOR) + " | renderer: " + glStr(GL_RENDERER));

	debug("Starting GLEW");

	glewExperimental = GL_TRUE; //Used to need this, might not be needed anymore
	//Must be called after window creation
	GLenum glewError = glewInit();

	if(glewError != GLEW_OK)
	{
		error("glewInit failed SDL_GetError: " + std::string(SDL_GetError()) + " glew error: " + std::to_string(glewError));
		return;
	}

	if(SDL_GL_SetSwapInterval(settings->getBool("graphics/usevsync")) != 0)
		error("SDL_GL_SetSwapInterval failed SDL_GetError: " + std::string(SDL_GetError()));

	if (settings->getBool("graphics/debug"))
	{
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(MessageCallback, 0);
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	bindImGui();

	valid = true;
}

RenderContext::~RenderContext()
{
	if(context)
		SDL_GL_DeleteContext(context);
	if(window)
		SDL_DestroyWindow(window);
}
