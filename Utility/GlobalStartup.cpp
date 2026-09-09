#include "GlobalStartup.h"

//What time the application started
std::chrono::time_point<std::chrono::high_resolution_clock> start;

float getTicksMS()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
}

//Calls functions like SDL_Init and such that don't return anything (important)
bool globalStartup(std::shared_ptr<SettingManager> settings,const ExecutableArguments &cmdArgs)
{
    start = std::chrono::high_resolution_clock::now();

   info("Starting Enet");
   if (enet_initialize() != 0)
   {
        error("Could not start up Enet");
        return true;
   }

   //No graphics or UI if running headless
   if (cmdArgs.dedicated)
       return false; //Everything's okay so far!

	info("Starting SDL");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
	{
		error("Could not start SDL, SDL_GetError: " + std::string(SDL_GetError()));
		return true;
	}

    debug("Setting SDL-GL attributes");

    if (SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) != 0)
        error("Setting attribute SDL_GL_DEPTH_SIZE failed: " + std::string(SDL_GetError()));

    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, settings->getInt("graphics/openglmajor")) != 0)
        error("Setting attribute SDL_GL_CONTEXT_MAJOR_VERSION failed: " + std::string(SDL_GetError()));

    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, settings->getInt("graphics/openglminor")) != 0)
        error("Setting attribute SDL_GL_CONTEXT_MINOR_VERSION failed: " + std::string(SDL_GetError()));

    if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, settings->getInt("graphics/mutlisamplebuffers")) != 0)
        error("Setting attribute SDL_GL_MULTISAMPLEBUFFERS failed: " + std::string(SDL_GetError()));

    if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, settings->getInt("graphics/multisamplesamples")) != 0)
        error("Setting attribute SDL_GL_MULTISAMPLESAMPLES failed: " + std::string(SDL_GetError()));
    
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        settings->getBool("graphics/compatibilityprofile") ? SDL_GL_CONTEXT_PROFILE_COMPATIBILITY : SDL_GL_CONTEXT_PROFILE_CORE
    ) != 0)
    {
        error("Setting attribute SDL_GL_CONTEXT_PROFILE_MASK failed: " + std::string(SDL_GetError()));
    }

    if (settings->getBool("graphics/debug"))
    {
        if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG) != 0)
            error("SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_DEBUG_FLAG) failed");
    }

    debug("Starting Imgui");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

	return false;
}

//Calls shutdown for global start-up functions called in globalStartup
void globalShutdown(const ExecutableArguments& cmdArgs)
{
    info("Shutting down Enet");
    enet_deinitialize();

    //No graphics or UI if running headless
    if (cmdArgs.dedicated)
        return;

    info("Shutting down Imgui");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

	info("Shutting down SDL");
	SDL_Quit();
}
