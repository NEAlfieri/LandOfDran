#include "LoopClient.h"

void LoopClient::leaveServer(ExecutableArguments& cmdArgs)
{
	info("Leaving server");

	pd.serverBrowser->open();

	if (!client)
		return;

	pd.chatWindow->close();

	//Will need to log in again to get eval access
	pd.debugMenu->reset();

	simulation.controllers.clear();
	simulation.controlledDynamics.clear();

	if (simulation.dynamics)
	{
		delete simulation.dynamics;
		simulation.dynamics = nullptr;
	}

	if (simulation.statics)
	{
		delete simulation.statics;
		simulation.statics = nullptr;
	}

	delete client;
	client = nullptr;

	//It's very possible some or all data structures may not have been initialized or allocated if we disconnected in the middle loading into a new server

	simulation.dynamicTypes.clear();
	pd.signals.typesToLoad = 0; //Disable progress bar in server browser UI until next join
	
	simulation.evalPassword = "";

	//Destroy server specific physics
	if (pd.physicsWorld)
	{
		pd.physicsWorld.reset();
		SimObject::world = nullptr;
	}

	pd.context->setMouseLock(false);

	cmdArgs.gameState = NotInGame;

	//Shut down our embedded single player server, if there was one
	if (localServer)
	{
		delete localServer;
		localServer = nullptr;
	}
}

void LoopClient::connectToServer(std::string ip, unsigned int port, std::string userName, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	if (cmdArgs.gameState != NotInGame)
		leaveServer(cmdArgs);

	//TODO: Note without some kind of multithreading, this message will never display in the UI
	pd.serverBrowser->setConnectionNote("Connecting to server...");
	info("Attempting connection to " + ip + ":" + std::to_string(port));

	//TODO: Make it so I can do this without having to re-write descriptions
	settings->addString("network/username", userName, true, "Guest name if not logged in");
	settings->addString("network/lastip", ip, true, "Last IP connected to");
	settings->addInt("network/port", port, true, "Connection port");
	settings->exportToFile("Config/settings.txt");

	cmdArgs.gameState = Connecting;

	//If we're hosting our own local server, it needs to service its ENet host while
	//we wait here for the handshake to complete - nothing else is ticking it right now.
	std::function<void()> pump = nullptr;
	if (localServer)
		pump = [this, &cmdArgs, settings]() { localServer->run(0.f, cmdArgs, settings); };

	client = new Client(ip, port, settings->getInt("network/packetholdtime"), pump);

	//Connection to server failed
	if (!client->isValid())
	{
		pd.serverBrowser->setConnectionNote("Could not connect");
		cmdArgs.gameState = NotInGame;
		delete client;
		client = nullptr;
		return;
	}

	client->send(makeConnectionRequest(userName), JoinNegotiation);

	//From here, further initalization will actually take place in Networking/PacketsFromServer/AcceptConnection.cpp
	//Assuming the server lets us join, of course
}

void LoopClient::hostSinglePlayer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	if (cmdArgs.gameState != NotInGame)
		leaveServer(cmdArgs);

	info("Starting server");

	localServer = new LoopServer(cmdArgs, settings);
	if (!localServer->isValid())
	{
		pd.serverBrowser->setConnectionNote("Could not start local server, see error log.");
		delete localServer;
		localServer = nullptr;
		return;
	}

	std::string userName = settings->getString("network/username");
	if (userName.length() < 1)
		userName = "Player";

	connectToServer("127.0.0.1", DEFAULT_PORT, userName, cmdArgs, settings);
}

