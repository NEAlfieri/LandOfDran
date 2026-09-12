#include "LoopServer.h"

#include "../LuaFunctions/Dynamic.h"
#include "../LuaFunctions/Static.h"

void LoopServer::run(float deltaT, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	//When embedded alongside a LoopClient in the same process (single player), both loops
	//share this one static pointer. Reassert ours here since the client may have pointed it
	//at its own PhysicsWorld since our last tick.
	SimObject::world = pd.physicsWorld;

	if (deltaT > slowestTickMS)
		slowestTickMS = deltaT;

	totalTicks++;
	totalTicksMS += deltaT;

	if (SDL_GetTicks() - slowestTickCounter > 1000)
	{
		slowestTickCounter = SDL_GetTicks();

		float averageTickMS = totalTicksMS / totalTicks;

		ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(float) * 2, getFlagsFromChannel(OtherReliable));
		ret->data[0] = (unsigned char)ServerPerformanceDetails;
		memcpy(ret->data + 1, &lastSlowestTickMS, sizeof(float));
		memcpy(ret->data + 1 + sizeof(float), &averageTickMS, sizeof(float));

		server->broadcast(ret, OtherReliable);

		totalTicks = 0;
		totalTicksMS = 0;
		lastSlowestTickMS = slowestTickMS;
		slowestTickMS = 0;
	}

	server->run(&pd,pd.luaState,pd.eventManager); //   <---- networking
	pd.dynamics->sendRecent();
	pd.statics->sendRecent();
	pd.physicsWorld->step(deltaT); 

	for (unsigned int a = 0; a < Logger::getStorage()->size(); a++)
		server->updateAdminConsoles(Logger::getStorage()->at(a));
	Logger::getStorage()->clear();

	for (unsigned int a = 0; a < pd.clients.size(); a++)
	{
		for (unsigned int b = 0; b < pd.clients[a]->controllers.size(); b++)
		{
			pd.clients[a]->controllers[b].controlWithLastInput(pd.physicsWorld,deltaT);
		}
	}

	scheduler->run(pd.luaState);
}

LoopServer::LoopServer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	server = new Server(DEFAULT_PORT);
	LUA_server = server;
	if (!server->isValid())
		return;

	LUA_args = &cmdArgs;
	LUA_pd = &pd;

	pd.evalPassword = settings->getString("hosting/evalpassword");
	pd.useEvalPassword = settings->getBool("hosting/useevalpassword");
	if (pd.useEvalPassword && (pd.evalPassword == " " || pd.evalPassword == "changeme" || pd.evalPassword.length() < 1))
	{
		error("Eval password protection is enabled, but the password is still the default 'changeme' (or empty). Set a real password in Settings, under Hosting, then restart the server. Eval console logins are refused until then.");
		pd.useEvalPassword = false;
	}

	//Not a dedicated server means we're embedded in the graphical client (single player/"Start Server"), so the
	//only client that can reach us over loopback is our own host - let them straight into the eval console
	pd.autoAdminForLoopback = !cmdArgs.dedicated;
	//TODO: Hash password

	//Start up Lua and give it access to all the default libraries, file io, debugging, math, etc.
	pd.luaState = luaL_newstate();
	luaL_openlibs(pd.luaState);
	registerOtherFunctions(pd.luaState);
	scheduler = new LuaScheduler(pd.luaState);
	pd.eventManager = new EventManager(pd.luaState);
	registerClientFunctions(pd.luaState);

	///Server just has one physics world that's started when the program starts and stays until shutdown, unlike client
	pd.physicsWorld = std::make_shared<PhysicsWorld>();
	SimObject::world = pd.physicsWorld;

	pd.dynamics = new ObjHolder<Dynamic>(SimObjectType::DynamicTypeId, server);
	pd.dynamics->makeLuaMetatable(pd.luaState, "metatable_dynamic", getDynamicFunctions(pd.luaState));
	pd.statics = new ObjHolder<StaticObject>(SimObjectType::StaticTypeId, server);
	pd.statics->makeLuaMetatable(pd.luaState, "metatable_static", getStaticFunctions(pd.luaState));

	info("Loading serverstart.lua");

	if (luaL_dofile(pd.luaState, "serverstart.lua"))
	{
		error("Error loading serverstart.lua: " + std::string(lua_tostring(pd.luaState, -1)));

		//Only block on console input for a real standalone dedicated server.
		//When embedded in the graphical client (single player), there's no console to read from.
		if (cmdArgs.dedicated)
		{
			info("Input any text to exit.");
			std::string holdForAWhile;
			std::cin >> holdForAWhile;
		}

		valid = false;
		return;
	}

	info("Server running");

	if(pd.allNetTypes.size() == 0)
		error("No NetTypes registered, clients will freeze on joining, no objects can be created.");

	valid = true;
}

LoopServer::~LoopServer()
{
	delete pd.eventManager;
	delete scheduler;

	LUA_args = nullptr;
	LUA_pd = nullptr;

	pd.physicsWorld.reset();
	SimObject::world = nullptr;

	delete pd.dynamics;
	delete pd.statics;

	pd.dynamicTypes.clear();
	pd.allNetTypes.clear();

	delete server;
	LUA_server = nullptr;
}
