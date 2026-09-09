// LandOfDran.cpp : Defines the entry point for the application.

#include "LandOfDran.h"
#include "GameLoop/LoopClient.h"
#include "GameLoop/LoopServer.h"
#include "Utility/SettingManager.h"
#include "Utility/DefaultPreferences.h"
#include "Utility/GlobalStartup.h"
#include <thread>

int main(int argc, char* argv[])
{
	//This holds everything we need to *play* the game instead of hosting it
	//is nullptr if dedicated server
	LoopClient* loopClient = nullptr;

	//Everything needed to *host* the game instead of playing it
	//is nullptr if not dedicated server
	LoopServer* loopServer = nullptr;

	Logger::setErrorFile("Logs/error.txt");
	Logger::setInfoFile("Logs/log.txt");
	info("Starting Land of Dran");

	//Parse command line arguments to see if we're hosting dedicated or launching the full game
	ExecutableArguments cmdArgs(argc, argv);

	//Import settings we have, set defaults for settings we don't, then export the file with any new default values
	info("Opening settings file");
	auto settings = std::make_shared<SettingManager>("Config/settings.txt");
	populateDefaults(settings);
	settings->exportToFile("Config/settings.txt");

	Logger::setDebug(settings->getBool("logger/verbose"));
	
	//Start SDL and other libraries
	globalStartup(settings,cmdArgs);

	//Start up for dedicated server or the actual game itself
	if (cmdArgs.dedicated)
	{
		info("Dedicated flag detected, starting server.");

		loopServer = new LoopServer(cmdArgs, settings);
		if (!loopServer->isValid())
			return 0;
	}
	else 
	{
		loopClient = new LoopClient(cmdArgs, settings);
		if (!loopClient->isValid())
			return 0;
	}

	auto lastTime = std::chrono::high_resolution_clock::now();
	
	//The main loop of the whole program
	while (cmdArgs.mainLoopRun)
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double,std::milli> duration = currentTime - lastTime;
		//Milliseconds since last frame
		float deltaT = duration.count();
		lastTime = currentTime;

		auto frameStart = std::chrono::high_resolution_clock::now();

		//Run server, or run game
		if (cmdArgs.dedicated)
			loopServer->run(deltaT,cmdArgs,settings);
		else
			loopClient->run(deltaT,cmdArgs,settings);

		auto frameEnd = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double,std::milli> frameDuration = frameEnd - frameStart;

		if (cmdArgs.dedicated)
		{
			//Keeps the server ticking at a steady SERVER_TICK_MS instead of free-spinning as fast as the OS scheduler allows,
			//which is important since PhysicsWorld::step relies on the caller not passing wildly inconsistent deltaT values
			if (frameDuration.count() < SERVER_TICK_MS)
			{
				//Busy wait is far more accurate than this_thread::sleep_for, which may oversleep by several ms on some platforms
				//yield() still lets other threads/processes run between clock checks, unlike a plain spin loop
				while (std::chrono::high_resolution_clock::now() < frameStart + std::chrono::duration<double, std::milli>(SERVER_TICK_MS))
					std::this_thread::yield();
			}
		}
		else
			SDL_Delay(1);
	}

	//Deallocate client if this wasn't a dedicated server
	if (cmdArgs.dedicated)
		delete loopServer;
	else
		delete loopClient;

	globalShutdown(cmdArgs);
	
	return 0;
}