void LoopClient::handleInput(float deltaT, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	pd.input->keystates = SDL_GetKeyboardState(NULL);

	//Event loop, mostly just passing stuff to InputMap (in-game controls) and UserInterface (gui controls)
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (pd.gui->handleInput(e, pd.input))
		{
			pd.context->setMouseLock(false);
		}
		pd.input->handleInput(e);

		if (e.type == SDL_QUIT)
		{
			//Remember the window size we're closing at so next launch starts at the same size
			//instead of the fixed default, which is what left saved ImGui window positions
			//(and the server browser) partially off-screen after a resize.
			glm::vec2 resolution = pd.context->getResolution();
			settings->addInt("graphics/startresolutionx", (int)resolution.x, true, "Program X resolution to start with", 1, 4096);
			settings->addInt("graphics/startresolutiony", (int)resolution.y, true, "Program Y resolution to start with", 1, 4096);
			settings->exportToFile("Config/settings.txt");

			leaveServer(cmdArgs);
			cmdArgs.mainLoopRun = false;
			break;
		}
		else if (e.type == SDL_WINDOWEVENT)
		{
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				pd.context->setSize(e.window.data1, e.window.data2);
				simulation.camera->setAspectRatio(pd.context->getResolution().x / pd.context->getResolution().y);
			}
		}
		else if (e.type == SDL_MOUSEMOTION && pd.context->getMouseLocked())
		{
			simulation.camera->turn(-(float)e.motion.xrel, -(float)e.motion.yrel);
		}
		else if (e.type == SDL_KEYDOWN)
		{
			//This one is not handled through input map because input map can be suppressed
			//By having one or more guis open, which defeats the purpose of a quick gui close key
			if (e.key.keysym.sym == SDLK_ESCAPE)
			{
				if (pd.gui->getOpenWindowCount() == 0)
				{
					pd.escapeMenu->open();
					pd.context->setMouseLock(false);
				}
				else
				{
					pd.gui->closeOneWindow();
					if (!pd.context->getMouseLocked() && !pd.gui->getOpenWindowCount() && cmdArgs.gameState != NotInGame)
						pd.context->setMouseLock(true);
				}
			}
		}
		else if (e.type == SDL_MOUSEBUTTONDOWN && simulation.camera && !pd.gui->shouldUnlockMouse() && cmdArgs.gameState == InGame)
		{
			int mx, my;
			int mask = SDL_GetMouseState(&mx, &my);

			float x = (float)mx / pd.context->getResolution().x * 2 - 1;
			float y = (float)my / pd.context->getResolution().y * 2 - 1;

			glm::vec3 worldPos = simulation.camera->mouseCoordsToWorldSpace(glm::vec2(x, y));
			glm::vec3 dir = simulation.camera->getDirection();

			ENetPacket *mouseClickPacket = makeMouseClickPacket(worldPos, dir, mask);
			client->send(mouseClickPacket, OtherReliable);
		}
	}

	//Interacting with gui, don't move around in-game
	pd.input->supressed = pd.gui->wantsSuppression();

	//Someone just applied setting changes
	if (pd.settingsMenu->pollForChanges())
	{
		Logger::setDebug(settings->getBool("logger/verbose"));
		simulation.camera->updateSettings(settings);
		pd.gui->updateSettings(settings);
		simulation.idealBufferSize = settings->getInt("network/snapshotbuffer");
	}

	if (pd.debugMenu->passwordSubmitted())
	{
		std::string password = pd.debugMenu->getPassword();
		simulation.evalPassword = password;
		if (cmdArgs.gameState != InGame)
			pd.debugMenu->adminLoginComment = "Not in a server!";
		else
			client->send(attemptEvalLogin(password), OtherReliable);
	}

	if (pd.debugMenu->isCommandWaiting())
	{
		std::string command = pd.debugMenu->getLuaCommand();
		client->send(evalCommand(simulation.evalPassword, command), OtherReliable);
	}

	if (pd.serverBrowser->serverPickReady())
	{
		std::string ip,userName;
		int port;
		pd.serverBrowser->getServerData(ip, port,userName);
		connectToServer(ip, port,userName,cmdArgs,settings);
	}

	if (pd.serverBrowser->singlePlayerReady())
	{
		pd.serverBrowser->clearSinglePlayerReady();
		hostSinglePlayer(cmdArgs, settings);
	}

	if (pd.serverBrowser->settingsReady())
	{
		pd.serverBrowser->clearSettingsReady();
		pd.settingsMenu->open();
	}

	EscapeButtonPressed escapeMenuButton = pd.escapeMenu->getLastButtonPress();
	switch (escapeMenuButton)
	{
		case LeaveGame:
		{
			if (cmdArgs.gameState != NotInGame)
				leaveServer(cmdArgs);
			cmdArgs.mainLoopRun = false;
			return;
		}

		case LeaveServer:
		{
			if (cmdArgs.gameState != NotInGame)
				leaveServer(cmdArgs);
			break;
		}

		case OpenChat:
		{
			pd.chatWindow->open();
			break;
		}

		case OpenSettings:
		{
			pd.settingsMenu->open();
			break;
		}

		case OpenDebugMenu:
		{
			pd.debugMenu->open();
			break;
		}

		case JoinServer:
		{
			pd.serverBrowser->open();
			break;
		}

		case None:
		default:
			break;
	}

	if(cmdArgs.gameState == NotInGame)
	{
		return;
	}

	//Various keys were pressed that were bound to certain commands:
	if (pd.input->pollCommand(MouseLock))
		pd.context->setMouseLock(!pd.context->getMouseLocked());

	//Move camera around
	simulation.camera->control(deltaT, pd.input);

	if (pd.input->pollCommand(FirstThirdPerson))
		simulation.camera->swapPerson();

	if (pd.input->pollCommand(DebugView))
		pd.debugMenu->showDebugPhysicsView = !pd.debugMenu->showDebugPhysicsView;
}

void LoopClient::predictLocalCollisions()
{
	if (!pd.physicsWorld)
		return;

	//How long a predicted local reaction is trusted before falling back to the (by-then-hopefully-arrived) server state
	const unsigned int predictionWindowMS = 250;

	//A pushed object almost always outruns the player's own walk speed within a frame or two, so contact - and with it,
	//the window above - ends almost immediately even though the object is still clearly sliding from the push. Without
	//this, the server's still-stale (pre-push) position would stomp it mid-slide, and since the player keeps walking
	//into it, that repeats continuously and looks like rubber-banding rather than a single correction
	const float stillMovingSpeedThreshold = 0.5f;

	//Upper bound on how long continued motion can keep extending prediction *after we've lost contact* with the
	//object (e.g. it slides/falls on past the point where we last touched it). Client and server resolve a many-body
	//pileup independently and can diverge into totally unrelated resting positions the longer prediction runs
	//unmoored from an actual confirmed touch, so we want to cap that divergence risk - but this must NOT count time
	//spent still actively touching the object (see predictLocallyStartedAt below), or a long sustained push (e.g.
	//walking a cube toward a ledge) gets cut off mid-fall once it goes over, handing back to a server position that
	//hasn't caught up yet and causing a visible correction right in the middle of an otherwise-normal interaction
	const unsigned int maxPredictionDurationMS = 1500;

	//True if this object is touching some OTHER (non-player) dynamic right now - not safe to predict, since that's
	//exactly where tiny numeric/ordering differences between the client's and server's independently-run simulations
	//get amplified into completely different outcomes. A lone object interacting only with the player and static
	//geometry (ground, bricks - identical on both sides since they don't move) stays close enough to the server's
	//own version to predict safely; a multi-body pileup does not. See the brainstorm on why piles teleport on settle
	auto touchingOtherDynamics = [&](const std::shared_ptr<Dynamic>& obj) -> bool
	{
		for (btRigidBody* other : pd.physicsWorld->getTouching(obj->body))
		{
			if (other->getUserIndex() != dynamicBody)
				continue;

			std::shared_ptr<Dynamic> otherDynamic = dynamicFromBody(other);
			if (otherDynamic && !otherDynamic->clientControlled)
				return true;
		}
		return false;
	};

	for (unsigned int i = 0; i < simulation.controlledDynamics.size(); i++)
	{
		std::shared_ptr<Dynamic> controlled = simulation.controlledDynamics[i];
		if (!controlled || !controlled->body)
			continue;

		for (btRigidBody* other : pd.physicsWorld->getTouching(controlled->body))
		{
			if (other->getUserIndex() != dynamicBody)
				continue;

			std::shared_ptr<Dynamic> touched = dynamicFromBody(other);
			if (touched && !touched->clientControlled && !touchingOtherDynamics(touched))
			{
				//Refreshed every frame we're actually touching it, so continuous pushing never runs into the cap
				//below - it only starts counting once contact actually ends, which is the point it's meant to bound
				touched->predictLocallyStartedAt = getTicksMS();
				touched->predictLocallyUntil = getTicksMS() + predictionWindowMS;
			}
		}
	}

	if (simulation.dynamics)
	{
		for (unsigned int i = 0; i < simulation.dynamics->size(); i++)
		{
			std::shared_ptr<Dynamic> d = simulation.dynamics->get(i);
			if (!d || !d->body || d->clientControlled)
				continue;

			bool stillPredicting = getTicksMS() < d->predictLocallyUntil;
			bool stillMovingFast = d->body->getLinearVelocity().length2() > stillMovingSpeedThreshold * stillMovingSpeedThreshold;
			bool underEpisodeCap = getTicksMS() < d->predictLocallyStartedAt + maxPredictionDurationMS;

			//Stop extending (letting the existing window run out naturally) the moment it touches another dynamic -
			//e.g. a cube pushed off a ledge lands on top of a different cube mid-fall
			if (stillPredicting && stillMovingFast && underEpisodeCap && !touchingOtherDynamics(d))
				d->predictLocallyUntil = getTicksMS() + predictionWindowMS;
		}
	}
}

void LoopClient::renderEverything(float deltaT)
{
	//TODO: Get rid of this
	if (simulation.dynamics)
	{
		for (unsigned a = 0; a < simulation.dynamics->size(); a++)
		{
			std::shared_ptr<Dynamic> d = simulation.dynamics->get(a);
			bool predictingLocally = getTicksMS() < d->predictLocallyUntil;

			if (d->wasPredictingLocally && !predictingLocally)
				d->handOffFromPrediction(simulation.idealBufferSize);
			d->wasPredictingLocally = predictingLocally;

			d->updateSnapshot(deltaT, pd.debugMenu->showDebugPhysicsView || predictingLocally);
		}
	}

	//Technically rendering related calculations based on previously inputted transform data
	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->getModel()->updateAll(deltaT);

	simulation.camera->calculateLightSpaceMatricies(glm::normalize(glm::vec3(0.2, 1.0, 0.4)), pd.lightSpaceMatricies);

	//Render shadows to texture:
	pd.shadows->use();
	pd.shaders->modelShadowShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformShadow, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);

	//Models:
	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();
	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->render(pd.shaders,false);

	//Bricks:
	pd.shaders->basicUniforms.nonInstanced = true;
	pd.shaders->updateBasicUBO();
	testBricks.render(-1);

	//Start rendering to screen:
	pd.context->select();
	pd.context->clear(0.4f, 0.4f, 0.8f);

	simulation.camera->render(pd.shaders, deltaT, pd.physicsWorld);
	pd.shaders->modelShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformModel, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);
	pd.shadows->bindDepthResult(ShadowArray);

	//Models:
	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();
	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->render(pd.shaders);

	//Render grass
	pd.shaders->basicUniforms.ScaleMatrix = glm::mat4(1.0);
	pd.shaders->basicUniforms.TranslationMatrix = glm::mat4(1.0);
	pd.shaders->basicUniforms.RotationMatrix = glm::mat4(1.0);
	pd.shaders->basicUniforms.nonInstanced = 1;
	pd.shaders->basicUniforms.cameraSpacePosition = 1;
	pd.shaders->updateBasicUBO();
	pd.grassMaterial->use(pd.shaders);
	glBindVertexArray(pd.grassVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	//Bricks
	if(pd.textures->getTexture(3))
		pd.textures->getTexture(3)->bind(PBRArray);
	pd.shaders->brickShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformBrick, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);
	pd.shaders->basicUniforms.nonInstanced = true;
	pd.shaders->updateBasicUBO();
	testBricks.render(pd.shaders->brickShader->getUniformLocation("brickChunkPos"));

	//Outlines/highlights: a selection-style indicator that should show through everything in the scene except
	//its own source object (so it doesn't just paint a solid blob over the object it's highlighting) and other
	//highlights (so overlapping highlights never fight over which one is "in front"). Scene depth is ignored
	//entirely (X-ray), and a per-object stencil mask excludes exactly that object's own silhouette from its
	//own outline. Processed one highlighted instance at a time since each needs its own stencil mask; expected
	//to be a small handful of instances at most (this is a selection indicator, not a bulk rendering effect)
	if (!ModelInstance::highlightedInstances.empty())
	{
		pd.shaders->outlineShader->use();
		GLint maskPassUniform = pd.shaders->outlineShader->getUniformLocation("maskPass");

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_STENCIL_TEST);
		glStencilMask(0xFF);

		for (ModelInstance* instance : ModelInstance::highlightedInstances)
		{
			glClear(GL_STENCIL_BUFFER_BIT);

			//Mask pass: mark this instance's own true (non-inflated) silhouette in the stencil buffer, contributing nothing to color
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glCullFace(GL_BACK);
			glStencilFunc(GL_ALWAYS, 1, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glUniform1i(maskPassUniform, 1);
			instance->renderSelfOutline();

			//Outline pass: draw the extruded shell everywhere except where the mask above just marked
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glCullFace(GL_FRONT);
			glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUniform1i(maskPassUniform, 0);
			instance->renderSelfOutline();
		}

		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
	}

	//GUI
	bool crossHair = false;
	if (simulation.camera)
		crossHair = pd.context->getMouseLocked() && simulation.camera->getFirstPerson();

	std::vector<std::string> hudLines;
	if (pd.debugMenu->showDebugPhysicsView)
		hudLines.push_back("Debug physics view ON (Left Shift toggles)");

	pd.escapeMenu->showLeaveServer = client != nullptr;
	pd.gui->render(pd.context->getResolution().x, pd.context->getResolution().y,crossHair,hudLines);

	//End frame
	pd.context->swap();
}

void LoopClient::sendControlledObjects()
{
	if(getTicksMS() - lastSentControlledObjects < 100)
		return;

	lastSentControlledObjects = getTicksMS();

	//We're simulating these objects for the server, send updates on their transforms back to the server
	for (int i = 0; i < simulation.controlledDynamics.size(); i++)
	{
		std::shared_ptr<Dynamic> d = simulation.controlledDynamics[i];

		//TODO: Do this through objHolder I guess? Ideally there'd never be more than like 1 or 2 of these per client tho
		if (!d->requiresNetUpdate())
			continue;

		ENetPacket* update = enet_packet_create(NULL, d->getUpdatePacketBytes() + 1 + sizeof(netIDType), getFlagsFromChannel(Unreliable));
		update->data[0] = (unsigned char)ControlledPhysics;
		netIDType id = d->getID();
		memcpy(update->data + 1, &id, sizeof(netIDType));
		d->addToUpdatePacket(update->data + 1 + sizeof(netIDType));
		client->send(update, Unreliable);
	}
}

void LoopClient::updateControllers(float deltaT)
{
	//Go through player controllers, remove any that are bound to now deleted dynamics 
	auto ctrlIter = simulation.controllers.begin();
	while (ctrlIter != simulation.controllers.end())
	{
		//Apply movement inputs client side 
		if ((*ctrlIter)->control(pd.input, simulation.camera, deltaT, pd.physicsWorld))
		{
			ctrlIter = simulation.controllers.erase(ctrlIter);
			continue;
		}
		else
		{
			//Send movement inputs to server to be applied there
			if (!client)
			{
				++ctrlIter;
				continue;
			}

			ENetPacket* packet = (*ctrlIter)->makeMovementInputsPacket();
			if(packet)
				client->send(packet, Unreliable);

			++ctrlIter;
		}
	}
}

void LoopClient::run(float deltaT,ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	//Single player: tick our embedded server before doing any client work this frame.
	//It shares the SimObject::world static with us, so reclaim it for our own PhysicsWorld once it's done.
	if (localServer)
	{
		localServer->run(deltaT, cmdArgs, settings);
		if (pd.physicsWorld)
			SimObject::world = pd.physicsWorld;
	}

	if (client)
	{
		//We're in game, equivalent to gameState == InGame

		KickReason reason = client->run(pd, simulation, cmdArgs); //  <--- networking, process packets
		if(reason != NotKicked) 
		{
			//We lost connection somehow
			leaveServer(cmdArgs);

			//Show pop-up
			pd.serverBrowser->setKickReason(reason);
		}

		sendControlledObjects();
	}

	//movement keys and camera direction as it relates to players / controlled objects 
	updateControllers(deltaT); 

	handleInput(deltaT,cmdArgs,settings); //mouse and keyboard input

	// --- UI Updates and Requests ---

	//Send info to debug menu for display
	NetInfo netInfo;
	if (client)
		netInfo = { client->getPing(), client->getIncoming(), client->getOutgoing(), simulation.serverLastSlowestFrame, simulation.serverAverageFrame };
	pd.debugMenu->passDetails(simulation.camera->getPosition(),simulation.camera->getDirection(), netInfo);

	if (simulation.dynamics && simulation.dynamics->size() > 0)
		pd.debugMenu->addExtraLine("First other snaps: " + std::to_string(simulation.dynamics->get(0)->interpolator.getNumSnapshots()));
	if (simulation.controlledDynamics.size() > 0)
		pd.debugMenu->addExtraLine("First controlled snaps: " + std::to_string(simulation.controlledDynamics[0]->interpolator.getNumSnapshots()));
	if (client)
	{
		pd.debugMenu->addExtraLine("Total queued packets: " + std::to_string(client->getNumQueued()));
		pd.debugMenu->addExtraLine("Ping variance: " + std::to_string(client->getPingVariance()));
		pd.debugMenu->addExtraLine("Packet loss: " + std::to_string(client->getLoss()));
	}

	if (pd.chatWindow->hasChatMessage() && client)
	{
		ENetPacket *chat = makeChatMessage(pd.chatWindow->getChatMessage());
		client->send(chat, OtherReliable);
	}

	//Progress loading SimObject types
	pd.serverBrowser->passLoadProgress(pd.signals.typesToLoad, simulation.dynamicTypes.size());

	// --- State changes requested from packets ---

	//Phase one loading started
	if (pd.signals.startPhaseOneLoading)
	{ 
		//Server accepted join request
		//Start up systems needed to play
		pd.physicsWorld = std::make_shared<PhysicsWorld>();
		SimObject::world = pd.physicsWorld;
		cmdArgs.gameState = LoadingTypes; 
	}

	//Phase one loading done
	if (pd.signals.finishedPhaseOneLoading)
	{
		//Create holders for objects now that we will start receiving data about them
		simulation.dynamics = new ObjHolder<Dynamic>(DynamicTypeId);
		simulation.statics = new ObjHolder<StaticObject>(StaticTypeId);

		ENetPacket* finishedLoading = makeLoadingFinished();
		client->send(finishedLoading, OtherReliable);

		cmdArgs.gameState = InGame;

		pd.serverBrowser->setConnectionNote("");
		pd.serverBrowser->close();

		pd.chatWindow->open();
	}

	//All signals from packets processed for this frame, reset flags
	pd.signals.reset();

	// --- End packet requests ---

	if (pd.physicsWorld)
		pd.physicsWorld->step(deltaT);

	predictLocalCollisions();

	renderEverything(deltaT);
}

LoopClient::LoopClient(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	//This will also populate key-bind specific defaults, so we reexport after
	pd.input = std::make_shared<InputMap>(settings);
	settings->exportToFile("Config/settings.txt");

	info("Not dedicated, initalizing client.");

	simulation.idealBufferSize = settings->getInt("network/snapshotbuffer");

	//Create our program window
	pd.context = std::make_shared<RenderContext>(settings);
	if (!pd.context->isValid())
	{
		//RenderContext already logged exactly what went wrong (window/GL context creation failure). Bail out
		//here instead of continuing on to create ImGui/ShaderManager/etc against a nonexistent GL context,
		//which previously crashed on a null GL function pointer far away from the actual root cause.
		error("Could not create a valid render context, aborting client startup.");
		return;
	}

	pd.gui = std::make_shared<UserInterface>();
	pd.gui->updateSettings(settings);
	pd.settingsMenu = pd.gui->createWindow<SettingsMenu>(settings, pd.input);
	pd.debugMenu = pd.gui->createWindow<DebugMenu>();
	pd.escapeMenu = pd.gui->createWindow<EscapeMenu>();
	pd.serverBrowser = pd.gui->createWindow<ServerBrowser>();
	pd.chatWindow = pd.gui->createWindow<ChatWindow>();
	pd.serverBrowser->passDefaultSettings(settings->getString("network/lastip"), settings->getInt("network/port"), settings->getString("network/username"));
	pd.serverBrowser->open();

	pd.gui->initAll();

	//Load all the shaders
	pd.shaders = std::make_shared<ShaderManager>();
	if(pd.shaders->readShaderList("Shaders/shadersList.txt"))
	{
		pd.gui->popupErrorMessage = "Error loading shaders, see error log.";
	}

	simulation.camera = std::make_shared<Camera>(pd.context->getResolution().x / pd.context->getResolution().y);
	simulation.camera->updateSettings(settings);

	//A few test decals
	pd.textures = std::make_shared<TextureManager>();
	pd.textures->allocateForDecals(128);
	pd.textures->finalizeDecals();

	pd.grassMaterial = new Material("Assets/ground/grass.txt", pd.textures);

	if (!pd.grassMaterial->isValid())
	{
		pd.gui->popupErrorMessage = "Error loading grass material, see error log.";
	}

	pd.grassVao = createQuadVAO();

	RenderTarget::RenderTargetSettings shadowSettings;
	shadowSettings.width = 2048;
	shadowSettings.height = 2048;
	shadowSettings.layers = 3;
	shadowSettings.useColor = false;
	pd.shadows = std::make_shared<RenderTarget>(shadowSettings,pd.textures);
	pd.lightSpaceMatriciesUniformShadow = pd.shaders->modelShadowShader->getUniformLocation("lightSpaceMatricies");
	pd.lightSpaceMatriciesUniformModel = pd.shaders->modelShader->getUniformLocation("lightSpaceMatricies");
	pd.lightSpaceMatriciesUniformBrick = pd.shaders->brickShader->getUniformLocation("lightSpaceMatricies");

	info("Start up complete");

	printAllGraphicsErrors("End of initalization");

	valid = true;

	//Automated testing convenience: skip the server browser and get straight into a game, same as clicking "Start Server"
	if (cmdArgs.autoSinglePlayer)
		hostSinglePlayer(cmdArgs, settings);

	/* {
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 2;
		tmp->h = 2;
		tmp->l = 2;
		tmp->x = 5;
		tmp->y = 5;
		tmp->z = 5;
		testBricks.addBrick(tmp);
	}

	{
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 2;
		tmp->h = 1;
		tmp->l = 1;
		tmp->x = 10;
		tmp->y = 10;
		tmp->z = 10;
		testBricks.addBrick(tmp);
	}

	{
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 1;
		tmp->h = 5;
		tmp->l = 1;
		tmp->x = 15;
		tmp->y = 15;
		tmp->z = 15;
		testBricks.addBrick(tmp);
	}

	{
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 5;
		tmp->h = 5;
		tmp->l = 5;
		tmp->x = 20;
		tmp->y = 5;
		tmp->z = 20;
		testBricks.addBrick(tmp);
	}
	*/
	{
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 4;
		tmp->h = 4;
		tmp->l = 4;
		tmp->x = 10;
		tmp->y = 5;
		tmp->z = 5;
		testBricks.addBrick(tmp);
	}

	{
		BrickRenderData* tmp = new BrickRenderData;
		tmp->w = 1;
		tmp->h = 1;
		tmp->l = 1;
		tmp->x = 15;
		tmp->y = 5;
		tmp->z = 5;
		//testBricks.addBrick(tmp);
	}

	/*for (int i = 0; i < 100000; i++)
	{
		BrickRenderData * tmp = new BrickRenderData;
		tmp->w = rand() % 5 + 1;
		tmp->h = rand() % 5 + 1;
		tmp->l = rand() % 5 + 1;
		tmp->x = rand() % 500;
		tmp->y = rand() % 300;
		tmp->z = rand() % 500;
		testBricks.addBrick(tmp);
	}*/

	testBricks.recompile();
}

LoopClient::~LoopClient()
{
	//testChunk.deleteAllBricks();

	pd.shadows.reset();

	delete pd.grassMaterial;
	glDeleteVertexArrays(1, &pd.grassVao);

	//Not needed this is a destructor lol
	//Also this should only be called when the programs shutting down anyway 
	pd.context.reset();
	pd.gui.reset(); //Will handle indivdual windows
	pd.shaders.reset();
	pd.textures.reset();

	//This one is actually useful because the server will learn we disconnected faster if we do it properly
	if(client)
		delete client;

	if (localServer)
		delete localServer;
}
