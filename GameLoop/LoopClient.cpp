#include "LoopClient.h"
#include "../External/stb_image_write.h"
#include "../Bricks/BrickSaves.h"
#include <cfloat>

void LoopClient::leaveServer(ExecutableArguments& cmdArgs)
{
	info("Leaving server");

	//Nobody to talk to anymore
	voiceToggled = false;

	//The next server hasn't been told about our paint can, and starts out thinking we don't have one
	paletteWasShown = false;
	paintCanOut = false;
	paintCanSent = false;

	//The next one decides for itself whether we can fly a camera around it
	freeCamera = false;
	cameraTargetBeforeFlying.reset();
	simulation.freeCameraEnabled = false;

	//Whatever this server's Lua put over our screen goes with it
	simulation.vignette.clear();

	pd.serverBrowser->open();

	if (!client)
		return;

	pd.chatWindow->close();
	//The next server has its own commands
	pd.chatWindow->clearSuggestions();
	pd.wrenchDialog->close();
	pd.printMenu->close();
	pd.vehicleLoader->close();
	pd.vehicleGhost.cancel();
	pd.brickSaveMenu->close();
	simulation.brickSaves.clear();
	pd.playerList->close();
	pd.playerList->clear();

	//Will need to log in again to get eval access
	pd.debugMenu->reset();

	simulation.controllers.clear();
	simulation.controlledDynamics.clear();

	//Before dynamics, while the physics world their bodies are in still exists
	if (simulation.vehicles)
	{
		simulation.vehicles->destroyAll();
		delete simulation.vehicles;
		simulation.vehicles = nullptr;
	}
	simulation.camera->alsoIgnore = nullptr;
	vehicleDraws.clear();
	pd.selectionBox.cancel();
	jetSuppressed = false;

	//destroyAll actually frees each object (and its ModelInstance, removing it from e.g. the highlight list)
	//Deleting the holder alone would leak them, leaving their highlights drawn over the main menu
	if (simulation.dynamics)
	{
		simulation.dynamics->destroyAll();
		delete simulation.dynamics;
		simulation.dynamics = nullptr;
	}

	if (simulation.statics)
	{
		simulation.statics->destroyAll();
		delete simulation.statics;
		simulation.statics = nullptr;
	}

	if (simulation.lights)
	{
		simulation.lights->destroyAll();
		delete simulation.lights;
		simulation.lights = nullptr;
	}

	if (simulation.emitters)
	{
		simulation.emitters->destroyAll();
		delete simulation.emitters;
		simulation.emitters = nullptr;
	}
	pd.particles->clear();

	//Removes brick and debris bodies, so it has to happen before the physics world is destroyed below
	delete simulation.bricks;
	simulation.bricks = nullptr;
	delete simulation.brickDebris;
	simulation.brickDebris = nullptr;
	simulation.brickTypeFromServer.clear();
	simulation.brickTypeToServer.clear();
	simulation.printFromServer.clear();
	simulation.serverPrintNames.clear();

	delete client;
	client = nullptr;

	//It's very possible some or all data structures may not have been initialized or allocated if we disconnected in the middle loading into a new server

	simulation.dynamicTypes.clear();
	pd.signals.typesToLoad = 0; //Disable progress bar in server browser UI until next join
	
	simulation.evalPassword = "";
	simulation.waterEnabled = false;
	pd.waterRipples.clear();
	simulation.rainIntensity = 0.0f;
	pd.rain.clear();
	simulation.dayCycle = DayCycle();
	simulation.skyboxPaths[0].clear();
	simulation.skyboxPaths[1].clear();
	pd.ghostBrick.hide();
	pd.brickHotbar->putAway();
	pd.brickHotbar->takeChange();
	pd.itemHotbar->putAway();
	pd.itemHotbar->takeChange();
	std::fill(std::begin(simulation.inventory), std::end(simulation.inventory), NO_ID);
	simulation.jetsEnabled = true;
	simulation.flashlightEnabled = true;
	flashlightOn = false;
	flashlightHeldMS = 0;
	flashlightCycling = false;
	pd.voice->clear();
	pd.audio->clear();

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

	if (userName.length() > 0)
		pd.state->addString("network/username", userName);
	joinedName = userName;
	pd.playerList->setOwnName(joinedName);
	pd.state->addString("network/lastip", ip);
	pd.state->addInt("network/lastport", port, true, "", 1, 65535);
	pd.state->exportToFile(ClientProgramData::stateFilePath);

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

	//How we look, which the server keeps until its Lua puts it on our player with client:applyAppearance
	client->send(makeAppearanceChoicePacket(AppearanceEditor::loadAppearance(settings)), JoinNegotiation);

	//And our paint, which the server's Lua paints bricks with
	client->send(makePaintChoicePacket(pd.paintMenu->getColor(), pd.paintMenu->getMaterial()), JoinNegotiation);

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

	std::string userName = pd.state->getString("network/username");
	if (userName.length() < 1)
		userName = "Player";

	connectToServer("127.0.0.1", DEFAULT_PORT, userName, cmdArgs, settings);
}

void LoopClient::updateDisplayItemHighlight()
{
	//How far from our player a click takes an item from a brick offering one, the same reach Inventory.lua allows, and how far the crosshair looks for one
	static constexpr float displayItemReach = 10.0f;
	static constexpr float displayItemLook = 60.0f;
	//A faint outline, enough to say it can be taken
	static const glm::vec4 displayItemOutline = glm::vec4(1.0f, 1.0f, 1.0f, 0.35f);
	static constexpr float displayItemOutlineThickness = 0.12f;

	std::shared_ptr<Dynamic> hovered = nullptr;
	std::shared_ptr<Dynamic> player = getOwnPlayer();
	if (simulation.dynamics && simulation.camera && pd.physicsWorld && player && player->body && pd.context->getMouseLocked())
	{
		glm::vec3 start = simulation.camera->getPosition();
		btVector3 hitPosition, hitNormal;
		btRigidBody* hit = pd.physicsWorld->doRaycast(g2b3(start), g2b3(start + simulation.camera->getDirection() * displayItemLook), player->body, hitPosition, hitNormal);
		if (hit && hit->getUserIndex() == dynamicBody)
		{
			std::shared_ptr<Dynamic> dynamic = dynamicFromBody(hit);
			if (dynamic && dynamic->getKind() == DynamicKind_Item && std::static_pointer_cast<Item>(dynamic)->display
				&& glm::distance(b2g3(hitPosition), b2g3(player->getPosition())) <= displayItemReach)
				hovered = dynamic;
		}
	}

	netIDType hoveredID = hovered ? hovered->getID() : NO_ID;
	if (hoveredID == highlightedDisplayItem)
		return;

	//The last one loses its outline, if it's still around
	if (highlightedDisplayItem != NO_ID && simulation.dynamics)
	{
		if (std::shared_ptr<Dynamic> last = simulation.dynamics->find(highlightedDisplayItem))
			last->setHighlight(glm::vec4(0), 0.0f);
	}

	if (hovered)
		hovered->setHighlight(displayItemOutline, displayItemOutlineThickness);

	highlightedDisplayItem = hoveredID;
}

//Puts the ghost brick on whatever the camera is pointing at, if anything is in reach
static void spawnGhostFromCamera(ClientProgramData& pd, Simulation& simulation)
{
	if (!pd.physicsWorld || !simulation.camera)
		return;

	glm::vec3 start = simulation.camera->getPosition();
	glm::vec3 direction = simulation.camera->getDirection();
	btRigidBody* ignore = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
	btVector3 hitPosition, hitNormal;
	if (pd.physicsWorld->doRaycast(g2b3(start), g2b3(start + direction * 250.0f), ignore, hitPosition, hitNormal))
		pd.ghostBrick.spawnAt(b2g3(hitPosition), b2g3(hitNormal));
}

//0 to 1 around the flashlight's colors: fades from white into red, goes around every hue, and fades back into white
static glm::vec3 flashlightColor(float cycle)
{
	static constexpr float fadeFromWhite = 0.1f;

	float saturation = std::clamp(std::min(cycle, 1.0f - cycle) / fadeFromWhite, 0.0f, 1.0f);
	glm::vec3 hue = glm::clamp(glm::abs(glm::mod(cycle * 6.0f + glm::vec3(0, 4, 2), 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
	return glm::mix(glm::vec3(1), hue, saturation);
}

void LoopClient::updateFlashlight(float deltaT)
{
	//Held longer than this, the key cycles the color instead of switching the flashlight on or off
	static constexpr float holdForColorMS = 350.0f;
	//Once around every color and back to white
	static constexpr float colorCycleMS = 8000.0f;
	//How often the color goes to the server while cycling
	static constexpr float colorSendMS = 100.0f;

	//Polled every frame so a press while it's disabled doesn't go off later, and so a tap too quick to be down on any frame still counts
	bool pressed = pd.input->pollCommand(Flashlight);

	//Driving a vehicle with a headlight, the key switches that instead of our own light, which is the vehicle's so it doesn't mind whether ours is allowed
	//Only a tap, holding it doesn't cycle a headlight's color, that's set in the vehicle's wrench dialog
	std::shared_ptr<Vehicle> driven = getDrivenVehicle();
	if (driven && driven->hasHeadlight && client)
	{
		flashlightHeldMS = 0;
		flashlightCycling = false;
		if (pressed)
			client->send(makeVehicleLightPacket(), OtherReliable);
		return;
	}

	//The server turned it off when it disabled it
	if (!simulation.flashlightEnabled || !client)
	{
		flashlightOn = false;
		flashlightHeldMS = 0;
		flashlightCycling = false;
		return;
	}

	bool switched = false;
	if (pd.input->isCommandKeydown(Flashlight))
	{
		flashlightHeldMS += deltaT;
		if (flashlightHeldMS >= holdForColorMS)
		{
			//Comes on to show the colors going by
			if (!flashlightOn)
			{
				flashlightOn = true;
				switched = true;
			}

			flashlightCycling = true;
			flashlightCycle = std::fmod(flashlightCycle + deltaT / colorCycleMS, 1.0f);
			flashlightColorUnsent = true;
		}
	}
	else
	{
		if ((flashlightHeldMS > 0 || pressed) && !flashlightCycling)
		{
			flashlightOn = !flashlightOn;
			switched = true;
		}

		flashlightHeldMS = 0;
		flashlightCycling = false;
	}

	flashlightSinceSentMS += deltaT;
	if (switched || (flashlightColorUnsent && (!flashlightCycling || flashlightSinceSentMS >= colorSendMS)))
	{
		client->send(makeFlashlightPacket(flashlightOn, flashlightColor(flashlightCycle)), OtherReliable);
		flashlightSinceSentMS = 0;
		flashlightColorUnsent = false;
	}
}

bool LoopClient::placeHeldLight(Light& light, glm::vec3& position, glm::vec3& direction)
{
	//How far along the beam past where it leaves the holder's collision box the light sits
	//The drawn arm and hand can reach past the collision box, so this has to clear them too
	static constexpr float handClearance = 0.45f;
	//Without a hand, how far past the side of the collision box, so the head doesn't shadow it
	static constexpr float headClearance = 0.3f;

	std::shared_ptr<Dynamic> holder = light.holder.lock();
	if (!holder || holder->getID() != light.getHolderID())
	{
		holder = simulation.dynamics ? simulation.dynamics->find(light.getHolderID()) : nullptr;
		light.holder = holder;
	}

	if (!holder)
		return false;

	std::shared_ptr<Model> model = holder->getType()->getModel();

	//Our own flashlight points exactly where we look, rather than gliding after the direction the server was last sent
	if (holder->clientControlled && simulation.camera->target.lock() == holder)
		direction = simulation.camera->getDirection();

	//From the right hand as it's drawn, swinging with it, which is kept up to date even while our own player is hidden in first person
	int hand = model->getMeshIdx("Right_Hand");
	if (hand != -1)
	{
		glm::vec3 handCenter = holder->getMeshCenter(hand);

		//The collision box as it's drawn, which the hand is inside
		btTransform bodyTransform = holder->body->getWorldTransform();
		btQuaternion bodyRotation = bodyTransform.getRotation();
		glm::vec3 origin = holder->renderedTransformInitialized ? holder->renderedPosition : b2g3(bodyTransform.getOrigin());
		glm::quat rotation = holder->renderedTransformInitialized ? holder->renderedRotation : glm::quat(bodyRotation.w(), bodyRotation.x(), bodyRotation.y(), bodyRotation.z());
		glm::quat toBox = glm::inverse(rotation);
		glm::vec3 halfExtents = model->getColHalfExtents();
		glm::vec3 start = toBox * (handCenter - (origin + rotation * model->getColOffset()));
		glm::vec3 along = toBox * direction;

		//Slid out of the box along the beam, so the holder's own body can't shadow it: just past the hand when it points away from them,
		//around the far side of them when it points across or behind them, for a body still turning to face where they look
		float exit = std::numeric_limits<float>::max();
		for (int axis = 0; axis < 3; axis++)
		{
			if (std::abs(along[axis]) > 0.00001f)
				exit = std::min(exit, ((along[axis] > 0 ? halfExtents[axis] : -halfExtents[axis]) - start[axis]) / along[axis]);
		}
		if (exit == std::numeric_limits<float>::max())
			exit = 0;

		position = handCenter + direction * (std::max(exit, 0.0f) + handClearance);
		return true;
	}

	glm::vec3 eyes;
	if (holder->clientControlled)
	{
		//Where the camera puts the eyes of a dynamic we move ourselves, so our own flashlight never trails behind the view
		eyes = b2g3(holder->body->getWorldTransform().getOrigin()) + holder->interpolator.getRotation() * model->getEyePosition();
	}
	else
	{
		glm::vec3 drawnAt = holder->renderedTransformInitialized ? holder->renderedPosition : b2g3(holder->getPosition());
		eyes = drawnAt + holder->renderedRotation * model->getEyePosition();
	}

	glm::vec3 halfExtents = model->getColHalfExtents();
	position = eyes + direction * (std::max(halfExtents.x, halfExtents.z) + headClearance);
	return true;
}

Texture* LoopClient::findItemIcon(const std::string& path)
{
	auto found = itemIcons.find(path);
	if (found != itemIcons.end())
		return found->second;

	//From our own copy of the game folder, like skyboxes
	Texture* icon = nullptr;
	if (!path.empty() && isPathInsideGameFolder(path) && std::filesystem::is_regular_file(path))
	{
		icon = pd.textures->createTexture(path);
		if (icon && !icon->isValid())
			icon = nullptr;
	}

	itemIcons[path] = icon;
	return icon;
}

void LoopClient::updateItemHotbar()
{
	static_assert(ItemHotbar::slotCount == inventorySize, "The item bar needs a slot for each inventory slot");

	for (int slot = 0; slot < inventorySize; slot++)
	{
		std::shared_ptr<Dynamic> dynamic = simulation.dynamics && simulation.inventory[slot] != NO_ID ? simulation.dynamics->find(simulation.inventory[slot]) : nullptr;
		if (!dynamic || dynamic->getKind() != DynamicKind_Item)
		{
			pd.itemHotbar->setSlot(slot, false, "", nullptr);
			continue;
		}

		const std::shared_ptr<DynamicType>& type = dynamic->getType();

		//The item's own model drawn a moment ago by renderItemIcons, or the flat icon its type came with
		if (Texture* drawn = pd.itemIcons ? pd.itemIcons->getIcon(slot) : nullptr)
			pd.itemHotbar->setSlot(slot, true, type->itemName, drawn, true);
		else
			pd.itemHotbar->setSlot(slot, true, type->itemName, findItemIcon(type->itemIconPath));
	}
}

//How long the picked item takes to turn all the way around in the item bar, milliseconds
static constexpr double itemIconSpinMS = 6000.0;

//How far the items we aren't holding are turned, radians, so they're seen from a corner rather than straight on
static constexpr float itemIconRestAngle = 0.6f;

void LoopClient::renderItemIcons()
{
	//Nothing to draw into, or the bar is away and whatever was drawn last is still what it would show
	if (!pd.itemIcons || !pd.itemHotbar || !pd.itemHotbar->isUp() || !simulation.dynamics)
		return;

	std::vector<ItemIconRenderer::Request> requests(inventorySize);
	float spin = (float)fmod(getTicksMS() / itemIconSpinMS, 1.0) * 6.2831853f;

	for (int slot = 0; slot < inventorySize; slot++)
	{
		std::shared_ptr<Dynamic> dynamic = simulation.inventory[slot] != NO_ID ? simulation.dynamics->find(simulation.inventory[slot]) : nullptr;
		if (!dynamic || dynamic->getKind() != DynamicKind_Item)
			continue;

		requests[slot].model = dynamic->getType()->getModel().get();
		requests[slot].instance = dynamic->getModelInstance();
		requests[slot].angle = slot == pd.itemHotbar->getSelected() ? spin : itemIconRestAngle;
	}

	GpuZone zone(pd.profiler, "Item icons");
	pd.itemIcons->render(pd.shaders, requests, (int)pd.context->getResolution().x, (int)pd.context->getResolution().y);
}

//How far away a name tag still shows, and where it starts fading out, world units (a stud is one)
static constexpr float nameTagRange = 256.0f;
static constexpr float nameTagFadeStart = 150.0f;

//How far over the top of what it belongs to a name tag floats, world units, enough to clear a player's head
static constexpr float nameTagLift = 5.5f;

void LoopClient::updateNameTags()
{
	pd.gui->nameTags.clear();

	if (!simulation.dynamics || !simulation.camera)
		return;

	//The appearance editor covers the scene, so nothing in it is there to put a tag over
	if (pd.appearanceEditor && pd.appearanceEditor->isOpen())
		return;

	const glm::vec2 resolution = pd.context->getResolution();
	const glm::vec3 cameraPosition = simulation.camera->getPosition();

	//Our own player, which the camera starts inside of in first person, never counts as something in the way
	btRigidBody* ownBody = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;

	//Whether the camera has a clear line to a point on the dynamic a tag belongs to, whose own body never blocks it
	auto canSee = [&](const glm::vec3& point, const btRigidBody* theirBody) -> bool
	{
		if (!pd.physicsWorld)
			return true;

		if (glm::distance(point, cameraPosition) < 0.1f)
			return true;

		return pd.physicsWorld->rayHitFraction(g2b3(cameraPosition), g2b3(point), ownBody, theirBody) >= 1.0f;
	};

	for (size_t a = 0; a < simulation.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = (*simulation.dynamics)[a];

		if (!dynamic || dynamic->nameTag.empty() || dynamic->getHidden())
			continue;

		//Our own name would just sit in the middle of our view
		if (dynamic->clientControlled)
			continue;

		//Over the top of it, wherever it's drawn, which is where it is in a vehicle's seat too
		glm::vec3 where = dynamic->getMeshCenter(-1);
		if (const std::shared_ptr<DynamicType>& type = dynamic->getType())
			if (std::shared_ptr<Model> model = type->getModel())
				where.y += model->getColHalfExtents().y + nameTagLift;

		float distance = glm::distance(where, cameraPosition);
		if (distance > nameTagRange)
			continue;

		glm::vec4 clip = simulation.camera->worldToClipSpace(where);
		//Behind the camera
		if (clip.w <= 0.0001f)
			continue;

		//A little past the edges still counts, a tag half off screen shouldn't blink out
		glm::vec3 ndc = glm::vec3(clip) / clip.w;
		if (std::abs(ndc.x) > 1.2f || std::abs(ndc.y) > 1.2f)
			continue;

		//Last, since it's the expensive part: nothing solid between us and them, so a tag doesn't hang in the air over
		//the wall someone stands behind. Their middle or the spot the tag floats at counts, so a head poking over one still shows
		if (!canSee(dynamic->getMeshCenter(-1), dynamic->body) && !canSee(where, dynamic->body))
			continue;

		float fade = distance < nameTagFadeStart ? 1.0f : 1.0f - (distance - nameTagFadeStart) / (nameTagRange - nameTagFadeStart);

		WorldNameTag tag;
		tag.text = dynamic->nameTag;
		tag.position = ImVec2((ndc.x * 0.5f + 0.5f) * resolution.x, (0.5f - ndc.y * 0.5f) * resolution.y);
		tag.color = IM_COL32(
			(int)(std::clamp(dynamic->nameTagColor.r, 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(dynamic->nameTagColor.g, 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(dynamic->nameTagColor.b, 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(fade, 0.0f, 1.0f) * 255.0f));
		tag.distance = distance;

		pd.gui->nameTags.push_back(tag);
	}
}

std::shared_ptr<Dynamic> LoopClient::getOwnPlayer() const
{
	return simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0];
}

void LoopClient::setFreeCamera(bool on)
{
	if (on == freeCamera || !simulation.camera)
		return;

	//Only if the server allows it, and only with a player to come back to
	if (on && (!simulation.freeCameraEnabled || !getOwnPlayer()))
		return;

	freeCamera = on;

	if (on)
	{
		//So it can be put back the way the server had it, see CameraSettingsPacket
		cameraTargetBeforeFlying = simulation.camera->target;
		cameraFreePositionBeforeFlying = simulation.camera->freePosition;
		cameraFreeDirectionBeforeFlying = simulation.camera->freeDirection;
		cameraFreeUpVectorBeforeFlying = simulation.camera->freeUpVector;

		//Where it already is, looking where it already looked, from here on flown by the walking keys, see Camera::control
		simulation.camera->target.reset();
		simulation.camera->freePosition = true;
		simulation.camera->freeDirection = true;
		simulation.camera->freeUpVector = false;
		simulation.camera->setUp(glm::vec3(0, 1, 0));

		//First person had our own model hidden, and there's no target left to show it again
		if (std::shared_ptr<Dynamic> player = getOwnPlayer())
			player->setHidden(false);
	}
	else
	{
		simulation.camera->target = cameraTargetBeforeFlying;
		simulation.camera->freePosition = cameraFreePositionBeforeFlying;
		simulation.camera->freeDirection = cameraFreeDirectionBeforeFlying;
		simulation.camera->freeUpVector = cameraFreeUpVectorBeforeFlying;

		//Whatever it was watching is gone, or it was never on anything: our own player is the sensible thing to come back to
		if (simulation.camera->target.expired())
		{
			simulation.camera->target = getOwnPlayer();
			simulation.camera->freePosition = true;
			simulation.camera->freeDirection = true;
		}

		cameraTargetBeforeFlying.reset();
	}

	if (client)
		client->send(makeFreeCameraPacket(freeCamera), OtherReliable);
}

void LoopClient::dropPlayerAtCamera()
{
	std::shared_ptr<Dynamic> player = getOwnPlayer();
	if (!freeCamera || !player || !simulation.camera)
		return;

	//Our game owns our own player's physics, so this goes to the server the usual way, see sendControlledObjects
	if (player->isInWorld())
	{
		player->setPosition(g2b3(simulation.camera->getPosition()));
		player->setVelocity(btVector3(0, 0, 0));
		player->setAngularVelocity(btVector3(0, 0, 0));
		player->activate();
	}

	setFreeCamera(false);
}

/*
	The preview light is ours alone, like a predicted click's: the server's lights are numbered up from 0 and those
	count down from the top, so one number in the middle can never be mistaken for either
*/
static constexpr netIDType previewLightID = NO_ID / 2;

void LoopClient::updateLightPreview(std::vector<PointLightSource>& lights, netIDType& hiddenLightID)
{
	hiddenLightID = NO_ID;

	netIDType brickID = NO_ID;
	netIDType vehicleID = NO_ID;
	BrickAttachments settings;
	if (!pd.wrenchDialog || !pd.wrenchDialog->getLightPreview(brickID, vehicleID, hiddenLightID, settings))
	{
		lightPreview = nullptr;
		return;
	}

	//The brick's real light, or the vehicle's headlight, stays hidden either way, so unchecking Has light shows it going out
	const Brick* brick = brickID != NO_ID && simulation.bricks ? simulation.bricks->find(brickID) : nullptr;
	std::shared_ptr<Vehicle> vehicle = vehicleID != NO_ID && simulation.vehicles ? simulation.vehicles->find(vehicleID) : nullptr;
	if ((!brick && !vehicle) || !settings.hasLight)
	{
		lightPreview = nullptr;
		return;
	}

	//A headlight's settings are in the vehicle's space, where it's drawn right now, see setVehicleHeadlight
	glm::vec3 position = brick ? brick->getWorldCenter() + settings.lightOffset : vehicle->renderedPosition + vehicle->renderedRotation * (vehicle->headlightMount + settings.lightOffset);
	if (vehicle)
		settings.lightDirection = vehicle->renderedRotation * settings.lightDirection;

	if (!lightPreview)
		lightPreview = Light::makeLocal(position);

	//Where the server would put the light for these settings, see updateBrickAttachments
	Light& light = *lightPreview;
	light.setPosition(position);
	light.setColor(settings.lightColor);
	light.setBrightness(settings.lightBrightness);
	light.setFlicker(settings.lightFlicker);
	light.setBlink(settings.lightBlinkSpeed, settings.lightBlinkStrength);
	light.setCoronaWidth(settings.lightCoronaWidth);
	light.setConeAngle(settings.lightConeAngle);
	light.setDirection(settings.lightDirection);
	light.setSpin(settings.lightSpin);

	uint32_t now = SDL_GetTicks();
	lights.push_back({ previewLightID, light.getRenderedPosition(now), light.getColor(), light.getRenderedBrightness(now),
		light.getCoronaWidth(), light.getRange(), light.getRenderedDirection(now), light.getConeCosine() });
}

/*
	The item in the slot our own item bar has picked, which is what a click acts with. Picked from our
	inventory rather than Item::equipped so it changes the moment we choose a slot, the same way
	placeHeldItems draws it, instead of once the server hears about it
*/
std::shared_ptr<Item> LoopClient::getOwnEquippedItem() const
{
	if (!simulation.dynamics || !pd.itemHotbar || !pd.itemHotbar->isUp())
		return nullptr;

	int slot = pd.itemHotbar->getSelected();
	if (slot < 0 || slot >= inventorySize || simulation.inventory[slot] == NO_ID)
		return nullptr;

	std::shared_ptr<Dynamic> dynamic = simulation.dynamics->find(simulation.inventory[slot]);
	if (!dynamic || dynamic->getKind() != DynamicKind_Item)
		return nullptr;

	return std::static_pointer_cast<Item>(dynamic);
}

void LoopClient::placeHeldItems(float deltaT)
{
	//Where our own item sits in first person, where our hand is hidden: right, up, and back from the camera, negative back is in front of it
	static const glm::vec3 firstPersonGrip = glm::vec3(1.5f, -1.6f, -2.6f);

	if (!simulation.dynamics)
		return;

	for (unsigned int a = 0; a < simulation.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = simulation.dynamics->get(a);
		if (dynamic->getKind() != DynamicKind_Item)
			continue;

		std::shared_ptr<Item> item = std::static_pointer_cast<Item>(dynamic);
		item->updateSwing(deltaT);

		if (!item->held)
		{
			if (item->getHidden())
				item->setHidden(false);

			//An item a brick offers turns slowly on the spot, standing up the way the item bar's icons do, whatever way its body is turned
			if (item->display && item->renderedTransformInitialized)
			{
				float spin = (float)fmod(getTicksMS() / Item::displaySpinMS, 1.0) * 6.2831853f;
				item->setDrawnTransform(item->renderedPosition, glm::angleAxis(spin, glm::vec3(0, 1, 0)));
			}
			continue;
		}

		std::shared_ptr<Dynamic> holder = item->holder.lock();
		if (item->holderID == NO_ID)
			holder = nullptr;
		else if (!holder || holder->getID() != item->holderID)
		{
			holder = simulation.dynamics->find(item->holderID);
			item->holder = holder;
		}

		//One of ours is in hand as soon as we pick its slot, rather than once the server hears about it
		bool equipped = item->equipped;
		for (int slot = 0; slot < inventorySize; slot++)
		{
			if (simulation.inventory[slot] == item->getID())
				equipped = pd.itemHotbar->isUp() && pd.itemHotbar->getSelected() == slot;
		}

		if (!equipped || !holder || !holder->renderedTransformInitialized)
		{
			if (!item->getHidden())
				item->setHidden(true);
			continue;
		}

		if (item->getHidden())
			item->setHidden(false);

		std::shared_ptr<Model> holderModel = holder->getType()->getModel();
		glm::vec3 grip;
		glm::quat facing;

		if (simulation.camera->getFirstPerson() && simulation.camera->target.lock() == holder)
		{
			//In view and turning with the camera, facing the way it looks
			glm::vec3 forward = glm::normalize(simulation.camera->getDirection());
			glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
			right = glm::length(right) > 0.001f ? glm::normalize(right) : glm::vec3(1, 0, 0);
			glm::vec3 up = glm::cross(right, forward);
			facing = glm::quat_cast(glm::mat3(right, up, -forward));
			grip = simulation.camera->getPosition() + facing * firstPersonGrip;
		}
		else
		{
			/*
				In the right hand as it's drawn, facing the way the holder does, but held level: a body lying
				down to crawl or tilted to swim has its face toward the floor, and the gun would go with it.
				Its right stays level through either, so the heading comes off that
			*/
			glm::quat body = holder->renderedTilt * holder->renderedRotation;
			glm::vec3 right = body * glm::vec3(1, 0, 0);
			right.y = 0;
			right = glm::length(right) > 0.001f ? glm::normalize(right) : glm::vec3(1, 0, 0);
			glm::vec3 forward = glm::cross(glm::vec3(0, 1, 0), right);
			facing = glm::quat_cast(glm::mat3(right, glm::vec3(0, 1, 0), -forward));
			int hand = holderModel->getMeshIdx("Right_Hand");
			grip = hand != -1 ? holder->getMeshCenter(hand) : holder->renderedPosition + facing * holderModel->getColOffset();
		}

		//The swing tips it forward around the grip, which is its type's hand offset on the model
		const std::shared_ptr<DynamicType>& type = item->getType();
		glm::quat rotation = facing * glm::angleAxis(item->getSwingAngle(), glm::vec3(1, 0, 0)) * type->handRotation;
		item->setDrawnTransform(grip - rotation * type->handOffset, rotation);
	}
}

std::shared_ptr<Vehicle> LoopClient::getDrivenVehicle() const
{
	if (!simulation.vehicles || simulation.controllers.empty())
		return nullptr;

	std::shared_ptr<Dynamic> player = simulation.controllers[0]->target.lock();
	if (!player)
		return nullptr;

	for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
	{
		if (simulation.vehicles->get(a)->driverID == player->getID())
			return simulation.vehicles->get(a);
	}

	return nullptr;
}

std::shared_ptr<Vehicle> LoopClient::getRiddenVehicle() const
{
	if (!simulation.vehicles || simulation.controllers.empty())
		return nullptr;

	std::shared_ptr<Dynamic> player = simulation.controllers[0]->target.lock();
	if (!player)
		return nullptr;

	for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
	{
		std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);
		if (vehicle->driverID == player->getID())
			return vehicle;

		for (const PassengerSeat& seat : vehicle->passengerSeats)
		{
			if (seat.riderID == player->getID())
				return vehicle;
		}
	}

	return nullptr;
}

void LoopClient::placeVehicleDrivers(float deltaT)
{
	simulation.camera->alsoIgnore = nullptr;

	if (!simulation.vehicles || !simulation.dynamics)
		return;

	std::shared_ptr<Dynamic> followed = simulation.camera->target.lock();

	auto riderOf = [&](netIDType id)
	{
		std::shared_ptr<Dynamic> rider = id != NO_ID ? simulation.dynamics->find(id) : nullptr;
		return rider && rider->getKind() == DynamicKind_Plain ? rider : nullptr;
	};

	for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
	{
		std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);

		//Got out, or someone else got in, before anyone is seated so someone moving seats isn't let out again after
		std::shared_ptr<Dynamic> driver = riderOf(vehicle->driverID);
		if (vehicle->seated.lock() != driver)
			vehicle->releaseSeated(Vehicle::driverSeat, simulation.idealBufferSize);

		std::vector<std::shared_ptr<Dynamic>> passengers(vehicle->passengerSeats.size());
		for (size_t s = 0; s < passengers.size(); s++)
		{
			passengers[s] = riderOf(vehicle->passengerSeats[s].riderID);
			if (vehicle->passengerSeats[s].seated.lock() != passengers[s])
				vehicle->releaseSeated((int)s, simulation.idealBufferSize);
		}

		//Out of the physics world while seated, like a carried item, so they don't bump into the vehicle
		auto place = [&](const std::shared_ptr<Dynamic>& rider, const btTransform& transform)
		{
			if (rider->isInWorld())
				rider->removeFromWorld();

			//Its body sits still out of the world, so anything that goes by how fast it's moving, like the Doppler effect, goes by the vehicle instead
			rider->ridingVehicle = vehicle;

			btQuaternion turn = transform.getRotation();
			rider->body->setWorldTransform(transform);
			rider->setDrawnTransform(b2g3(transform.getOrigin()), glm::quat(turn.w(), turn.x(), turn.y(), turn.z()));
			rider->playWalkingAnimation = false;
			rider->stop(0);

			//Riding in a model vehicle they sit, if their model has a sit animation, and stand back up in Vehicle::releaseSeated
			int sit = vehicle->isModelVehicle() ? rider->getType()->getModel()->getAnimationID("sit") : -1;
			if (sit != -1)
				rider->play(sit, true);

			rider->turnHead(deltaT);

			//A third person camera sees through the vehicle it rides in
			if (rider == followed)
				simulation.camera->alsoIgnore = vehicle->body;
		};

		if (driver)
		{
			vehicle->seated = driver;
			place(driver, vehicle->getSeatTransform(true));
		}

		for (size_t s = 0; s < passengers.size(); s++)
		{
			const std::shared_ptr<Dynamic>& passenger = passengers[s];
			if (!passenger)
				continue;

			vehicle->passengerSeats[s].seated = passenger;

			//Our own player faces where the camera looks right away, others follow their smoothed look, and anyone who hasn't looked anywhere faces the way it drives
			glm::vec3 look = vehicle->renderedRotation * vehicle->forward;
			if (passenger->hasLook)
				look = passenger->clientControlled || !passenger->renderedLookInitialized ? passenger->lookDirection : passenger->renderedLook;

			place(passenger, vehicle->getPassengerTransform((int)s, *passenger, look, true));
		}
	}
}

void LoopClient::placeVehicleWheels()
{
	if (!simulation.vehicles)
		return;

	for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
	{
		std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);

		//A model vehicle's body is drawn where its bricks would be, everything else about it is the same
		if (vehicle->bodyInstance)
			vehicle->bodyInstance->setModelTransform(vehicle->getDrawnTransform());

		//Which model its wheels have is up to the vehicle, see Vehicle::finishClient
		Model* tire = vehicle->getWheelModel();
		if (!tire || vehicle->wheels.empty())
			continue;

		//The tire model's thinnest side is its axle and its widest is its diameter, around the middle of its bounding box
		glm::vec3 halfExtents = tire->getColHalfExtents();
		int axleAxis = 0;
		for (int axis = 1; axis < 3; axis++)
		{
			if (halfExtents[axis] < halfExtents[axleAxis])
				axleAxis = axis;
		}

		glm::vec3 modelAxle(0);
		modelAxle[axleAxis] = 1;
		float modelRadius = std::max(halfExtents[(axleAxis + 1) % 3], halfExtents[(axleAxis + 2) % 3]);
		modelRadius = std::max(modelRadius, 0.0001f);
		glm::mat4 centered = glm::translate(-tire->getColOffset());

		glm::vec3 axle = glm::normalize(glm::cross(glm::vec3(0, -1, 0), vehicle->forward));
		glm::mat4 align = glm::toMat4(glm::rotation(modelAxle, axle));

		for (size_t w = 0; w < vehicle->wheels.size(); w++)
		{
			VehicleWheel& wheel = vehicle->wheels[w];
			if (!wheel.tire)
				continue;

			float scale = wheel.radius / modelRadius;
			wheel.tire->setModelTransform(vehicle->getDrawnWheelTransform((int)w) * align * glm::scale(glm::vec3(scale)) * centered);
		}
	}
}

void LoopClient::renderPartModels(bool useMaterials) const
{
	for (Model* hat : pd.hatModels)
		hat->render(pd.shaders, useMaterials);
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
			//Fullscreen always uses graphics/startresolution from the settings menu instead
			if (!settings->getBool("graphics/startfullscreen"))
			{
				glm::vec2 resolution = pd.context->getResolution();
				pd.state->addInt("window/width", (int)resolution.x, true, "", 1, 10000);
				pd.state->addInt("window/height", (int)resolution.y, true, "", 1, 10000);
				pd.state->exportToFile(ClientProgramData::stateFilePath);
			}

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
				createWaterTargets(settings);
				createGodRayTarget(settings);
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
				//Escape while typing a chat message only cancels it
				if (pd.chatWindow->isTyping())
					pd.chatWindow->stopTyping(true);
				//A saved vehicle being placed or a vehicle selection box is put away before anything opens
				else if (pd.vehicleGhost.isActive() && pd.gui->getOpenWindowCount() == 0)
					pd.vehicleGhost.cancel();
				else if (pd.selectionBox.isActive() && pd.gui->getOpenWindowCount() == 0)
					pd.selectionBox.cancel();
				//The appearance editor's color window and painting close before the editor itself
				else if (pd.appearanceEditor->isOpen() && pd.appearanceEditor->handleEscape())
				{
				}
				else if (pd.gui->getOpenWindowCount() == 0)
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
		else if (e.type == SDL_MOUSEWHEEL && pd.context->getMouseLocked() && !pd.gui->shouldUnlockMouse())
		{
			//Moves through the paint palette while it shows, then item slots while the item bar is out, otherwise hot bar slots while building, like the old game
			int amount = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
			if (!pd.paintMenu->scroll(amount) && !pd.itemHotbar->scroll(amount))
				pd.brickHotbar->scroll(amount);
		}
		else if (e.type == SDL_MOUSEBUTTONDOWN && simulation.camera && !pd.gui->shouldUnlockMouse() && cmdArgs.gameState == InGame && !pd.appearanceEditor->isOpen())
		{
			//A saved vehicle's ghost takes the left click that places it
			if (e.button.button == SDL_BUTTON_LEFT && pd.vehicleGhost.isActive() && pd.context->getMouseLocked())
			{
				if (client && pd.vehicleGhost.hasPlace())
				{
					static uint32_t nextUploadID = 1;
					for (ENetPacket* packet : makeVehicleUploadPackets(nextUploadID++, pd.vehicleGhost.placesAsVehicle(), pd.vehicleGhost.getSpot(), pd.vehicleGhost.getFileBytes()))
						client->send(packet, OtherReliable);
				}
				pd.vehicleGhost.cancel();
				continue;
			}

			//A vehicle selection box takes left clicks for itself while it's out
			if (e.button.button == SDL_BUTTON_LEFT && pd.selectionBox.isActive() && pd.context->getMouseLocked() && pd.physicsWorld)
			{
				btRigidBody* ignore = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
				btVector3 hitPosition, hitNormal;
				glm::vec3 start = simulation.camera->getPosition();
				glm::vec3 direction = simulation.camera->getDirection();
				bool hit = pd.physicsWorld->doRaycast(g2b3(start), g2b3(start + direction * 250.0f), ignore, hitPosition, hitNormal) != nullptr;
				pd.selectionBox.press(start, direction, hit, b2g3(hitPosition));
				continue;
			}

			//Right mouse getting into or out of a vehicle doesn't jet until it's let go
			if (e.button.button == SDL_BUTTON_RIGHT)
			{
				jetSuppressed = getRiddenVehicle() != nullptr;
				if (!jetSuppressed && pd.physicsWorld)
				{
					btRigidBody* ignore = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
					glm::vec3 start = simulation.camera->getPosition();
					btRigidBody* hit = pd.physicsWorld->doRaycast(g2b3(start), g2b3(start + simulation.camera->getDirection() * 30.0f), ignore);
					jetSuppressed = hit && hit->getUserIndex() == vehicleBody;
				}
			}

			int mx, my;
			int mask = SDL_GetMouseState(&mx, &my);

			float x = (float)mx / pd.context->getResolution().x * 2 - 1;
			float y = (float)my / pd.context->getResolution().y * 2 - 1;

			glm::vec3 worldPos = simulation.camera->mouseCoordsToWorldSpace(glm::vec2(x, y));
			glm::vec3 dir = simulation.camera->getDirection();

			/*
				A click with a weapon plays what the server already said it would, right now, rather than
				once the shot comes back. Purely what it looks and sounds like, see Networking/ClickAction.h
			*/
			if (e.button.button == SDL_BUTTON_LEFT && pd.context->getMouseLocked())
			{
				std::shared_ptr<Item> equipped = getOwnEquippedItem();
				if (equipped)
				{
					pd.clickActions->onClick(equipped->getID(),
						pd.particles ? pd.particles->getNowMS() : (double)SDL_GetTicks(), pd.audio.get(), equipped);
				}
			}

			//Our player reaches out on every left click while playing, whether or not it hits anything, and the server shows everyone else
			if (e.button.button == SDL_BUTTON_LEFT && pd.context->getMouseLocked() && !simulation.controllers.empty())
			{
				std::shared_ptr<Dynamic> player = simulation.controllers[0]->target.lock();
				if (player && player->getType()->getModel()->getAnimationID("grab") != -1)
				{
					player->playOneShot(player->getType()->getModel()->getAnimationID("grab"));
					client->send(makePlayerGrabPacket(), Unreliable);
				}
			}

			//Holding the wrench key, a left click wrenches the brick under the crosshair instead, until there's a wrench item
			if (e.button.button == SDL_BUTTON_LEFT && pd.input->isCommandKeydown(Wrench))
				client->send(makeWrenchRequestPacket(simulation.camera->getPosition(), dir), OtherReliable);
			else
			{
				//The mask can't say which button this was if another is held too
				unsigned char pressFlags = e.button.button == SDL_BUTTON_LEFT ? ClickFlag_LeftPress : (e.button.button == SDL_BUTTON_RIGHT ? ClickFlag_RightPress : 0);
				ENetPacket *mouseClickPacket = makeMouseClickPacket(worldPos, dir, mask, false, pressFlags);
				client->send(mouseClickPacket, OtherReliable);

				//While building, a left click puts the ghost brick wherever the crosshair points
				if ((mask & SDL_BUTTON_LMASK) && pd.brickHotbar->isBuilding())
					spawnGhostFromCamera(pd, simulation);
			}
		}
		else if (e.type == SDL_MOUSEBUTTONUP && simulation.camera && client && cmdArgs.gameState == InGame)
		{
			if (e.button.button == SDL_BUTTON_LEFT)
			{
				pd.selectionBox.release();
				//Nothing repeats once the trigger is let go
				pd.clickActions->onRelease();
			}
			if (e.button.button == SDL_BUTTON_RIGHT)
				jetSuppressed = false;

			//Every release goes to the server, even over a window, so whatever holding the button started, like a swing, always ends
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			float x = (float)mx / pd.context->getResolution().x * 2 - 1;
			float y = (float)my / pd.context->getResolution().y * 2 - 1;

			glm::vec3 worldPos = simulation.camera->mouseCoordsToWorldSpace(glm::vec2(x, y));
			client->send(makeMouseClickPacket(worldPos, simulation.camera->getDirection(), (unsigned char)SDL_BUTTON(e.button.button), true), OtherReliable);
		}
	}

	//Interacting with gui, don't move around in-game
	pd.input->supressed = pd.gui->wantsSuppression();

	//Someone just applied setting changes
	if (pd.settingsMenu->pollForChanges())
	{
		//A newly picked resolution resizes a windowed game right away, which is then remembered as the window size on exit
		//Fullscreen picks it up on the next launch
		glm::ivec2 pickedResolution(settings->getInt("graphics/startresolutionx"), settings->getInt("graphics/startresolutiony"));
		if (pickedResolution != pd.appliedStartResolution)
		{
			pd.appliedStartResolution = pickedResolution;
			if (!settings->getBool("graphics/startfullscreen"))
				pd.context->resizeWindow(pickedResolution.x, pickedResolution.y);
		}

		Logger::setDebug(settings->getBool("logger/verbose"));
		simulation.camera->updateSettings(settings);
		applyDrawDistance();
		pd.gui->updateSettings(settings);
		simulation.idealBufferSize = settings->getInt("network/snapshotbuffer");
		createWaterTargets(settings);
		createShadowTarget(settings);
		createGodRayTarget(settings);
		pd.audio->setVolumes(settings->getFloat("audio/mastervolume"), settings->getFloat("audio/musicvolume"));
		pd.audio->setEnvironmentOptions(settings->getInt("audio/reverbquality"), settings->getInt("audio/occlusionquality"));
		pd.acousticProbe.setQuality(settings->getInt("audio/reverbquality"), settings->getInt("audio/occlusionquality"));
		pd.audio->setVoiceVolume(settings->getFloat("audio/voicevolume"));
		pd.voice->setMicrophone(settings->getString("audio/microphone"), settings->getFloat("audio/microphonevolume"));
		if (simulation.brickDebris)
			simulation.brickDebris->setLifetime(settings->getFloat("graphics/brickdebrisseconds"));
		pd.particles->setMaxParticles(settings->getInt("graphics/maxparticles"));
		pd.printVideos.setMaxPlaying(settings->getInt("graphics/maxvideoprints"));
		pd.imageBasedLighting = settings->getBool("graphics/imagebasedlighting");
		pd.depthPrePass = settings->getBool("graphics/depthprepass");
		pd.rain.setQuality(settings->getInt("graphics/rainquality"), pd.textures);

		pd.itemIcons3d = settings->getBool("graphics/itemicons3d");
		if (!pd.itemIcons3d)
			pd.itemIcons.reset();
		else if (!pd.itemIcons)
			pd.itemIcons = std::make_shared<ItemIconRenderer>(pd.textures);
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

	WrenchSubmission wrenchSubmission;
	if (pd.wrenchDialog->takeSubmission(wrenchSubmission) && client)
	{
		if (wrenchSubmission.vehicleID != NO_ID)
			client->send(makeVehicleWrenchSubmitPacket(wrenchSubmission.vehicleID, wrenchSubmission.attachments), OtherReliable);
		else
			client->send(makeWrenchSubmitPacket(wrenchSubmission.brickID, wrenchSubmission.collides, wrenchSubmission.name, wrenchSubmission.attachments), OtherReliable);
	}

	//Picking a print in the print menu puts it on the brick it was opened for, there's nothing else in there to apply
	netIDType printBrickID;
	std::string pickedPrint;
	if (pd.printMenu->takeSubmission(printBrickID, pickedPrint) && client)
		client->send(makePrintSubmitPacket(printBrickID, pickedPrint), OtherReliable);

	//The server sends the vehicle's bricks back for VehicleSaveDataPacket to write
	netIDType saveVehicleID;
	std::string savePath;
	if (pd.wrenchDialog->takeSaveRequest(saveVehicleID, savePath) && client)
	{
		simulation.vehicleSaves[saveVehicleID] = { savePath, "" };
		client->send(makeVehicleSaveRequestPacket(saveVehicleID), OtherReliable);
	}

	//A picked save follows the crosshair as a ghost until a left click places it
	std::string loadPath;
	bool loadAsVehicle = true;
	if (pd.vehicleLoader->takeRequest(loadPath, loadAsVehicle) && client && cmdArgs.gameState == InGame)
	{
		std::ifstream file(loadPath, std::ios::binary);
		std::string bytes = file.is_open() ? std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()) : "";
		std::string failure;
		if (bytes.empty() || bytes.size() > 8 * 1024 * 1024)
			pd.gui->addCenterPrint("Couldn't read " + loadPath, 3000, 1.0f, 0.4f, 0.4f);
		else if (!pd.vehicleGhost.start(std::filesystem::path(loadPath).stem().string(), bytes, loadAsVehicle, pd.brickTypes, pd.prints, failure))
			pd.gui->addCenterPrint(failure, 3000, 1.0f, 0.4f, 0.4f);
		else
		{
			pd.brickHotbar->putAway();
			pd.itemHotbar->putAway();
			pd.selectionBox.cancel();
		}
	}

	netIDType removeVehicleID;
	if (pd.wrenchDialog->takeRemoveRequest(removeVehicleID) && client)
		client->send(makeVehicleRemoveRequestPacket(removeVehicleID), OtherReliable);

	if (vehicleLoaderWasOpen && !pd.vehicleLoader->isOpen() && pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
		pd.context->setMouseLock(true);
	vehicleLoaderWasOpen = pd.vehicleLoader->isOpen();

	//The saved bricks window: the server sends us a save of the bricks to write to our own Saves folder, with a picture we draw
	//of them, and an admin can upload one of those for the server to load
	pd.brickSaveMenu->setAdmin(pd.debugMenu->isAuthenticated());

	std::string saveFileName;
	if (pd.brickSaveMenu->takeSaveRequest(saveFileName) && client && cmdArgs.gameState == InGame)
	{
		std::string savePath = getSavePath(saveFileName);
		if (savePath.empty())
			pd.gui->addCenterPrint("That name can't be used for a file.", 3000, 1.0f, 0.4f, 0.4f);
		else if (!pd.debugMenu->isAuthenticated())
			saveBricksLocally(savePath);
		else
		{
			static uint32_t nextSaveRequestID = 1;
			uint32_t requestID = nextSaveRequestID++;
			simulation.brickSaves[requestID] = { savePath, renderSavePreview(), "" };
			client->send(makeBrickSaveRequestPacket(requestID), OtherReliable);
		}
	}

	std::string loadFileName;
	bool loadClear = false;
	glm::ivec3 loadOffset(0);
	if (pd.brickSaveMenu->takeLoadRequest(loadFileName, loadClear, loadOffset) && client && cmdArgs.gameState == InGame)
	{
		std::string loadPath = getSavePath(loadFileName);
		std::ifstream file(loadPath, std::ios::binary);
		std::string bytes = file.is_open() ? std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()) : "";
		if (bytes.empty())
			pd.gui->addCenterPrint("Couldn't read " + loadPath, 3000, 1.0f, 0.4f, 0.4f);
		else if (bytes.size() > 16 * 1024 * 1024)
			pd.gui->addCenterPrint(loadPath + " is too big to upload", 3000, 1.0f, 0.4f, 0.4f);
		else
		{
			static uint32_t nextBrickUploadID = 1;
			bool blockland = loadFileName.length() > 4 && lowercase(loadFileName.substr(loadFileName.length() - 4)) == ".bls";
			for (ENetPacket* packet : makeBrickUploadPackets(nextBrickUploadID++, loadFileName, blockland, loadClear, loadOffset, bytes))
				client->send(packet, OtherReliable);
			pd.gui->addCenterPrint("Uploading " + loadFileName + "...", 2000, 1.0f, 1.0f, 1.0f);
		}
	}

	if (brickSaveMenuWasOpen && !pd.brickSaveMenu->isOpen() && pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
		pd.context->setMouseLock(true);
	brickSaveMenuWasOpen = pd.brickSaveMenu->isOpen();

	//Applied or closed, back to playing if nothing else is open
	if (wrenchDialogWasOpen && !pd.wrenchDialog->isOpen() && pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
		pd.context->setMouseLock(true);
	wrenchDialogWasOpen = pd.wrenchDialog->isOpen();

	if (printMenuWasOpen && !pd.printMenu->isOpen() && pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
		pd.context->setMouseLock(true);
	printMenuWasOpen = pd.printMenu->isOpen();

	if (colorPickerWasOpen && !pd.paintMenu->isOpen() && pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
		pd.context->setMouseLock(true);
	colorPickerWasOpen = pd.paintMenu->isOpen();

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

	if (pd.serverBrowser->appearanceReady())
	{
		pd.serverBrowser->clearAppearanceReady();
		pd.serverBrowser->close();
		appearanceEditorFromBrowser = true;
		pd.appearanceEditor->open();
	}

	//Saved or not: the browser comes back if that's where it was opened from, otherwise we go back to playing
	if (appearanceEditorWasOpen && !pd.appearanceEditor->isOpen())
	{
		if (appearanceEditorFromBrowser)
			pd.serverBrowser->open();
		else if (pd.gui->getOpenWindowCount() == 0 && cmdArgs.gameState == InGame)
			pd.context->setMouseLock(true);
		appearanceEditorFromBrowser = false;
	}
	appearanceEditorWasOpen = pd.appearanceEditor->isOpen();

	//Saving while connected changes our player right away, if the server's Lua put our appearance on it
	if (pd.appearanceEditor->takeSaved() && client)
		client->send(makeAppearanceChoicePacket(AppearanceEditor::loadAppearance(settings)), OtherReliable);

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

		case OpenSettings:
		{
			pd.settingsMenu->open();
			break;
		}

		case OpenAppearance:
		{
			appearanceEditorFromBrowser = false;
			pd.appearanceEditor->open();
			pd.context->setMouseLock(false);
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

		case OpenSavedVehicles:
		{
			if (cmdArgs.gameState == InGame)
				pd.vehicleLoader->openLoader();
			break;
		}

		case OpenBrickSaves:
		{
			if (cmdArgs.gameState == InGame)
				pd.brickSaveMenu->openMenu();
			break;
		}

		case OpenPlayers:
		{
			//Part of the HUD, so it stays up once the escape menu is gone and the mouse goes back to playing
			if (cmdArgs.gameState == InGame)
			{
				pd.playerList->show();
				if (pd.gui->getOpenWindowCount() == 0)
					pd.context->setMouseLock(true);
			}
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

	//Narrows the view while held, like the old game
	simulation.camera->zooming = pd.input->isCommandKeydown(Zoom);

	if (cmdArgs.gameState == InGame)
		updateFlashlight(deltaT);

	if (pd.input->pollCommand(FirstThirdPerson))
		simulation.camera->swapPerson();

	//The free camera: off our player to fly around, and back onto it wherever we've flown to
	if (cmdArgs.gameState == InGame)
	{
		if (pd.input->pollCommand(DropCameraAtPlayer))
			setFreeCamera(true);

		if (pd.input->pollCommand(DropPlayerAtCamera))
			dropPlayerAtCamera();

		//The server took it away, or our player is gone, so the camera goes back where it was
		if (freeCamera && (!simulation.freeCameraEnabled || !getOwnPlayer()))
			setFreeCamera(false);
		//The server aimed our camera itself, with client:bindCamera or client:staticCamera, which wins over flying it
		else if (freeCamera && (!simulation.camera->target.expired() || !simulation.camera->freePosition))
		{
			freeCamera = false;
			cameraTargetBeforeFlying.reset();
			if (client)
				client->send(makeFreeCameraPacket(false), OtherReliable);
		}
	}

	if (pd.input->pollCommand(DebugView))
		pd.debugMenu->showDebugPhysicsView = !pd.debugMenu->showDebugPhysicsView;

	//Who's here, with their scores and pings, up until the key is hit again
	if (pd.input->pollCommand(TogglePlayerList) && cmdArgs.gameState == InGame)
		pd.playerList->toggle();

	//Talking is a toggle rather than a key you hold down, so it keeps going while you do something else
	if (pd.input->pollCommand(PushToTalk))
		voiceToggled = !voiceToggled;

	//Building
	if (pd.input->pollCommand(OpenBrickSelector))
	{
		pd.brickSelector->open();
		pd.context->setMouseLock(false);
	}

	//Drawing a box around bricks to slice into a vehicle puts bricks and items away, so Enter slices instead of planting
	if (pd.input->pollCommand(StartSelection))
	{
		pd.selectionBox.toggle();
		if (pd.selectionBox.isActive())
		{
			pd.vehicleGhost.cancel();
			pd.brickHotbar->putAway();
			pd.itemHotbar->putAway();
		}
	}
	pd.selectionBox.update(simulation.camera->getPosition(), simulation.camera->getDirection());

	if (pd.vehicleGhost.isActive() && pd.physicsWorld)
	{
		btRigidBody* ignore = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
		std::shared_ptr<Vehicle> driven = getRiddenVehicle();
		pd.vehicleGhost.update(simulation.camera->getPosition(), simulation.camera->getDirection(), *pd.physicsWorld, ignore, driven ? driven->body : nullptr);
	}

	pd.paintMenu->updatePaintKey(pd.input->pollCommand(OpenPaintMenu));

	//The palette coming out puts a paint can in our hand, which outlasts the palette hiding itself again
	if (pd.paintMenu->isPaletteShown() && !paletteWasShown)
	{
		paintCanOut = true;
		pd.itemHotbar->putAway();

		//Just for us, nobody else needs to hear our palette
		pd.audio->playSound("SprayActivate");
	}
	paletteWasShown = pd.paintMenu->isPaletteShown();

	if (pd.input->pollCommand(CustomColor))
	{
		pd.paintMenu->toggleCustomColor();
		if (pd.paintMenu->isOpen())
			pd.context->setMouseLock(false);
	}

	HotbarBrick picked;
	bool slotsChanged = false;
	while (pd.brickSelector->popPick(picked))
	{
		pd.brickHotbar->add(picked);
		slotsChanged = true;
	}

	bool colorChanged = pd.paintMenu->takeChange();
	if (colorChanged && client)
		client->send(makePaintChoicePacket(pd.paintMenu->getColor(), pd.paintMenu->getMaterial()), OtherReliable);
	if (slotsChanged || colorChanged)
	{
		pd.brickHotbar->save(pd.state);
		pd.paintMenu->save(pd.state);

		//Written right away rather than on exit, since not every way of quitting saves
		pd.state->exportToFile(ClientProgramData::stateFilePath);
	}
	pd.brickHotbar->peek = pd.brickSelector->isOpen();

	//The item bar slides out with the picked item in hand, and building puts it away, like the old game only had one out at a time
	if (pd.input->pollCommand(OpenInventory))
	{
		pd.itemHotbar->toggle();
		paintCanOut = false;
		pd.paintMenu->putAway();
		if (pd.itemHotbar->isUp())
			pd.brickHotbar->putAway();
	}

	for (int a = 0; a < BrickHotbar::slotCount; a++)
	{
		if (pd.input->pollCommand((InputCommand)(UseBrick1 + a)))
		{
			pd.brickHotbar->pressSlot(a);
			pd.itemHotbar->putAway();
			paintCanOut = false;
			pd.paintMenu->putAway();
			pd.selectionBox.cancel();
			pd.vehicleGhost.cancel();
		}
	}
	if (pd.input->pollCommand(HideGhostBrick))
		pd.brickHotbar->putAway();

	if (pd.itemHotbar->takeChange() && client)
		client->send(makeInventorySelectPacket(pd.itemHotbar->isUp(), pd.itemHotbar->getSelected()), OtherReliable);

	if (paintCanOut != paintCanSent && client)
	{
		client->send(makePaintCanPacket(paintCanOut), OtherReliable);
		paintCanSent = paintCanOut;
	}

	if (pd.brickHotbar->takeChange())
	{
		HotbarBrick building;
		if (pd.brickHotbar->getSelected(building))
		{
			//Special slots hold a type name, which might not be among the types this copy of the game has
			int special = building.special ? pd.brickTypes.findSpecial(building.name) : -1;
			const SpecialBrickType* type = pd.brickTypes.getSpecial(special);

			if (building.special && !type)
			{
				pd.ghostBrick.hide();
				pd.gui->addCenterPrint("Missing special brick " + building.name, 2000, 1.0f, 0.4f, 0.4f);
			}
			else
			{
				//Starting to build again begins in move mode, like the old game
				if (!pd.ghostBrick.isVisible())
					pd.ghostBrick.setResizeMode(false);

				if (type)
					pd.ghostBrick.select(type->width, type->height, type->length, (uint16_t)(special + 1));
				else
					pd.ghostBrick.select(building.width, building.height, building.length);

				if (!pd.ghostBrick.show())
					spawnGhostFromCamera(pd, simulation);
			}
		}
		else
			pd.ghostBrick.hide();
	}

	pd.ghostBrick.setColor(pd.paintMenu->getColor());
	pd.ghostBrick.setMaterial(pd.paintMenu->getMaterial());
	pd.ghostBrick.update(deltaT, pd.input, simulation.camera->getDirection());

	//Clicks like the old game, if the server registered sounds by these names
	if (pd.ghostBrick.didRotate())
		pd.audio->playSound("ClickRotate");
	if (pd.ghostBrick.didMove())
		pd.audio->playSound("ClickMove");

	//Polled every frame so presses made while the ghost is hidden don't fire later
	bool plantPressed = pd.input->pollCommand(PlantBrick);
	if (plantPressed && pd.selectionBox.getPhase() == SelectionBox::Selecting)
	{
		if (client)
			client->send(makeSliceRequestPacket(pd.selectionBox.getMin(), pd.selectionBox.getMax()), OtherReliable);
		pd.selectionBox.cancel();
	}
	else if (plantPressed && pd.ghostBrick.isVisible())
	{
		//Special types go by the server's ID for them
		Brick planted = pd.ghostBrick.get();
		if (planted.isSpecial())
			planted.typeID = planted.typeID < simulation.brickTypeToServer.size() ? simulation.brickTypeToServer[planted.typeID] : 0;

		if (pd.ghostBrick.get().isSpecial() && !planted.isSpecial())
			pd.gui->addCenterPrint("This server doesn't have that brick", 2000, 1.0f, 0.4f, 0.4f);
		else
			client->send(makePlantBrickPacket(planted), OtherReliable);
	}

	//The key alone does nothing, undo is Ctrl plus the bound key
	//A press removes one brick, holding it keeps removing more after a short delay
	static constexpr float undoRepeatDelayMS = 500.0f;
	static constexpr float undoRepeatIntervalMS = 100.0f;

	bool ctrlDown = SDL_GetModState() & KMOD_CTRL;
	bool undoPressed = pd.input->pollCommand(UndoBrick) && ctrlDown;
	bool undoHeld = ctrlDown && pd.input->isCommandKeydown(UndoBrick);

	if (undoPressed)
	{
		client->send(makeUndoBrickPacket(), OtherReliable);
		undoHeldMS = 0;
		undoSinceRepeatMS = 0;
	}
	else if (undoHeld)
	{
		undoHeldMS += deltaT;
		undoSinceRepeatMS += deltaT;
		if (undoHeldMS > undoRepeatDelayMS && undoSinceRepeatMS > undoRepeatIntervalMS)
		{
			client->send(makeUndoBrickPacket(), OtherReliable);
			undoSinceRepeatMS = 0;
		}
	}
	else
		undoHeldMS = 0;

	//Like undo, Ctrl plus the bound key, which is W by default, so walking doesn't drop anything
	if (pd.input->pollCommand(DropItem) && ctrlDown && client)
		client->send(makeDropItemPacket(pd.itemHotbar->getSelected()), OtherReliable);

	//The next free seat of the vehicle we ride, the server picks which
	if (pd.input->pollCommand(CycleSeat) && client && getRiddenVehicle())
		client->send(makeSeatCyclePacket(), OtherReliable);
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
			if (touched && !touched->clientControlled && !touched->isProjectile && !touchingOtherDynamics(touched))
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

void LoopClient::createWaterTargets(std::shared_ptr<SettingManager> settings)
{
	pd.waterReflection.reset();
	pd.waterRefraction.reset();

	//0 = off, 1 = half resolution, 2 = full resolution
	int quality = settings->getInt("graphics/waterquality");
	if (quality <= 0)
		return;

	int divisor = quality == 1 ? 2 : 1;

	RenderTarget::RenderTargetSettings waterSettings;
	waterSettings.width = std::max(1, (int)pd.context->getResolution().x / divisor);
	waterSettings.height = std::max(1, (int)pd.context->getResolution().y / divisor);

	pd.waterReflection = std::make_shared<RenderTarget>(waterSettings, pd.textures);
	pd.waterRefraction = std::make_shared<RenderTarget>(waterSettings, pd.textures);
}

void LoopClient::createShadowTarget(std::shared_ptr<SettingManager> settings)
{
	pd.shadowSoftness = std::min(std::max(settings->getInt("graphics/shadowsoftness"), 0), 3);

	//0 = off, 1 = 2k, 2 = 4k, capped at what the graphics card allows
	GLint maxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	int picked = std::min(std::max(settings->getInt("graphics/shadowresolution"), 0), 2);
	pd.sunShadows = picked > 0;

	//Turned off, the maps shrink to a single texel rather than going away: model.frag and brick.frag still have
	//ShadowArray bound, and the matrices they look it up with put every surface outside the cascades, so it all reads lit
	int resolution = pd.sunShadows ? (1024 << picked) : 1;
	if (maxTextureSize > 0)
		resolution = std::min(resolution, (int)maxTextureSize);

	if (!pd.shadows || resolution != pd.shadowResolution)
	{
		pd.shadowResolution = resolution;
		//A new set of maps starts empty, so none of the cached cascades are worth keeping
		pd.cascadeDrawn[0] = pd.cascadeDrawn[1] = pd.cascadeDrawn[2] = false;

		RenderTarget::RenderTargetSettings shadowSettings;
		shadowSettings.width = resolution;
		shadowSettings.height = resolution;
		shadowSettings.layers = 3;
		shadowSettings.useColor = false;
		shadowSettings.depthCompare = true;
		pd.shadows.reset();
		pd.shadows = std::make_shared<RenderTarget>(shadowSettings, pd.textures);

		//A new depth texture holds whatever was in that memory, and with the sun's shadows off nothing ever
		//draws into it, so every layer is cleared to fully lit here rather than left to chance
		for (int cascade = 0; cascade < 3; cascade++)
			pd.shadows->useLayer(cascade);
		pd.context->select();
	}

	pd.coloredShadows = settings->getBool("graphics/shadowcolor");

	/*
		graphics/pointshadowquality: 256, 512 or 1024 for every face of every shadowed light's cube. This used to
		be a quarter of the sun's cascade, which tied it to a setting that has nothing to do with it and left no
		way to ask for cheap sun shadows and sharp point ones, or the other way around
		High is the ceiling because the faces are 32 bit depth: 8 lights of 6 faces at 1024 is about 200 MB,
		plus up to another 100 MB of tint maps with colored shadows on
	*/
	int pointQuality = std::min(std::max(settings->getInt("graphics/pointshadowquality"), 0), 2);
	pd.pointLights->setShadowSettings(settings->getInt("graphics/pointshadows"), 256 << pointQuality, pd.coloredShadows, pd.textures);

	//Half resolution to save memory, colored shadows just come out a little softer
	int tintResolution = pd.coloredShadows ? std::max(1, resolution / 2) : 1;
	if (pd.shadowTint && tintResolution == pd.shadowTintResolution)
		return;
	pd.shadowTintResolution = tintResolution;

	RenderTarget::RenderTargetSettings tintSettings;
	tintSettings.width = tintResolution;
	tintSettings.height = tintResolution;
	tintSettings.layers = 3;
	tintSettings.channels = 3;
	tintSettings.depthCompare = true;
	//Untinted light, which each transparent brick then multiplies its color into
	tintSettings.clearColor = glm::vec4(1);
	pd.shadowTint.reset();
	pd.shadowTint = std::make_shared<RenderTarget>(tintSettings, pd.textures);
}

void LoopClient::createGodRayTarget(std::shared_ptr<SettingManager> settings)
{
	//0 = off, then 32 samples per step, see SettingGodRay
	int quality = std::min(std::max(settings->getInt("graphics/godrayquality"), 0), 4);
	pd.godRaySamples = quality * 32;

	pd.godRayMask.reset();
	if (pd.godRaySamples < 1)
		return;

	/*
		Half the width and height of the screen. Nothing shades into it, so what it costs is almost all
		drawing the occluders' shapes, and the rays that come out of it are soft enough that a sharper
		mask wouldn't show. Anything thinner than a couple of pixels does drop out of it, but a wire
		that thin doesn't throw a visible ray either
	*/
	RenderTarget::RenderTargetSettings maskSettings;
	maskSettings.width = std::max(1, (int)pd.context->getResolution().x / 2);
	maskSettings.height = std::max(1, (int)pd.context->getResolution().y / 2);
	//Only how much sun is at each pixel, and no depth to read back, just one to test against
	maskSettings.channels = 1;
	maskSettings.useDepth = false;
	maskSettings.useDepthBuffer = true;
	//A sun just off the edge of the screen still throws rays in, and the march after it has to read
	//black out there rather than smearing the edge of the mask across the sky
	maskSettings.colorWrap = GL_CLAMP_TO_BORDER;

	pd.godRayMask = std::make_shared<RenderTarget>(maskSettings, pd.textures);
	if (!pd.godRayMask->isValid())
	{
		error("Couldn't make the god ray mask, there won't be any god rays");
		pd.godRayMask.reset();
		pd.godRaySamples = 0;
	}
}

/*
	God rays, the light that streams out past the edges of whatever is between the camera and the sun.

	Same two steps the old game used: everything solid is drawn flat black into a small target with the sun
	disc painted into whatever sky it left showing, then every pixel of the screen marches across that mask
	toward the sun and takes light from however much open sky it crossed. A pixel whose line to the sun runs
	along the edge of a roof crosses sky the whole way and comes out bright; one just inside the shadow that
	roof throws out from the sun crosses none and stays dark. It's the difference between neighbours like
	those that reads as a ray.

	It's an extra pass over the bricks and models in view, so it's off unless the player asks for it
*/
void LoopClient::renderGodRays()
{
	if (pd.godRaySamples < 1 || !pd.godRayMask || !simulation.camera)
		return;

	//Only the sun's own disc throws rays, and a .hdr sky has its sun painted into the image with no disc drawn
	if (pd.shaders->skyUniforms.DaySkybox == SkyboxHDR)
		return;

	//Goes to 0 as the sun reaches the horizon and the night sky fades in over it, see Environment::skyboxBlend
	float daylight = 1.0f - pd.environment.skyboxBlend;
	if (daylight <= 0.01f)
		return;

	glm::vec3 sunDirection = glm::normalize(pd.environment.sunDirection);

	/*
		Where the sun is on screen. It's infinitely far away, so only its direction matters, and CameraAngle is
		the view matrix without the camera's position in it. Behind the camera the projection turns that around
		into a mirror image on the wrong side of the screen, so the rays fade out well before the sun gets there
	*/
	glm::vec4 clip = pd.shaders->cameraUniforms.CameraProjection * pd.shaders->cameraUniforms.CameraAngle * glm::vec4(sunDirection, 0.0f);
	float facing = glm::smoothstep(0.0f, 0.25f, glm::dot(glm::normalize(simulation.camera->getDirection()), sunDirection));
	if (clip.w <= 0.0001f || facing <= 0.0f)
		return;

	float strength = facing * daylight * godRayStrength;
	glm::vec2 sunScreen = glm::vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f;

	GpuZone zone(pd.profiler, "God rays");

	//The mask: everything solid in flat black, from exactly the camera the scene was just drawn from
	glm::mat4 cameraMatrix = pd.shaders->cameraUniforms.CameraProjection * pd.shaders->cameraUniforms.CameraView;
	pd.godRayMask->use();

	//renderScene left these set for the grass, and modelShadowCascade.vert reads nonInstanced to decide
	//whether a mesh is placed by the model matrix or by its own instance transform
	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();

	//Models aren't guaranteed to be closed meshes, so both sides block the sun
	glDisable(GL_CULL_FACE);
	pd.shaders->modelShadowCascadeShader->use();
	glUniformMatrix4fv(pd.shadowCascadeMatrixUniformModel, 1, GL_FALSE, &cameraMatrix[0][0]);
	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->render(pd.shaders, false);
	if (pd.tireModel)
		pd.tireModel->render(pd.shaders, false);
	renderPartModels(false);
	glEnable(GL_CULL_FACE);

	//Opaque bricks only: light comes through a transparent one, so it shouldn't cut a ray off
	pd.shaders->brickShadowCascadeShader->use();
	glUniformMatrix4fv(pd.shadowCascadeMatrixUniformBrick, 1, GL_FALSE, &cameraMatrix[0][0]);
	glUniform1f(pd.shadowCascadeMinOpacityUniform, 0.5f);
	glUniform1i(pd.shadowCascadeSkipContainingUniform, 0);
	pd.brickRenderer->renderShadowCascade(cameraMatrix, true, false, false, &vehicleDraws, nullptr, true);

	//The sun disc, which only reaches the pixels none of that covered. Same far plane trick renderScene's sky uses
	pd.shaders->godRayMaskShader->use();
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glBindVertexArray(pd.skyVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	//The rays themselves, added onto the scene: light reaching the camera through the air, never blocked by it
	pd.context->select();
	pd.shaders->godRayShader->use();
	pd.godRayMask->bindColorResult(GodRayMask);
	glUniform2f(pd.godRaySunScreenUniform, sunScreen.x, sunScreen.y);
	glUniform1i(pd.godRaySampleCountUniform, pd.godRaySamples);
	glUniform1f(pd.godRayStrengthUniform, strength);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBindVertexArray(pd.skyVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void LoopClient::applyDrawDistance()
{
	pd.drawDistance = simulation.camera ? simulation.camera->getDrawDistance() : 1000.0f;

	//The far plane clips whatever still gets submitted, but a chunk dropped here is never submitted at all
	if (pd.brickRenderer)
		pd.brickRenderer->setDrawDistance(pd.drawDistance);
}

void LoopClient::renderScene(bool clipAtWater)
{
	//The water's reflection and refraction draw the scene again, which would just overwrite these,
	//so only the pass the player actually sees is broken down
	bool timePasses = !clipAtWater && pd.profiler.isEnabled();

	if (clipAtWater)
		glEnable(GL_CLIP_DISTANCE0);

	if (timePasses)
		pd.profiler.begin("Models");

	pd.shaders->modelShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformModel, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);
	glUniform1i(pd.shaders->modelShader->getUniformLocation("shadowSoftness"), pd.shadowSoftness);
	glUniform1i(pd.shaders->modelShader->getUniformLocation("coloredShadows"), pd.tintShadowsActive);
	pd.shadows->bindDepthResult(ShadowArray);
	pd.shadowTint->bindDepthResult(TintDepthArray);
	pd.shadowTint->bindColorResult(TintColorArray);
	pd.pointLights->bindShadowMaps();
	pd.rain.bindMap();

	//Models:
	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();
	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->render(pd.shaders);
	if (pd.tireModel)
		pd.tireModel->render(pd.shaders);
	renderPartModels(true);

	if (timePasses)
	{
		pd.profiler.end();
		pd.profiler.begin("Grass");
	}

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

	if (timePasses)
	{
		pd.profiler.end();
		pd.profiler.begin("Brick depth pre-pass");
	}

	/*
		Depth pre-pass: the opaque bricks are drawn nearest first into depth alone with almost no fragment work,
		then drawn again for real with the depth test on equal. Bricks behind a wall fail that test and never
		reach model.frag, which in a build you're standing inside is most of them. Early-Z already throws some
		away from the sorted order alone, but only whole chunks at a time, not bricks buried inside one
	*/
	if (pd.depthPrePass)
	{
		pd.shaders->brickDepthShader->use();
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		pd.brickRenderer->renderDepth(pd.shaders);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	if (timePasses)
	{
		pd.profiler.end();
		pd.profiler.begin("Bricks");
	}

	//Bricks, transparent ones last
	pd.shaders->brickShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformBrick, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);
	glUniform1i(pd.shaders->brickShader->getUniformLocation("shadowSoftness"), pd.shadowSoftness);
	glUniform1i(pd.shaders->brickShader->getUniformLocation("coloredShadows"), pd.tintShadowsActive);
	//Everything the pre-pass drew is already in the depth buffer at exactly this depth, so the test has to
	//accept equal or those bricks would all fail it and vanish. brick.vert's gl_Position is invariant so the
	//two programs agree on that depth to the bit
	if (pd.depthPrePass)
		glDepthFunc(GL_LEQUAL);

	pd.brickRenderer->render(pd.shaders, false);
	pd.brickRenderer->renderGroups(pd.shaders, vehicleDraws, false);

	if (pd.depthPrePass)
		glDepthFunc(GL_LESS);

	//Only in the main view, not reflected or refracted by water
	//Debris writes depth, so it goes before anything drawn without depth writes
	if (!clipAtWater && simulation.brickDebris)
		simulation.brickDebris->render(pd.shaders, pd.brickRenderer);

	if (timePasses)
		pd.profiler.end();

	//The sky shader doesn't write gl_ClipDistance, so clipping has to come off before it
	if (clipAtWater)
		glDisable(GL_CLIP_DISTANCE0);

	if (timePasses)
		pd.profiler.begin("Sky");

	/*
		The sky goes last rather than first: sky.vert puts it right on the far plane, so every pixel the
		scene already covered fails the depth test and sky.frag never runs there. Looking at a build that
		fills the screen that's most of them
	*/
	pd.shaders->skyShader->use();
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glBindVertexArray(pd.skyVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	if (timePasses)
		pd.profiler.end();
}

void LoopClient::saveBricksLocally(const std::string& path)
{
	scope("LoopClient::saveBricksLocally");

	if (!simulation.bricks || simulation.bricks->size() == 0)
	{
		pd.gui->addCenterPrint("There are no bricks to save.", 3000, 1.0f, 0.4f, 0.4f);
		return;
	}

	std::vector<const Brick*> all;
	all.reserve(simulation.bricks->size());
	for (size_t a = 0; a < simulation.bricks->size(); a++)
		all.push_back(simulation.bricks->get(a));

	LodSaveInfo saveInfo;
	saveInfo.savedBy = joinedName;
	saveInfo.savedAt = (int64_t)time(nullptr);
	saveInfo.thumbnail = renderSavePreview();

	std::error_code errorCode;
	std::filesystem::create_directories("Saves", errorCode);

	//Our type and print IDs are our own, and we never learn who planted a brick
	std::ofstream file(path, std::ios::binary);
	bool written = file.is_open() && writeLodBricks(file, all, &pd.brickTypes, &pd.prints, true, &saveInfo);
	file.close();

	if (written)
	{
		info("Saved " + std::to_string(all.size()) + " bricks to " + path + " from what we can see of them");
		pd.gui->addCenterPrint("Saved " + std::to_string(all.size()) + " bricks to " + path, 4000, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		error("Couldn't write " + path);
		pd.gui->addCenterPrint("Couldn't write " + path, 4000, 1.0f, 0.4f, 0.4f);
	}

	pd.brickSaveMenu->refresh();
}

//stb's JPEG writer hands out pieces of the file, appended to the string it's given
static void appendPreviewBytes(void* context, void* data, int size)
{
	((std::string*)context)->append((const char*)data, size);
}

std::string LoopClient::renderSavePreview()
{
	scope("LoopClient::renderSavePreview");

	static constexpr int previewSize = 256;

	if (!simulation.bricks || simulation.bricks->size() == 0)
		return "";

	//What the bricks take up, in world units
	glm::vec3 low(FLT_MAX), high(-FLT_MAX);
	for (size_t a = 0; a < simulation.bricks->size(); a++)
	{
		const Brick* brick = simulation.bricks->get(a);
		low = glm::min(low, glm::vec3(brick->x * STUD_SIZE, brick->y * PLATE_SIZE, brick->z * STUD_SIZE));
		high = glm::max(high, glm::vec3((brick->x + brick->footprintWidth()) * STUD_SIZE, (brick->y + brick->height) * PLATE_SIZE, (brick->z + brick->footprintLength()) * STUD_SIZE));
	}

	glm::vec3 center = (low + high) * 0.5f;
	float radius = glm::length(high - low) * 0.5f + 1.0f;

	//Looking down at the build from above one corner, far enough back to be outside all of it
	glm::vec3 direction = glm::normalize(glm::vec3(0.45f, -1.0f, 0.65f));
	glm::vec3 eye = center - direction * (radius * 2.0f + 50.0f);
	glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));

	//An orthographic frame fitted around the corners of the box the bricks are in, square since the picture is
	glm::vec3 viewLow(FLT_MAX), viewHigh(-FLT_MAX);
	for (int corner = 0; corner < 8; corner++)
	{
		glm::vec3 point((corner & 1) ? high.x : low.x, (corner & 2) ? high.y : low.y, (corner & 4) ? high.z : low.z);
		glm::vec3 seen = glm::vec3(view * glm::vec4(point, 1.0f));
		viewLow = glm::min(viewLow, seen);
		viewHigh = glm::max(viewHigh, seen);
	}
	float halfWidth = std::max(viewHigh.x - viewLow.x, viewHigh.y - viewLow.y) * 0.5f * 1.08f;
	glm::vec2 middle((viewLow.x + viewHigh.x) * 0.5f, (viewLow.y + viewHigh.y) * 0.5f);
	//View space looks down -z, and the grass around the build reaches further back than its corners do
	float nearPlane = std::max(-viewHigh.z - 10.0f, 0.1f);
	float farPlane = -viewLow.z + halfWidth * 4.0f + 400.0f;
	glm::mat4 projection = glm::ortho(middle.x - halfWidth, middle.x + halfWidth, middle.y - halfWidth, middle.y + halfWidth, nearPlane, farPlane);

	RenderTarget::RenderTargetSettings settings;
	settings.width = previewSize;
	settings.height = previewSize;
	settings.channels = 3;
	settings.useColor = true;
	settings.useDepth = false;
	settings.useDepthBuffer = true;

	//Noon, whatever time it is, so a build saved at night can still be seen in its picture
	const SkyKeyframe& day = simulation.dayCycle.phases[DaytimePhase];
	settings.clearColor = glm::vec4(day.fogColor, 1.0f);

	RenderTarget target(settings, pd.textures);
	if (!target.isValid())
	{
		error("Couldn't make a render target for the picture");
		return "";
	}

	EnvironmentUniforms previousEnvironment = pd.shaders->environmentUniforms;
	CameraUniforms previousCamera = pd.shaders->cameraUniforms;
	GLint previousPointLightCount = pd.shaders->pointLightUniforms.PointLightCount;

	EnvironmentUniforms& environment = pd.shaders->environmentUniforms;
	environment.SunDirection = glm::normalize(glm::vec3(0.3f, 1.0f, 0.45f));
	environment.LightDirection = environment.SunDirection;
	environment.LightColor = day.lightColor;
	environment.AmbientColor = day.ambientColor;
	environment.SkyColor = day.skyColor;
	environment.FogColor = day.fogColor;
	//No fog inside the frame, and the grass reaches a little past the end of the fog, so this is also how far it goes
	environment.FogDistanceMin = std::max(600.0f, radius * 4.0f);
	environment.FogDistanceMax = environment.FogDistanceMin;
	environment.ShadowStrength = 0.0f;
	environment.RainIntensity = 0.0f;
	environment.RainWetness = 0.0f;
	environment.ClipPlane = glm::vec4(0.0f);
	pd.shaders->updateEnvironmentUBO();

	pd.shaders->pointLightUniforms.PointLightCount = 0;
	pd.shaders->updatePointLightUBO();

	CameraUniforms& camera = pd.shaders->cameraUniforms;
	camera.CameraProjection = projection;
	camera.CameraView = view;
	camera.CameraAngle = glm::mat4(glm::mat3(view));
	camera.CameraPosition = eye;
	camera.CameraDirection = direction;
	pd.shaders->updateCameraUBO();

	//Nowhere near any shadow cascade, so cascadeLight finds every one out of range and returns fully lit
	glm::mat4 noShadow = glm::translate(glm::vec3(1.0e7f, 1.0e7f, 0.0f));
	glm::mat4 lightSpaceMatricies[3] = { noShadow, noShadow, noShadow };

	//The whole build, however far the draw distance is
	pd.brickRenderer->setDrawDistance(0);

	target.use();
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	//Grass, the way renderScene draws it
	pd.shaders->modelShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformModel, 3, GL_FALSE, &lightSpaceMatricies[0][0][0]);
	glUniform1i(pd.shaders->modelShader->getUniformLocation("coloredShadows"), 0);
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

	//Bricks, transparent ones last, without any vehicles' since they aren't saved
	pd.shaders->brickShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformBrick, 3, GL_FALSE, &lightSpaceMatricies[0][0][0]);
	glUniform1i(pd.shaders->brickShader->getUniformLocation("coloredShadows"), 0);
	pd.brickRenderer->render(pd.shaders, false);
	pd.brickRenderer->render(pd.shaders, true);

	//A render target is made with no read buffer, since nothing else reads one back
	std::vector<unsigned char> pixels(previewSize * previewSize * 3);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, previewSize, previewSize, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	//Everything back the way the frame had it
	pd.brickRenderer->setDrawDistance(pd.drawDistance);
	pd.shaders->environmentUniforms = previousEnvironment;
	pd.shaders->updateEnvironmentUBO();
	pd.shaders->cameraUniforms = previousCamera;
	pd.shaders->updateCameraUBO();
	pd.shaders->pointLightUniforms.PointLightCount = previousPointLightCount;
	pd.shaders->updatePointLightUBO();
	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glm::vec2 resolution = pd.context->getResolution();
	glViewport(0, 0, (GLsizei)resolution.x, (GLsizei)resolution.y);

	//OpenGL reads the bottom row first, a picture starts at the top
	std::vector<unsigned char> flipped(pixels.size());
	for (int row = 0; row < previewSize; row++)
		memcpy(flipped.data() + row * previewSize * 3, pixels.data() + (previewSize - 1 - row) * previewSize * 3, previewSize * 3);

	std::string jpeg;
	if (!stbi_write_jpg_to_func(appendPreviewBytes, &jpeg, previewSize, previewSize, 3, flipped.data(), 85))
	{
		error("Couldn't write the picture as a JPEG");
		return "";
	}

	info("Drew a " + std::to_string(jpeg.size()) + " byte picture of " + std::to_string(simulation.bricks->size()) + " bricks for a save");
	return jpeg;
}

void LoopClient::renderTransparent(bool clipAtWater)
{
	if (clipAtWater)
		glEnable(GL_CLIP_DISTANCE0);

	pd.shaders->brickShader->use();
	glUniformMatrix4fv(pd.lightSpaceMatriciesUniformBrick, 3, GL_FALSE, (GLfloat*)pd.lightSpaceMatricies);
	glUniform1i(pd.shaders->brickShader->getUniformLocation("shadowSoftness"), pd.shadowSoftness);
	glUniform1i(pd.shaders->brickShader->getUniformLocation("coloredShadows"), pd.tintShadowsActive);
	pd.shadows->bindDepthResult(ShadowArray);
	pd.shadowTint->bindDepthResult(TintDepthArray);
	pd.shadowTint->bindColorResult(TintColorArray);
	pd.pointLights->bindShadowMaps();
	pd.rain.bindMap();
	pd.brickRenderer->render(pd.shaders, true);
	pd.brickRenderer->renderGroups(pd.shaders, vehicleDraws, true);

	//The box around bricks about to be sliced into a vehicle, and the face under the crosshair that dragging moves
	if (!clipAtWater && pd.selectionBox.isActive() && pd.selectionBox.getPhase() != SelectionBox::WaitingForClick)
	{
		pd.brickRenderer->renderGhost(pd.shaders, pd.selectionBox.getBoxBrick(), 0.0f);
		Brick face;
		if (pd.selectionBox.getFaceBrick(face))
			pd.brickRenderer->renderGhost(pd.shaders, face, 1.0f);
	}

	if (!clipAtWater && pd.ghostBrick.isVisible())
	{
		//Pulses a bit under once a second so the ghost can't be mistaken for a planted transparent brick
		float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() / 1000.0f * 6.2831853f * 0.8f);
		pd.brickRenderer->renderGhost(pd.shaders, pd.ghostBrick.get(), pulse);
	}

	//A saved vehicle about to be placed, pulsing the same way
	if (!clipAtWater && pd.vehicleGhost.hasPlace())
	{
		float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() / 1000.0f * 6.2831853f * 0.8f);
		pd.vehicleGhost.forEachPlaced([&](const Brick& brick) { pd.brickRenderer->renderGhost(pd.shaders, brick, pulse); });
	}

	//Neither writes depth, so they go after everything that does
	pd.particles->render(pd.shaders, pd.lightSpaceMatricies);
	pd.pointLights->renderCoronae(pd.shaders);

	if (clipAtWater)
		glDisable(GL_CLIP_DISTANCE0);
}

void LoopClient::makeWaterRipples(float deltaT)
{
	//Vertical speed, world units per second, below which going in or out of the water doesn't ripple
	static constexpr float crossingSpeed = 1.0f;
	//Slower than this, something at the surface leaves no wake, so floating things bobbing in place stay quiet
	static constexpr float wakeSpeed = 1.0f;

	pd.waterRipples.update(deltaT / 1000.0f);

	if (!simulation.dynamics)
		return;

	for (unsigned int a = 0; a < simulation.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> d = simulation.dynamics->get(a);

		if (!simulation.waterEnabled || !d->renderedTransformInitialized || !d->isInWorld())
		{
			d->rippleStateKnown = false;
			continue;
		}

		//Around where it's drawn, the body of someone else's dynamic can be off between server updates
		btVector3 aabbMin, aabbMax;
		d->body->getAabb(aabbMin, aabbMax);
		glm::vec3 halfSize = b2g3(aabbMax - aabbMin) * 0.5f;
		glm::vec3 center = d->renderedPosition + d->renderedRotation * d->getType()->getModel()->getColOffset();
		float bottom = center.y - halfSize.y;
		float top = center.y + halfSize.y;

		//Same gap between going in and coming out as LoopServer::playWaterSounds
		bool wasInWater = d->rippleInWater;
		if (!wasInWater && bottom < simulation.waterLevel)
			d->rippleInWater = true;
		else if (wasInWater && bottom > simulation.waterLevel + 0.5f)
			d->rippleInWater = false;

		//Just showed up, or water just appeared under it
		if (!d->rippleStateKnown)
		{
			d->rippleStateKnown = true;
			d->rippleWakeDistance = 0;
			continue;
		}

		glm::vec2 surface(center.x, center.z);
		float size = std::max(halfSize.x, halfSize.z);
		//How it's actually moving, see Dynamic::updateSnapshot
		const glm::vec3& velocity = d->tiltVelocity;

		if (d->rippleInWater != wasInWater)
		{
			if (std::abs(velocity.y) > crossingSpeed)
				pd.waterRipples.add(surface, std::clamp(std::abs(velocity.y) / 15.0f, 0.2f, 1.0f) * (d->rippleInWater ? 1.0f : 0.5f), size);
			d->rippleWakeDistance = 0;
		}
		else if (d->rippleInWater && top > simulation.waterLevel)
		{
			//Wading, swimming, or floating along: small short lived ripples every so often
			float speed = glm::length(velocity);
			if (speed > wakeSpeed)
				d->rippleWakeDistance += speed * deltaT / 1000.0f;

			if (d->rippleWakeDistance > std::max(2.0f, size * 1.5f))
			{
				pd.waterRipples.add(surface, std::clamp(speed / 30.0f, 0.1f, 0.35f), size, 1.2f);
				d->rippleWakeDistance = 0;
			}
		}
	}
}

void LoopClient::updateParticles()
{
	//Particles don't get far from their emitters, so emitters well past the end of the fog don't eject any
	static constexpr float ejectPastFog = 64.0f;

	double nowMS = ParticleSystem::getNowMS();
	glm::vec3 cameraPosition = simulation.camera->getPosition();
	//Or the draw distance, if that's nearer, since nothing past it is drawn at all
	float ejectDistance = std::min(pd.environment.fogDistanceMax + ejectPastFog, pd.drawDistance);

	//Where an aimed emitter's particles go: what the dynamic aiming it looks at within the emitter's range, or the end of that
	//For our own player that's what our crosshair is on, otherwise what's in front of the dynamic's eyes the way it looks
	auto findAimTarget = [this](Emitter& emitter, glm::vec3& target) -> bool
	{
		std::shared_ptr<Dynamic> aimer = emitter.aimer.lock();
		if (!aimer || aimer->getID() != emitter.getAimDynamicID())
		{
			aimer = simulation.dynamics ? simulation.dynamics->find(emitter.getAimDynamicID()) : nullptr;
			emitter.aimer = aimer;
		}

		if (!aimer || !pd.physicsWorld)
			return false;

		glm::vec3 drawnAt = aimer->renderedTransformInitialized ? aimer->renderedPosition : b2g3(aimer->getPosition());
		glm::vec3 start = drawnAt + aimer->renderedRotation * aimer->getType()->getModel()->getEyePosition();
		glm::vec3 direction = aimer->hasLook ? aimer->lookDirection : aimer->renderedRotation * glm::vec3(0, 0, -1);
		float range = emitter.getAimRange();

		if (simulation.camera->target.lock() == aimer)
		{
			//A third person camera starts behind the player, so it reaches that much further
			range += glm::distance(simulation.camera->getPosition(), start);
			start = simulation.camera->getPosition();
			direction = simulation.camera->getDirection();
		}

		if (glm::length(direction) < 0.0001f)
			return false;
		direction = glm::normalize(direction);

		btVector3 hitPosition, hitNormal;
		if (pd.physicsWorld->doRaycast(g2b3(start), g2b3(start + direction * range), aimer->body, hitPosition, hitNormal))
			target = b2g3(hitPosition);
		else
			target = start + direction * range;
		return true;
	};

	if (simulation.emitters)
	{
		int64_t ticks = SDL_GetTicks();

		for (unsigned int a = 0; a < simulation.emitters->size(); a++)
		{
			std::shared_ptr<Emitter> emitter = simulation.emitters->get(a);
			glm::vec3 position = emitter->getPosition();
			glm::quat rotation = glm::quat(1, 0, 0, 0);
			glm::vec3 velocity = glm::vec3(0);

			if (emitter->getAttachKind() == EmitterAttachDynamic)
			{
				std::shared_ptr<Dynamic> target = emitter->dynamic.lock();
				if (!target || target->getID() != emitter->getDynamicID())
				{
					target = simulation.dynamics ? simulation.dynamics->find(emitter->getDynamicID()) : nullptr;
					emitter->dynamic = target;
				}

				//Hasn't arrived yet, or is gone and the emitter's removal is on its way
				if (!target)
				{
					ParticleSystem::skipEmission(emitter->clock, position, rotation, nowMS);
					continue;
				}

				rotation = target->getMeshRotation(emitter->getMeshIndex());
				position = target->getMeshCenter(emitter->getMeshIndex()) + rotation * emitter->getDynamicOffset();
				velocity = b2g3(target->getVelocity());
			}
			else if (emitter->getAttachKind() == EmitterAttachVehicle)
			{
				std::shared_ptr<Vehicle> vehicle = emitter->vehicle.lock();
				if (!vehicle || vehicle->getID() != emitter->getDynamicID())
				{
					vehicle = simulation.vehicles ? simulation.vehicles->find(emitter->getDynamicID()) : nullptr;
					emitter->vehicle = vehicle;
				}

				if (!vehicle)
				{
					ParticleSystem::skipEmission(emitter->clock, position, rotation, nowMS);
					continue;
				}

				position = vehicle->renderedPosition + vehicle->renderedRotation * emitter->getVehicleOffset();
				rotation = vehicle->renderedRotation;
				velocity = vehicle->serverVelocity;
			}

			//The server removes it too, this just keeps a short burst from running long while that's on its way
			const EmitterTypeData* type = pd.particles->getEmitterType(emitter->getTypeID());
			bool expired = type && emitter->getAttachKind() != EmitterAttachBrick && emitter->getAttachKind() != EmitterAttachVehicle && type->lifetimeMS > 0 && ticks - emitter->startMS > (int64_t)type->lifetimeMS;

			if (expired || glm::distance(position, cameraPosition) > ejectDistance)
				ParticleSystem::skipEmission(emitter->clock, position, rotation, nowMS);
			else
			{
				glm::vec3 aimTarget;
				bool aimed = emitter->getAimDynamicID() != NO_ID && findAimTarget(*emitter, aimTarget);
				pd.particles->emit(emitter->clock, emitter->getTypeID(), position, rotation, velocity, nowMS, emitter->getTint(), aimed ? &aimTarget : nullptr);
			}
		}
	}

	//Dirt thrown up by vehicle wheels, a darker shade of the brick each one drives on, or brown off of bricks
	static constexpr float dirtShade = 0.7f;
	static const glm::vec4 plainDirt(0.42f, 0.28f, 0.14f, 1.0f);
	if (simulation.vehicles && pd.physicsWorld)
	{
		for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
		{
			std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);
			for (size_t w = 0; w < vehicle->wheels.size(); w++)
			{
				VehicleWheel& wheel = vehicle->wheels[w];
				glm::vec3 center = glm::vec3(vehicle->getDrawnWheelTransform((int)w)[3]);
				glm::vec3 ground = center - glm::vec3(0, wheel.radius, 0);
				const glm::quat upright(1, 0, 0, 0);

				if (!wheel.dirt || vehicle->dirtEmitterType == Vehicle::noEmitterType || glm::distance(ground, cameraPosition) > ejectDistance)
				{
					ParticleSystem::skipEmission(wheel.dirtClock, ground, upright, nowMS);
					continue;
				}

				glm::vec4 tint = plainDirt;
				btVector3 hitPosition, hitNormal;
				btRigidBody* under = pd.physicsWorld->doRaycast(g2b3(center), g2b3(center - glm::vec3(0, wheel.radius + 1.0f, 0)), vehicle->body, hitPosition, hitNormal);
				if (under && under->getUserIndex() == brickBody)
				{
					tint = glm::vec4(glm::vec3(((Brick*)under->getUserPointer())->color) / 255.0f * dirtShade, 1.0f);
					ground = b2g3(hitPosition);
				}

				pd.particles->emit(wheel.dirtClock, vehicle->dirtEmitterType, ground, upright, vehicle->serverVelocity, nowMS, tint);
			}
		}
	}

	pd.particles->update(nowMS, cameraPosition, std::min(pd.environment.fogDistanceMax, pd.drawDistance));
}

/*
	Works out where each of the sun's shadow cascades sits this frame and which of them are redrawn into.
	The near one is small, cheap and right in front of the player, so it always goes. The two farther ones cover
	far more of the build and cost most of the pass, while a frame or two of lag in them can't be seen at that
	distance, so they take turns on an offset count and rarely land on the same frame. A cascade that isn't
	redrawn keeps the matrix it was drawn with, so its map is still looked up in exactly the right place - it
	just sits a little behind where the camera has got to
*/
void LoopClient::pickShadowCascades(bool* drawCascade)
{
	drawCascade[0] = drawCascade[1] = drawCascade[2] = false;

	/*
		With the sun's shadows turned off every surface has to come out lit. Rather than branch in model.frag,
		the cascades are pointed somewhere nothing can be, so cascadeLight finds each one out of range and
		returns fully lit, the same trick AppearanceEditor::renderPreview uses for its turntable
	*/
	if (!pd.sunShadows)
	{
		//Far enough out that no brick can be inside a cascade: the world is bounded well within this
		glm::mat4 noShadow = glm::translate(glm::vec3(1.0e7f, 1.0e7f, 0.0f));
		pd.lightSpaceMatricies[0] = pd.lightSpaceMatricies[1] = pd.lightSpaceMatricies[2] = noShadow;
		pd.cascadeDrawn[0] = pd.cascadeDrawn[1] = pd.cascadeDrawn[2] = false;
		return;
	}

	//Fog fully hides anything past fogDistanceMax, so shadows don't need to reach further
	glm::mat4 freshMatricies[3];
	float freshRadius[3];
	simulation.camera->calculateLightSpaceMatricies(pd.environment.lightDirection, pd.environment.fogDistanceMax, pd.shadowResolution, freshMatricies, freshRadius);

	pd.cascadeFrame++;
	drawCascade[0] = true;
	drawCascade[1] = (pd.cascadeFrame % 2) == 0;
	drawCascade[2] = (pd.cascadeFrame % 4) == 1;

	/*
		A build edit deliberately doesn't force the far ones. They come round on their own within a few frames
		anyway, and forcing them lands all three redraws on the same frame: anything that changes a static sets
		that off, so a server recolouring something twice a second would hitch twice a second. The near cascade
		redraws every frame regardless, so a brick just placed still casts right away where the player is standing
	*/

	glm::vec3 cameraPosition = simulation.camera->getPosition();
	for (int cascade = 0; cascade < 3; cascade++)
	{
		//Never drawn yet, or the camera has moved far enough across the cascade that a stale one would start
		//running off the edges of its map. Neither of those can wait for a turn
		if (!pd.cascadeDrawn[cascade] ||
			glm::length(cameraPosition - pd.cascadeDrawnFrom[cascade]) > pd.cascadeRadius[cascade] * 0.1f)
			drawCascade[cascade] = true;

		if (!drawCascade[cascade])
			continue;

		pd.lightSpaceMatricies[cascade] = freshMatricies[cascade];
		pd.cascadeRadius[cascade] = freshRadius[cascade];
		pd.cascadeDrawnFrom[cascade] = cameraPosition;
		pd.cascadeDrawn[cascade] = true;
	}
}

void LoopClient::renderEverything(float deltaT)
{
	pd.profiler.begin("Update, no drawing");

	//TODO: Get rid of this
	if (simulation.dynamics)
	{
		for (unsigned a = 0; a < simulation.dynamics->size(); a++)
		{
			std::shared_ptr<Dynamic> d = simulation.dynamics->get(a);

			//Carried items are drawn in hand by placeHeldItems
			if (!d->isInWorld())
				continue;

			bool predictingLocally = getTicksMS() < d->predictLocallyUntil;

			if (d->wasPredictingLocally && !predictingLocally)
				d->handOffFromPrediction(simulation.idealBufferSize);
			d->wasPredictingLocally = predictingLocally;

			d->updateSnapshot(deltaT, pd.debugMenu->showDebugPhysicsView || predictingLocally, simulation.waterEnabled ? simulation.waterLevel : PlayerController::noWater);
		}
	}

	//Before the camera, which follows our player while they drive too
	placeVehicleDrivers(deltaT);

	makeWaterRipples(deltaT);

	//Before carried items are placed, so ours in first person keep up with the camera instead of trailing a frame behind
	simulation.camera->render(pd.shaders, deltaT, pd.physicsWorld);

	placeHeldItems(deltaT);
	updateDisplayItemHighlight();

	/*
		Anything a predicted click started keeps going here: its later sounds, and its muzzle flash and
		light, which sit on the item wherever placeHeldItems just drew it, see Networking/ClickAction.h
	*/
	{
		std::shared_ptr<Item> equipped = getOwnEquippedItem();
		bool drawn = equipped && equipped->renderedTransformInitialized && !equipped->getHidden();

		pd.clickActions->update(pd.particles ? pd.particles->getNowMS() : (double)SDL_GetTicks(),
			pd.audio.get(), pd.particles, equipped,
			drawn ? equipped->renderedPosition : glm::vec3(0),
			drawn ? equipped->renderedRotation : glm::quat(1, 0, 0, 0), drawn);
	}

	vehicleDraws.clear();
	if (simulation.vehicles)
	{
		for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
		{
			std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);
			if (vehicle->getBrickGroup() != -1)
				vehicleDraws.push_back({ vehicle->getBrickGroup(), vehicle->getBrickTransform() });
		}
	}

	//Technically rendering related calculations based on previously inputted transform data
	//Anything past the draw distance is left out of the instance buffers entirely, see Model::updateAll
	glm::vec3 modelCullFrom = simulation.camera->getPosition();

	//Before the loop below, since a model vehicle's body and wheels are instances of dynamic types
	placeVehicleWheels();

	for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
		simulation.dynamicTypes[a]->getModel()->updateAll(deltaT, modelCullFrom, pd.drawDistance);

	if (pd.tireModel)
		pd.tireModel->updateAll(deltaT, modelCullFrom, pd.drawDistance);

	//Hats ride on the heads just updated, so their models come after, see Dynamic::placeParts
	if (simulation.dynamics)
	{
		for (unsigned int a = 0; a < simulation.dynamics->size(); a++)
			simulation.dynamics->get(a)->placeParts();
	}
	for (Model* hat : pd.hatModels)
		hat->updateAll(deltaT, modelCullFrom, pd.drawDistance);

	pd.environment.cycle = simulation.dayCycle;
	pd.environment.calc(simulation.worldTimeSeconds);
	pd.environment.passUniforms(pd.shaders);

	//Eases toward the server's rain and moves the map of what's overhead along with the camera, does nothing while it's dry
	pd.rain.update(deltaT, simulation.rainIntensity, simulation.camera->getPosition());
	pd.rain.passUniforms(pd.shaders);

	//Only loads anything when the server picks new skyboxes or graphics/imagebasedlighting changes
	pd.skybox->update(simulation.skyboxPaths[0], simulation.skyboxPaths[1], pd.imageBasedLighting);
	pd.skybox->passUniforms(pd.shaders, pd.environment.skyboxBlend);
	pd.skybox->bind();

	//Every wave in water.vert/frag completes a whole number of cycles per 100 seconds, so wrapping here is seamless
	pd.shaders->environmentUniforms.WaveTime = (float)fmod(getTicksMS() / 1000.0, 100.0);
	pd.shaders->environmentUniforms.WaterLevel = simulation.waterLevel;
	pd.shaders->environmentUniforms.HorizonHeight = simulation.waterEnabled ? std::max(0.0f, simulation.waterLevel) : 0.0f;
	pd.shaders->environmentUniforms.ClipPlane = glm::vec4(0);
	pd.shaders->updateEnvironmentUBO();

	updateParticles();

	//Fits this frame's shadow cascades and says which of them are being redrawn
	bool drawCascade[3];
	pickShadowCascades(drawCascade);

	pd.profiler.end();

	{
		GpuZone zone(pd.profiler, "Brick chunk uploads");
		pd.brickRenderer->rebuildDirty(4.0f);
	}

	//After the chunk uploads, which is what knows whether any brick is wearing a video print at all
	{
		GpuZone zone(pd.profiler, "Video prints");
		pd.printVideos.update(deltaT / 1000.0f, pd.textures,
			[&](uint16_t print) { return pd.brickRenderer->isPrintUsed(print); });
	}

	pd.profiler.begin("Sun shadows");

	//Render shadows to texture, one cascade at a time so each only draws the chunks that can cast into it
	//Casters between the light and a cascade still shadow it, flattened onto its near plane instead of clipped away
	glEnable(GL_DEPTH_CLAMP);
	//Pushes casters back by their slope, more for wider filters, so a filter's outer samples don't land on the surface being shaded
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f + (float)pd.shadowSoftness, 1.0f);

	pd.shaders->basicUniforms.nonInstanced = 0;
	pd.shaders->basicUniforms.cameraSpacePosition = 0;
	pd.shaders->updateBasicUBO();

	//With colored shadows on, transparent bricks tint the light passing through them instead of blocking it
	pd.tintShadowsActive = pd.coloredShadows && pd.brickRenderer->hasTransparentBricks();

	//A point light's shadows leave out the bricks it's inside, so a light in the middle of its brick shines out of it. The sun's leave out none
	auto setSkippedPoint = [](GLint containingUniform, GLint pointUniform, const glm::vec3* lightPosition)
	{
		glUniform1i(containingUniform, lightPosition ? 1 : 0);
		if (lightPosition)
			glUniform3fv(pointUniform, 1, &(*lightPosition)[0]);
	};

	//Into the bound layer of a tint map, for the sun's cascades (no light position) and point lights' cube faces alike
	auto drawShadowTint = [this, setSkippedPoint](const glm::mat4& lightSpaceMatrix, const glm::vec3* lightPosition)
	{
		glCullFace(GL_FRONT);

		//Depth of the transparent brick nearest the light, so surfaces in front of it aren't tinted
		//Every transparent brick tints however see-through it is, the tint just gets fainter
		pd.shaders->brickShadowCascadeShader->use();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformBrick, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		glUniform1f(pd.shadowCascadeMinOpacityUniform, 0.0f);
		setSkippedPoint(pd.shadowCascadeSkipContainingUniform, pd.shadowCascadeSkipPointUniform, lightPosition);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		pd.brickRenderer->renderShadowCascade(lightSpaceMatrix, false, true, false, &vehicleDraws, lightPosition);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		//Each channel keeps the least light any transparent brick along the way lets through, so order doesn't matter and
		//a wall of stacked transparent plates tints like one brick instead of compounding into a plain shadow
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendEquation(GL_MIN);
		pd.shaders->brickShadowTintShader->use();
		glUniformMatrix4fv(pd.shadowTintMatrixUniform, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		glUniform1f(pd.shadowTintMinOpacityUniform, 0.0f);
		setSkippedPoint(pd.shadowTintSkipContainingUniform, pd.shadowTintSkipPointUniform, lightPosition);
		pd.brickRenderer->renderShadowCascade(lightSpaceMatrix, false, true, true, &vehicleDraws, lightPosition);
		glBlendEquation(GL_FUNC_ADD);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);

		glCullFace(GL_BACK);
	};

	//Only timed on the frames they're actually drawn, so these read as what a cascade costs when it goes,
	//not what it averages per frame. The Sun shadows row around them is the per frame number
	static const char* cascadeZones[3] = { "Cascade 0 (near)", "Cascade 1 (mid)", "Cascade 2 (far)" };

	//Nothing is drawn into any of them while they're off, and pickShadowCascades has pointed them out of the way
	for (int cascade = 0; cascade < 3 && pd.sunShadows; cascade++)
	{
		//Its map and its matrix are both still the ones from the frame it was last drawn on
		if (!drawCascade[cascade])
			continue;

		GpuZone zone(pd.profiler, cascadeZones[cascade]);
		pd.shadows->useLayer(cascade);

		//Models aren't guaranteed to be closed meshes, so both sides cast
		glDisable(GL_CULL_FACE);
		pd.shaders->modelShadowCascadeShader->use();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformModel, 1, GL_FALSE, &pd.lightSpaceMatricies[cascade][0][0]);
		for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
			simulation.dynamicTypes[a]->render(pd.shaders, false);
		if (pd.tireModel)
			pd.tireModel->render(pd.shaders, false);
		renderPartModels(false);
		glEnable(GL_CULL_FACE);

		//Bricks are closed boxes, so only their far sides are drawn, which leaves a whole brick between a lit face and the depth it's compared to
		glCullFace(GL_FRONT);
		pd.shaders->brickShadowCascadeShader->use();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformBrick, 1, GL_FALSE, &pd.lightSpaceMatricies[cascade][0][0]);
		//Without colored shadows, mostly see-through bricks don't block any light
		glUniform1f(pd.shadowCascadeMinOpacityUniform, 0.5f);
		setSkippedPoint(pd.shadowCascadeSkipContainingUniform, pd.shadowCascadeSkipPointUniform, nullptr);
		pd.brickRenderer->renderShadowCascade(pd.lightSpaceMatricies[cascade], true, !pd.tintShadowsActive, false, &vehicleDraws);

		if (pd.tintShadowsActive)
		{
			pd.shadowTint->useLayer(cascade);
			drawShadowTint(pd.lightSpaceMatricies[cascade], nullptr);
		}

		glCullFace(GL_BACK);
	}

	pd.profiler.end();

	//Point light shadows use perspective views that already start right at the light
	glDisable(GL_DEPTH_CLAMP);

	std::vector<PointLightSource> lightSources;

	//Lights a predicted click is shining, which are ours alone and aren't objects the server knows about
	pd.clickActions->addLights(lightSources);

	//Same for the light the wrench dialog is editing, which stands in for the brick's real one while it's open
	netIDType hiddenLightID = NO_ID;
	updateLightPreview(lightSources, hiddenLightID);

	if (simulation.lights)
	{
		Uint32 now = SDL_GetTicks();
		lightSources.reserve(simulation.lights->size());
		for (unsigned int a = 0; a < simulation.lights->size(); a++)
		{
			std::shared_ptr<Light> light = simulation.lights->get(a);

			//The wrench dialog is showing what this one would look like with the settings being edited instead
			if (light->getID() == hiddenLightID)
				continue;

			glm::vec3 position = light->getRenderedPosition(now);
			glm::vec3 direction = light->getRenderedDirection(now);

			//Flashlights, skipped until the player holding one arrives
			if (light->getHolderID() != NO_ID && !placeHeldLight(*light, position, direction))
				continue;

			//Lights carried over onto a vehicle from its bricks, where they are on it as it's drawn
			if (light->getVehicleID() != NO_ID)
			{
				std::shared_ptr<Vehicle> vehicle = light->vehicle.lock();
				if (!vehicle || vehicle->getID() != light->getVehicleID())
				{
					vehicle = simulation.vehicles ? simulation.vehicles->find(light->getVehicleID()) : nullptr;
					light->vehicle = vehicle;
				}

				if (!vehicle)
					continue;

				position = vehicle->renderedPosition + vehicle->renderedRotation * position;
				direction = vehicle->renderedRotation * direction;
			}

			lightSources.push_back({ light->getID(), position, light->getColor(), light->getRenderedBrightness(now), light->getCoronaWidth(), light->getRange(),
				direction, light->getConeCosine() });
		}
	}

	const CameraUniforms& view = pd.shaders->cameraUniforms;
	pd.pointLights->update(lightSources, pd.shaders, view.CameraPosition, view.CameraProjection * view.CameraView,
		std::min(pd.environment.fogDistanceMax, pd.drawDistance));

	//Dynamics can move or animate at any time, so a light with one in range redraws its shadows every frame
	auto movingCastersNear = [this](const glm::vec3& position, float range) -> bool
	{
		if (!simulation.dynamics)
			return false;

		for (unsigned int a = 0; a < simulation.dynamics->size(); a++)
		{
			//A carried item is where its holder is, who counts already
			if (!simulation.dynamics->get(a)->isInWorld())
				continue;

			btVector3 aabbMin, aabbMax;
			simulation.dynamics->get(a)->body->getAabb(aabbMin, aabbMax);
			glm::vec3 closest = glm::clamp(position, b2g3(aabbMin), b2g3(aabbMax));
			if (glm::dot(closest - position, closest - position) < range * range)
				return true;
		}

		//Only moving vehicles, a parked one's shadow stays where it was last drawn
		for (unsigned int a = 0; simulation.vehicles && a < simulation.vehicles->size(); a++)
		{
			std::shared_ptr<Vehicle> vehicle = simulation.vehicles->get(a);
			if (!vehicle->body || glm::length(vehicle->serverVelocity) < 0.05f)
				continue;

			btVector3 aabbMin, aabbMax;
			vehicle->body->getAabb(aabbMin, aabbMax);
			glm::vec3 closest = glm::clamp(position, b2g3(aabbMin), b2g3(aabbMax));
			if (glm::dot(closest - position, closest - position) < range * range)
				return true;
		}
		return false;
	};

	auto drawPointShadowCasters = [this, setSkippedPoint](const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPosition, bool tinted)
	{
		glDisable(GL_CULL_FACE);
		pd.shaders->modelShadowCascadeShader->use();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformModel, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		for (unsigned int a = 0; a < simulation.dynamicTypes.size(); a++)
			simulation.dynamicTypes[a]->render(pd.shaders, false);
		if (pd.tireModel)
			pd.tireModel->render(pd.shaders, false);
		renderPartModels(false);
		glEnable(GL_CULL_FACE);

		//Like the sun: transparent bricks tint the light instead of blocking it, or without colored shadows the ones at least half opaque block it
		glCullFace(GL_FRONT);
		pd.shaders->brickShadowCascadeShader->use();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformBrick, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		glUniform1f(pd.shadowCascadeMinOpacityUniform, 0.5f);
		setSkippedPoint(pd.shadowCascadeSkipContainingUniform, pd.shadowCascadeSkipPointUniform, &lightPosition);
		pd.brickRenderer->renderShadowCascade(lightSpaceMatrix, true, !tinted, false, &vehicleDraws, &lightPosition);
		glCullFace(GL_BACK);
	};

	auto drawPointShadowTint = [drawShadowTint](const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPosition)
	{
		drawShadowTint(lightSpaceMatrix, &lightPosition);
	};

	{
		GpuZone zone(pd.profiler, "Point light shadows");
		pd.pointLights->renderShadows(pd.brickRenderer->getGeneration() + simulation.staticsChanged, pd.tintShadowsActive, movingCastersNear, drawPointShadowCasters, drawPointShadowTint);
	}

	glDisable(GL_POLYGON_OFFSET_FILL);

	//The rain map: the topmost brick at each spot around the camera, seen straight down, only redrawn when it moves or bricks change
	//Nearly see-through bricks let rain through, and players and other dynamics don't keep it off anything
	if (pd.rain.mapNeedsDrawing(pd.brickRenderer->getGeneration(), !vehicleDraws.empty(), deltaT))
	{
		GpuZone zone(pd.profiler, "Rain map");

		//Bricks above the map's top are flattened onto it, so they still count as overhead
		glEnable(GL_DEPTH_CLAMP);
		glDisable(GL_CULL_FACE);
		pd.shaders->brickShadowCascadeShader->use();
		glm::mat4 mapMatrix = pd.rain.useMap();
		glUniformMatrix4fv(pd.shadowCascadeMatrixUniformBrick, 1, GL_FALSE, &mapMatrix[0][0]);
		glUniform1f(pd.shadowCascadeMinOpacityUniform, 0.05f);
		glUniform1i(pd.shadowCascadeSkipContainingUniform, 0);
		pd.brickRenderer->renderShadowCascade(mapMatrix, true, true, false, &vehicleDraws);
		glEnable(GL_CULL_FACE);
		glDisable(GL_DEPTH_CLAMP);
	}

	bool cameraUnderwater = simulation.camera->getPosition().y < simulation.waterLevel;
	bool renderWaterPasses = simulation.waterEnabled && pd.waterReflection && pd.waterRefraction;

	auto setClipPlane = [this](const glm::vec4& plane)
	{
		pd.shaders->environmentUniforms.ClipPlane = plane;
		pd.shaders->updateEnvironmentUBO();
	};

	//Keep a little past the surface so wave troughs don't open gaps where objects meet the water
	const float clipOverlap = 0.25f;

	if (renderWaterPasses)
	{
		GpuZone zone(pd.profiler, "Water reflect/refract");

		//Reflection: the scene above the water, from a camera mirrored below the surface
		if (!cameraUnderwater)
		{
			pd.waterReflection->use();
			simulation.camera->uploadReflectionUniforms(pd.shaders, simulation.waterLevel);
			setClipPlane(glm::vec4(0, 1, 0, clipOverlap - simulation.waterLevel));
			renderScene(true);
			renderTransparent(true);
			simulation.camera->uploadUniforms(pd.shaders);
		}

		//Refraction: whatever is on the other side of the surface from the camera
		pd.waterRefraction->use();
		if (cameraUnderwater)
			setClipPlane(glm::vec4(0, 1, 0, clipOverlap - simulation.waterLevel));
		else
			setClipPlane(glm::vec4(0, -1, 0, clipOverlap + simulation.waterLevel));
		renderScene(true);
		renderTransparent(true);

		setClipPlane(glm::vec4(0));
	}

	//Start rendering to screen:
	pd.profiler.begin("Scene to screen");
	pd.context->select();
	pd.context->clear(pd.environment.fogColor.r, pd.environment.fogColor.g, pd.environment.fogColor.b);
	renderScene(false);
	pd.profiler.end();

	if (simulation.waterEnabled)
	{
		GpuZone zone(pd.profiler, "Water surface");
		pd.shaders->waterShader->use();
		pd.pointLights->bindShadowMaps();
		//Always reaches past the end of the fog, so its edge is never visible, unless the draw distance cuts it
		//nearer - then it ends exactly where the bricks and models around it do rather than carrying on past them
		float surfaceRadius = std::min(std::max(waterRadius, pd.environment.fogDistanceMax + 10.0f), pd.drawDistance);
		glUniform1f(pd.shaders->waterShader->getUniformLocation("waterRadius"), surfaceRadius);
		glUniform1f(pd.shaders->waterShader->getUniformLocation("gridSpacing"), surfaceRadius * 2.0f / waterGridCells);
		glUniform1i(pd.shaders->waterShader->getUniformLocation("useReflection"), renderWaterPasses && !cameraUnderwater);
		glUniform1i(pd.shaders->waterShader->getUniformLocation("useRefraction"), renderWaterPasses);
		glUniform1i(pd.shaders->waterShader->getUniformLocation("cameraUnderwater"), cameraUnderwater);
		pd.waterRipples.upload(pd.shaders->waterShader, simulation.camera->getPosition());

		if (renderWaterPasses)
		{
			pd.waterReflection->bindColorResult(Reflection);
			pd.waterRefraction->bindColorResult(Refraction);
		}

		//Visible from both above and below
		glDisable(GL_CULL_FACE);
		glBindVertexArray(pd.waterVao);
		glDrawArrays(GL_TRIANGLES, 0, pd.waterVertexCount);
		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
	}

	//After the water, which writes depth, so water behind a transparent brick can't paint over it
	{
		GpuZone zone(pd.profiler, "Transparent, particles, coronae");
		renderTransparent(false);
	}

	//Rain drops and splashes, not seen from under the water
	if (!(simulation.waterEnabled && cameraUnderwater))
	{
		GpuZone zone(pd.profiler, "Rain drops");
		pd.rain.render(pd.shaders, pd.skyVao, pd.context->getResolution().y);
	}

	//Sunlight streaming past whatever is between the camera and the sun. Under the water there's no sun disc
	//drawn to come from, the surface overhead is in the way of it
	if (!(simulation.waterEnabled && cameraUnderwater))
		renderGodRays();

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

	//Blue tint, slow waves, darker edges, and the scene wobbling like light bending through moving water while the camera is under the water,
	//and the colored vignette from Lua's client:setVignette wobbling it the same way, both drawn in one pass over the finished scene
	bool underwaterEffect = simulation.waterEnabled && cameraUnderwater;
	ScreenVignette& vignette = simulation.vignette;
	if (vignette.active())
		vignette.elapsedMS += deltaT;
	bool vignetteEffect = vignette.active();
	if (underwaterEffect || vignetteEffect)
	{
		//The finished scene is copied out first so underwater.frag can draw it back warped
		int screenWidth = std::max(1, (int)pd.context->getResolution().x);
		int screenHeight = std::max(1, (int)pd.context->getResolution().y);
		if (!pd.underwaterScene || pd.underwaterScene->settings.width != screenWidth || pd.underwaterScene->settings.height != screenHeight)
		{
			RenderTarget::RenderTargetSettings sceneSettings;
			sceneSettings.width = screenWidth;
			sceneSettings.height = screenHeight;
			sceneSettings.channels = 4;
			sceneSettings.useDepth = false;
			sceneSettings.useDepthBuffer = false;
			pd.underwaterScene.reset();
			pd.underwaterScene = std::make_shared<RenderTarget>(sceneSettings, pd.textures);

			//A driver might not resolve the screen into this format, so only the first copy is checked, glGetError can stall every frame
			for (int a = 0; a < 16 && glGetError() != GL_NO_ERROR; a++);
			pd.underwaterScene->copyFromScreen();
			pd.underwaterSceneCopies = pd.underwaterScene->isValid() && glGetError() == GL_NO_ERROR;
			if (!pd.underwaterSceneCopies)
				error("Couldn't copy the screen for the underwater and vignette effects, they won't be distorted");
		}
		else if (pd.underwaterSceneCopies)
			pd.underwaterScene->copyFromScreen();

		//Both the color and the wobble die away together as the vignette's time runs out
		float fade = vignette.fade();
		glm::vec4 vignetteColor(vignette.color, vignetteEffect ? vignette.alpha * fade : 0.0f);
		float vignetteWave = vignetteEffect ? vignette.strength * fade : 0.0f;

		Program* effect = pd.shaders->underwaterShader;
		effect->use();
		glUniform1i(effect->getUniformLocation("distort"), pd.underwaterSceneCopies);
		glUniform1i(effect->getUniformLocation("underwater"), underwaterEffect);
		glUniform4fv(effect->getUniformLocation("vignetteColor"), 1, &vignetteColor[0]);
		glUniform1f(effect->getUniformLocation("vignetteWave"), vignetteWave);
		if (pd.underwaterSceneCopies)
			pd.underwaterScene->bindColorResult(ScreenCopy);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		//Distorted, it redraws the whole picture itself, otherwise just the tints are blended on top
		if (!pd.underwaterSceneCopies)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		glBindVertexArray(pd.skyVao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}

	//Covers the scene while picking how our player looks
	if (pd.appearanceEditor->isOpen())
		pd.appearanceEditor->renderPreview(pd.shaders, (int)pd.context->getResolution().x, (int)pd.context->getResolution().y, deltaT);

	//After everything drawn into the scene, which these have nothing to do with, and before the bar that shows them
	renderItemIcons();
	updateItemHotbar();

	//GUI
	updateNameTags();

	bool crossHair = false;
	if (simulation.camera)
		crossHair = pd.context->getMouseLocked() && simulation.camera->getFirstPerson();

	std::vector<std::string> hudLines;
	if (pd.debugMenu->showDebugPhysicsView)
		hudLines.push_back(std::string("Debug physics view ON (") + SDL_GetScancodeName(pd.input->getKeyBind(DebugView)) + " toggles)");
	if (pd.ghostBrick.isVisible())
	{
		const Brick& ghost = pd.ghostBrick.get();
		std::string material = ghost.material != BrickMaterial_None ? " " + std::string(brickMaterialNames[ghost.material]) : "";
		if (const SpecialBrickType* type = pd.brickTypes.getSpecial(ghost.typeID - 1))
			hudLines.push_back("Ghost brick " + type->uiName + material + ": IJKL move, . , up/down, U rotate, Left Alt super shift, Enter plant, / put away, Ctrl+Z undo");
		else
			hudLines.push_back("Ghost brick " + std::to_string(ghost.width) + "x" + std::to_string(ghost.height) + "x" + std::to_string(ghost.length) + material +
				": IJKL move, . , up/down, U rotate, Left Shift resize, Left Alt super shift, Enter plant, / put away, Ctrl+Z undo");
	}

	if (pd.selectionBox.getPhase() == SelectionBox::WaitingForClick)
		hudLines.push_back("Selecting bricks for a vehicle: click where the vehicle is, Escape cancels");
	else if (pd.selectionBox.isActive())
	{
		glm::ivec3 size = pd.selectionBox.getMax() - pd.selectionBox.getMin();
		hudLines.push_back("Vehicle selection " + std::to_string(size.x) + "x" + std::to_string(size.z) + " studs, " + std::to_string(size.y) +
			" plates tall: drag its faces, Enter slices every brick touching it into a vehicle, Escape cancels");
	}
	if (pd.vehicleGhost.isActive())
		hudLines.push_back("Placing " + pd.vehicleGhost.getName() + (pd.vehicleGhost.placesAsVehicle() ? " as a vehicle" : " as bricks") + ": aim with the crosshair, left click places it, Escape cancels");
	if (freeCamera)
		hudLines.push_back("Free camera: the walking keys fly it, " + std::string(SDL_GetScancodeName(pd.input->getKeyBind(DropPlayerAtCamera))) +
			" drops your player here. Your player stays where you left it, and a yellow light shows everyone where the camera is");

	if (std::shared_ptr<Vehicle> driven = getDrivenVehicle())
	{
		std::string line = "Driving: W/S drive, A/D steer, jump brakes, left click honks, right click gets out";
		if (driven->hasHeadlight)
			line += ", " + std::string(SDL_GetScancodeName(pd.input->getKeyBind(Flashlight))) + " switches its headlight";
		hudLines.push_back(line);
	}
	else if (getRiddenVehicle())
		hudLines.push_back("Riding: right click gets off");

	pd.escapeMenu->showLeaveServer = client != nullptr;
	pd.gui->superShiftIndicator = pd.ghostBrick.isVisible() ? (pd.ghostBrick.isSuperShift() ? 1 : 0) : -1;
	pd.gui->resizeIndicator = pd.ghostBrick.isVisible() ? (pd.ghostBrick.isResizeMode() ? 1 : 0) : -1;
	pd.gui->voiceIndicator = !client ? -1 : (pd.voice->isTransmitting() ? 1 : (pd.voice->isMuted() ? 0 : -1));
	pd.gui->voiceSpeakers = pd.voice->getSpeaking();
	pd.gui->voiceLevel = pd.voice->getInputLevel();
	pd.gui->voiceClipping = pd.voice->isClipping();
	pd.gui->setMouseCaptured(pd.context->getMouseLocked());
	//The appearance editor fills the screen with your player, not the world the brick bar, item bar, and palette belong to
	pd.gui->hideGameHud = pd.appearanceEditor && pd.appearanceEditor->isOpen();
	{
		GpuZone zone(pd.profiler, "GUI");
		pd.gui->render(pd.context->getResolution().x, pd.context->getResolution().y,crossHair,hudLines);
	}

	//End frame
	{
		GpuZone zone(pd.profiler, "Swap");
		pd.context->swap();
	}

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
	//Jets while right mouse is held, only while the mouse is captured for playing rather than clicking around a window
	bool jet = simulation.jetsEnabled && pd.context->getMouseLocked() && !pd.input->supressed && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) && !jetSuppressed && !getRiddenVehicle();
	bool firing = pd.context->getMouseLocked() && !pd.input->supressed && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);

	//Left shift lies the player down. It shares its key with the ghost brick's resize mode, so it waits until they're done building
	bool crawl = pd.context->getMouseLocked() && !pd.input->supressed && pd.input->isCommandKeydown(Crawl) && !pd.ghostBrick.isVisible() && !getRiddenVehicle();

	//Go through player controllers, remove any that are bound to now deleted dynamics
	auto ctrlIter = simulation.controllers.begin();
	while (ctrlIter != simulation.controllers.end())
	{
		//Apply movement inputs client side 
		float waterLevel = simulation.waterEnabled ? simulation.waterLevel : PlayerController::noWater;
		//The keys fly the camera instead while it's loose, so the player they belong to stands where it was left
		(*ctrlIter)->sendQuickly = firing || freeCamera;
		bool gone = freeCamera ?
			(*ctrlIter)->control(pd.physicsWorld, deltaT, simulation.camera->getDirection(), simulation.camera->getPosition(),
				false, false, false, false, false, false, false, false, waterLevel) :
			(*ctrlIter)->control(pd.input, simulation.camera, deltaT, pd.physicsWorld, jet, crawl, waterLevel);
		if (gone)
		{
			ctrlIter = simulation.controllers.erase(ctrlIter);
			continue;
		}
		else
		{
			if ((*ctrlIter)->jumped)
				pd.audio->playSound("Jump");

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
	//-profile times every pass from launch and writes a breakdown to the log each window, so a scene can be
	//measured without anything having to click through the UI
	pd.profiler.setEnabled(cmdArgs.profileRendering || pd.debugMenu->wantsPassTimings());

	/*
		The whole frame is timed, not just the drawing: a hitch anywhere here shows up as a long frame, and
		if it's on the CPU the GPU runs dry waiting for commands, which stretches whatever pass happened to
		be in flight. Timing only the render passes makes that look like the renderer's fault when it isn't
	*/
	pd.profiler.beginFrame(deltaT);
	pd.profiler.begin("Whole frame");

	//Single player: tick our embedded server before doing any client work this frame.
	//It shares the SimObject::world static with us, so reclaim it for our own PhysicsWorld once it's done.
	if (localServer)
	{
		GpuZone zone(pd.profiler, "Local server tick");
		localServer->run(deltaT, cmdArgs, settings);
		if (pd.physicsWorld)
			SimObject::world = pd.physicsWorld;
	}

	pd.profiler.begin("Networking");
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

		//Keeps the sky moving smoothly between the server's once a second WorldStateUpdate packets
		simulation.worldTimeSeconds += (deltaT / 1000.0) * simulation.timeScale;
	}

	pd.profiler.end();

	//movement keys and camera direction as it relates to players / controlled objects 
	pd.profiler.begin("Controllers and input");
	updateControllers(deltaT); 

	handleInput(deltaT,cmdArgs,settings); //mouse and keyboard input
	pd.profiler.end();

	// --- UI Updates and Requests ---

	//Send info to debug menu for display
	NetInfo netInfo;
	if (client)
		netInfo = { client->getPing(), client->getIncoming(), client->getOutgoing(), simulation.serverLastSlowestFrame, simulation.serverAverageFrame };
	pd.debugMenu->passDetails(simulation.camera->getPosition(),simulation.camera->getDirection(), netInfo);

	pd.debugMenu->addExtraLine("Time of day: " + std::to_string(pd.environment.dayFraction) + " (x" + std::to_string(simulation.timeScale) + ")");
	if (simulation.bricks)
		pd.debugMenu->addExtraLine("Bricks: " + std::to_string(simulation.bricks->size()));
	pd.debugMenu->addExtraLine("Environmental audio: " + pd.acousticProbe.getStats());
	pd.debugMenu->addExtraLine("Rain: " + pd.rain.getStats());
	pd.debugMenu->addExtraLine("Point lights: " + pd.pointLights->getStats());
	pd.debugMenu->addExtraLine("Shadows: " + pd.brickRenderer->getShadowStats());
	pd.debugMenu->addExtraLine("Particles: " + pd.particles->getStats());
	if (!pd.printVideos.getStatus().empty())
		pd.debugMenu->addExtraLine("Prints: " + pd.printVideos.getStatus());
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
		simulation.lights = new ObjHolder<Light>(LightTypeId);
		simulation.emitters = new ObjHolder<Emitter>(EmitterTypeId);
		simulation.vehicles = new ObjHolder<Vehicle>(VehicleTypeId);
		simulation.bricks = new BrickHolder(pd.physicsWorld, &pd.brickTypes);
		simulation.bricks->setRenderer(pd.brickRenderer);
		simulation.brickDebris = new BrickDebris(pd.physicsWorld, &pd.brickTypes);
		simulation.brickDebris->setLifetime(settings->getFloat("graphics/brickdebrisseconds"));

		ENetPacket* finishedLoading = makeLoadingFinished();
		client->send(finishedLoading, OtherReliable);

		cmdArgs.gameState = InGame;

		pd.serverBrowser->setConnectionNote("");
		pd.serverBrowser->close();

		//Chat is part of the HUD rather than a window that holds the mouse, so playing starts with the mouse captured
		pd.chatWindow->open();
		pd.context->setMouseLock(true);
	}

	//All signals from packets processed for this frame, reset flags
	pd.signals.reset();

	// --- End packet requests ---

	/*
		Water holds up every dynamic, like on the server, not just the ones this client simulates itself
		Otherwise the local bodies of floating objects fall between server updates and snap back up on each one,
		which shows in the debug physics view and knocks into players swimming around them
	*/
	if (pd.physicsWorld && simulation.dynamics && simulation.waterEnabled)
	{
		for (unsigned int a = 0; a < simulation.dynamics->size(); a++)
		{
			if (simulation.dynamics->get(a)->isInWorld())
				simulation.dynamics->get(a)->applyWaterForces(simulation.waterLevel, deltaT);
		}
	}

	//Vehicles' bodies move to where they're drawn before the step, so they push players around from there
	pd.profiler.begin("Physics");
	if (simulation.vehicles)
	{
		for (unsigned int a = 0; a < simulation.vehicles->size(); a++)
			simulation.vehicles->get(a)->updateSnapshot(deltaT);
	}

	if (pd.physicsWorld)
		pd.physicsWorld->step(deltaT);

	if (simulation.brickDebris)
		simulation.brickDebris->update(deltaT);

	predictLocalCollisions();
	pd.profiler.end();

	{
		GpuZone zone(pd.profiler, "Rendering");
		renderEverything(deltaT);
	}

	pd.profiler.begin("Audio");

	//The listener is the camera, which rendering just moved for this frame
	glm::vec3 listener = simulation.camera->getPosition();
	bool inWorld = pd.physicsWorld && cmdArgs.gameState == InGame;
	pd.audio->setUnderwater(inWorld && simulation.waterEnabled && listener.y < simulation.waterLevel);

	pd.acousticProbe.updateStats(deltaT);
	if (inWorld && pd.audio->wantsListenerSpace())
	{
		const btRigidBody* player = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
		float averageDistance, enclosure;
		if (pd.acousticProbe.measure(deltaT, *pd.physicsWorld, listener, player, averageDistance, enclosure))
			pd.audio->setListenerSpace(averageDistance, enclosure);
	}

	//Rain gets quieter and muffled the less open sky there is around the camera, which is only measured while it's raining
	float rainIntensity = inWorld ? pd.rain.getIntensity() : 0.0f;
	if (rainIntensity > 0.0f)
	{
		const btRigidBody* player = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
		float skyExposure;
		if (pd.acousticProbe.skyExposure(deltaT, *pd.physicsWorld, listener, player, skyExposure))
			pd.audio->setRainExposure(skyExposure);
	}
	pd.audio->setRain(rainIntensity);

	//The Doppler effect goes by the velocity of what the camera follows, so swinging a third person camera around doesn't bend every sound's pitch
	std::optional<glm::vec3> listenerVelocity;
	if (std::shared_ptr<Dynamic> followed = simulation.camera->target.lock())
	{
		//Riding a vehicle parks its rider's body out of the physics world, so the vehicle is what they're really moving with,
		//which is also what its own sounds move with, leaving them unbent for whoever rides along with them
		if (std::shared_ptr<Vehicle> ridden = followed->ridingVehicle.lock())
		{
			listenerVelocity = ridden->serverVelocity;
		}
		else
		{
			btVector3 velocity = followed->getVelocity();
			listenerVelocity = glm::vec3(velocity.x(), velocity.y(), velocity.z());
		}
	}

	//The key toggles it, so unlike a held key it isn't dropped by typing in chat, but leaving the game still ends it
	if (!client || cmdArgs.gameState != InGame)
		voiceToggled = false;
	pd.voice->update(voiceToggled, [this](unsigned char flags, uint16_t sequence, const unsigned char* data, unsigned int length)
	{
		if (client)
			client->send(makeVoiceFramePacket(flags, sequence, data, length), VoiceData);
	}, deltaT);

	std::string microphoneProblem;
	if (pd.voice->takeMicrophoneProblem(microphoneProblem))
		pd.gui->addCenterPrint(microphoneProblem, 4000, 1.0f, 0.45f, 0.45f);

	pd.audio->update(listener, simulation.camera->getDirection(), listenerVelocity, deltaT);
	pd.profiler.end();

	pd.profiler.end();
	pd.profiler.endFrame();
	pd.debugMenu->passProfilerResults(pd.profiler.getResults());
	pd.brickRenderer->resetShadowStats();

	if (cmdArgs.profileRendering && pd.profiler.getWindow() != loggedProfilerWindow)
	{
		loggedProfilerWindow = pd.profiler.getWindow();
		for (const std::string& line : pd.profiler.getReport())
			info(line);
		info("shadows: " + pd.brickRenderer->getShadowStats());
	}
}

LoopClient::LoopClient(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings)
{
	//This will also populate key-bind specific defaults, so we reexport after
	pd.input = std::make_shared<InputMap>(settings);
	settings->exportToFile("Config/settings.txt");

	info("Not dedicated, initalizing client.");

	simulation.idealBufferSize = settings->getInt("network/snapshotbuffer");

	//Created empty the first time, so loading it doesn't log a missing file error
	if (!std::filesystem::exists(ClientProgramData::stateFilePath))
		std::ofstream emptyStateFile(ClientProgramData::stateFilePath);
	pd.state = std::make_shared<SettingManager>(ClientProgramData::stateFilePath);

	//Older builds kept the last server and name in settings.txt
	if (!pd.state->getPreference("network/username") && settings->getPreference("network/username"))
		pd.state->addString("network/username", settings->getString("network/username"));
	if (!pd.state->getPreference("network/lastip") && settings->getPreference("network/lastip"))
		pd.state->addString("network/lastip", settings->getString("network/lastip"));
	if (!pd.state->getPreference("network/lastport") && settings->getPreference("network/port"))
		pd.state->addInt("network/lastport", settings->getInt("network/port"), true, "", 1, 65535);

	//Create our program window
	pd.context = std::make_shared<RenderContext>(settings, pd.state);
	pd.appliedStartResolution = glm::ivec2(settings->getInt("graphics/startresolutionx"), settings->getInt("graphics/startresolutiony"));
	if (!pd.context->isValid())
	{
		//RenderContext already logged exactly what went wrong (window/GL context creation failure). Bail out
		//here instead of continuing on to create ImGui/ShaderManager/etc against a nonexistent GL context,
		//which previously crashed on a null GL function pointer far away from the actual root cause.
		error("Could not create a valid render context, aborting client startup.");
		return;
	}

	//Created before the windows since the brick selector loads its icons through it
	pd.textures = std::make_shared<TextureManager>();

	//Carries on silently if there's no audio device
	pd.audio = std::make_shared<AudioSystem>();
	pd.audio->setVolumes(settings->getFloat("audio/mastervolume"), settings->getFloat("audio/musicvolume"));
	pd.audio->setEnvironmentOptions(settings->getInt("audio/reverbquality"), settings->getInt("audio/occlusionquality"));
	pd.acousticProbe.setQuality(settings->getInt("audio/reverbquality"), settings->getInt("audio/occlusionquality"));
	pd.audio->setVoiceVolume(settings->getFloat("audio/voicevolume"));

	pd.voice = std::make_shared<VoiceChat>(pd.audio);
	pd.voice->setMicrophone(settings->getString("audio/microphone"), settings->getFloat("audio/microphonevolume"));

	//A sound is muffled by anything between it and the camera, other than the player's own body and whatever the sound is on
	pd.audio->setOcclusionTest([this](const glm::vec3& listener, const glm::vec3& source, const btRigidBody* sourceBody)
	{
		if (!pd.physicsWorld)
			return 0.0f;

		const btRigidBody* player = simulation.controlledDynamics.empty() ? nullptr : simulation.controlledDynamics[0]->body;
		return pd.acousticProbe.occlusion(*pd.physicsWorld, listener, source, player, sourceBody);
	});

	pd.brickTypes.load("Assets/brick/types");
	pd.prints.load("Assets/brick/prints");

	pd.gui = std::make_shared<UserInterface>();
	pd.gui->updateSettings(settings);
	pd.settingsMenu = pd.gui->createWindow<SettingsMenu>(settings, pd.input);
	pd.settingsMenu->setStringChoices("audio/microphone", &VoiceChat::listMicrophones);
	pd.debugMenu = pd.gui->createWindow<DebugMenu>();
	pd.escapeMenu = pd.gui->createWindow<EscapeMenu>();
	pd.serverBrowser = pd.gui->createWindow<ServerBrowser>();
	pd.chatWindow = pd.gui->createWindow<ChatWindow>();
	pd.brickSelector = pd.gui->createWindow<BrickSelector>(&pd.brickTypes, pd.textures);
	pd.brickHotbar = pd.gui->createWindow<BrickHotbar>();
	pd.paintMenu = pd.gui->createWindow<PaintMenu>(pd.input);
	pd.itemHotbar = pd.gui->createWindow<ItemHotbar>();
	pd.itemIcons3d = settings->getBool("graphics/itemicons3d");
	if (pd.itemIcons3d)
		pd.itemIcons = std::make_shared<ItemIconRenderer>(pd.textures);
	pd.appearanceEditor = pd.gui->createWindow<AppearanceEditor>(settings, pd.textures, &pd.faceNames, &pd.shirtNames, &pd.hatNames);
	pd.wrenchDialog = pd.gui->createWindow<WrenchDialog>();
	pd.printMenu = pd.gui->createWindow<PrintMenu>(pd.textures, &pd.prints);
	pd.vehicleLoader = pd.gui->createWindow<VehicleLoader>();
	pd.brickSaveMenu = pd.gui->createWindow<BrickSaveMenu>(&pd.brickTypes, &pd.prints);
	pd.playerList = pd.gui->createWindow<PlayerListWindow>();
	//Builds from before the state file kept the hot bar in settings.txt
	std::shared_ptr<SettingManager> hotbarSource = pd.state;
	if (!pd.state->getPreference("hotbar/slot1/filled") && settings->getPreference("hotbar/slot1/filled"))
		hotbarSource = settings;
	pd.paintMenu->load(hotbarSource);
	pd.brickHotbar->load(hotbarSource, [this](const std::string& brickName) { return pd.brickSelector->findIcon(brickName); });
	pd.paintMenu->save(pd.state);
	pd.brickHotbar->save(pd.state);
	pd.state->exportToFile(ClientProgramData::stateFilePath);

	//Anything moved to the state file comes out of settings.txt, otherwise it would linger in the settings menu doing nothing
	bool removedFromSettings = settings->remove("hotbar");
	for (const char* moved : { "network/username", "network/lastip", "network/port" })
		removedFromSettings = settings->remove(moved) || removedFromSettings;
	//Replaced by audio/reverbquality and audio/occlusionquality
	for (const char* replaced : { "audio/environmentalreverb", "audio/occlusion", "audio/raycastquality" })
		removedFromSettings = settings->remove(replaced) || removedFromSettings;
	if (removedFromSettings)
		settings->exportToFile("Config/settings.txt");

	std::string lastIp = pd.state->getPreference("network/lastip") ? pd.state->getString("network/lastip") : "localhost";
	int lastPort = pd.state->getPreference("network/lastport") ? pd.state->getInt("network/lastport") : DEFAULT_PORT;
	std::string lastName = pd.state->getPreference("network/username") ? pd.state->getString("network/username") : "Guest";
	pd.serverBrowser->passDefaultSettings(lastIp, lastPort, lastName);
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

	//Faces and shirts players pick in the appearance editor, each on its own decal array layer, servers send them by file name
	auto listImages = [](const char* folder)
	{
		std::vector<std::filesystem::path> paths;
		if (std::filesystem::is_directory(folder))
		{
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder))
			{
				std::string extension = lowercase(entry.path().extension().string());
				if (entry.is_regular_file() && (extension == ".png" || extension == ".jpg"))
					paths.push_back(entry.path());
			}
		}
		std::sort(paths.begin(), paths.end());
		return paths;
	};
	std::vector<std::filesystem::path> facePaths = listImages("Assets/faces");
	std::vector<std::filesystem::path> shirtPaths = listImages("Assets/shirts");

	//Decal IDs get 8 bits of a mesh's instance flags, faces come first
	if (facePaths.size() > 256)
		facePaths.resize(256);
	if (facePaths.size() + shirtPaths.size() > 256)
		shirtPaths.resize(256 - facePaths.size());

	//Brick prints share the decal array with them, see PrintTypes
	size_t printCount = std::min<size_t>(pd.prints.size(), 256 - facePaths.size() - shirtPaths.size());
	if (printCount < pd.prints.size())
		error("Only " + std::to_string(printCount) + " of " + std::to_string(pd.prints.size()) + " prints fit in the decal array, the rest will look plain");

	pd.textures->allocateForDecals(256, std::max<unsigned int>(1, (unsigned int)(facePaths.size() + shirtPaths.size() + printCount)));
	for (const std::filesystem::path& facePath : facePaths)
	{
		if (pd.textures->addDecal(facePath.generic_string(), (int)pd.faceNames.size()))
			pd.faceNames.push_back(facePath.filename().string());
	}
	for (const std::filesystem::path& shirtPath : shirtPaths)
	{
		if (pd.textures->addDecal(shirtPath.generic_string(), (int)(pd.faceNames.size() + pd.shirtNames.size())))
			pd.shirtNames.push_back(shirtPath.filename().string());
	}
	bool skippedVideos = false;
	for (size_t print = 0; print < printCount; print++)
	{
		const PrintType* type = pd.prints.get((int)print);
		int layer = (int)(facePaths.size() + shirtPaths.size() + print);

		//A .webm plays into its layer instead of being loaded once, see Graphics/PrintVideos.h
		bool loaded = type->video ? pd.printVideos.add((int)print, *type, layer, pd.textures) : pd.textures->addDecal(type->filePath, layer);
		skippedVideos |= type->video && !loaded;

		if (loaded)
			pd.prints.setDecalLayer((int)print, layer);
	}

	if (skippedVideos && !VideoPlayer::isSupported())
		error("This build has no libvpx, so .webm prints are skipped, see the video prints line from CMake when it was built");

	pd.printVideos.setMaxPlaying(settings->getInt("graphics/maxvideoprints"));
	pd.textures->finalizeDecals();

	pd.grassMaterial = new Material("Assets/ground/grass.txt", pd.textures);

	if (!pd.grassMaterial->isValid())
	{
		pd.gui->popupErrorMessage = "Error loading grass material, see error log.";
	}

	pd.grassVao = createQuadVAO();

	pd.pointLights = new PointLights();
	pd.particles = new ParticleSystem(pd.textures);
	pd.particles->setMaxParticles(settings->getInt("graphics/maxparticles"));
	createShadowTarget(settings);
	pd.rain.setQuality(settings->getInt("graphics/rainquality"), pd.textures);
	pd.lightSpaceMatriciesUniformModel = pd.shaders->modelShader->getUniformLocation("lightSpaceMatricies");
	pd.lightSpaceMatriciesUniformBrick = pd.shaders->brickShader->getUniformLocation("lightSpaceMatricies");
	pd.shadowCascadeMatrixUniformModel = pd.shaders->modelShadowCascadeShader->getUniformLocation("lightSpaceMatrix");
	pd.shadowCascadeMatrixUniformBrick = pd.shaders->brickShadowCascadeShader->getUniformLocation("lightSpaceMatrix");
	pd.shadowTintMatrixUniform = pd.shaders->brickShadowTintShader->getUniformLocation("lightSpaceMatrix");
	pd.shadowCascadeMinOpacityUniform = pd.shaders->brickShadowCascadeShader->getUniformLocation("minOpacity");
	pd.shadowTintMinOpacityUniform = pd.shaders->brickShadowTintShader->getUniformLocation("minOpacity");
	pd.shadowCascadeSkipContainingUniform = pd.shaders->brickShadowCascadeShader->getUniformLocation("skipContaining");
	pd.shadowCascadeSkipPointUniform = pd.shaders->brickShadowCascadeShader->getUniformLocation("skipPoint");
	pd.godRaySunScreenUniform = pd.shaders->godRayShader->getUniformLocation("sunScreenPosition");
	pd.godRaySampleCountUniform = pd.shaders->godRayShader->getUniformLocation("sampleCount");
	pd.godRayStrengthUniform = pd.shaders->godRayShader->getUniformLocation("strength");
	pd.shadowTintSkipContainingUniform = pd.shaders->brickShadowTintShader->getUniformLocation("skipContaining");
	pd.shadowTintSkipPointUniform = pd.shaders->brickShadowTintShader->getUniformLocation("skipPoint");

	pd.brickRenderer = new InstancedBrickRenderer(pd.shaders, pd.textures, &pd.brickTypes, &pd.prints);

	//Every vehicle's wheels are drawn with it
	pd.tireModel = new Model("Assets/tire/tire.txt", pd.textures, glm::vec3(1.0f));
	if (!pd.tireModel->isValid() || pd.tireModel->getNumMeshes() < 1)
	{
		error("Couldn't load Assets/tire/tire.txt, vehicle wheels won't be drawn");
		delete pd.tireModel;
		pd.tireModel = nullptr;
	}

	//Hats players pick in the appearance editor, worn on their player's Head, servers send them by file name like faces
	if (std::filesystem::is_directory(PlayerAppearance::partsFolder))
	{
		std::vector<std::filesystem::path> hatPaths;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(PlayerAppearance::partsFolder))
		{
			if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".txt")
				hatPaths.push_back(entry.path());
		}
		std::sort(hatPaths.begin(), hatPaths.end());

		for (const std::filesystem::path& hatPath : hatPaths)
		{
			Model* hat = new Model(hatPath.generic_string(), pd.textures, glm::vec3(1.0f));
			if (!hat->isValid() || hat->getNumMeshes() < 1 || !hat->isAttachment())
			{
				error("Couldn't load " + hatPath.generic_string() + " as a hat" + (hat->isValid() && !hat->isAttachment() ? ", it needs an attach line saying which mesh it's worn on" : ""));
				delete hat;
				continue;
			}

			pd.hatNames.push_back(hatPath.filename().string());
			pd.hatModels.push_back(hat);
		}
	}

	pd.skybox = new Skybox(pd.shaders);
	pd.imageBasedLighting = settings->getBool("graphics/imagebasedlighting");
	pd.depthPrePass = settings->getBool("graphics/depthprepass");
	//The camera picked the setting up in its own updateSettings above, before the brick renderer existed
	applyDrawDistance();

	glGenVertexArrays(1, &pd.skyVao);

	//Integer grid coordinates go through the same math for every cell that shares a vertex, so no cracks between cells
	auto gridPoint = [](int x, int z) { return glm::vec2(x, z) / (float)waterGridCells * 2.0f - 1.0f; };
	std::vector<glm::vec2> waterGrid;
	waterGrid.reserve(waterGridCells * waterGridCells * 6);
	for (int x = 0; x < waterGridCells; x++)
	{
		for (int z = 0; z < waterGridCells; z++)
		{
			waterGrid.push_back(gridPoint(x, z));
			waterGrid.push_back(gridPoint(x, z + 1));
			waterGrid.push_back(gridPoint(x + 1, z));
			waterGrid.push_back(gridPoint(x + 1, z));
			waterGrid.push_back(gridPoint(x, z + 1));
			waterGrid.push_back(gridPoint(x + 1, z + 1));
		}
	}
	pd.waterVertexCount = (GLsizei)waterGrid.size();

	glGenVertexArrays(1, &pd.waterVao);
	glBindVertexArray(pd.waterVao);
	glGenBuffers(1, &pd.waterVbo);
	glBindBuffer(GL_ARRAY_BUFFER, pd.waterVbo);
	glBufferData(GL_ARRAY_BUFFER, waterGrid.size() * sizeof(glm::vec2), waterGrid.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glBindVertexArray(0);

	createWaterTargets(settings);
	createGodRayTarget(settings);

	info("Start up complete");

	printAllGraphicsErrors("End of initalization");

	valid = true;

	//Automated testing convenience: skip the server browser and get straight into a game, same as clicking "Start Server"
	if (cmdArgs.autoSinglePlayer)
		hostSinglePlayer(cmdArgs, settings);
}

LoopClient::~LoopClient()
{
	//testChunk.deleteAllBricks();

	pd.shadows.reset();
	pd.shadowTint.reset();
	pd.waterReflection.reset();
	pd.waterRefraction.reset();
	pd.underwaterScene.reset();

	delete pd.grassMaterial;
	glDeleteVertexArrays(1, &pd.grassVao);
	glDeleteVertexArrays(1, &pd.skyVao);
	glDeleteVertexArrays(1, &pd.waterVao);
	glDeleteBuffers(1, &pd.waterVbo);

	//Vehicles' tire instances are gone with leaveServer, as are the hats dynamics wore
	delete pd.tireModel;
	pd.tireModel = nullptr;

	for (Model* hat : pd.hatModels)
		delete hat;
	pd.hatModels.clear();
	pd.hatNames.clear();

	delete pd.brickRenderer;
	pd.brickRenderer = nullptr;

	delete pd.pointLights;
	pd.pointLights = nullptr;

	delete pd.particles;
	pd.particles = nullptr;

	delete pd.skybox;
	pd.skybox = nullptr;

	//Its model and picking target need the OpenGL context, which goes away with pd.context
	if (pd.appearanceEditor)
		pd.appearanceEditor->releaseGraphics();

	//Not needed this is a destructor lol
	//Also this should only be called when the programs shutting down anyway
	pd.context.reset();
	pd.gui.reset(); //Will handle indivdual windows
	pd.shaders.reset();
	pd.textures.reset();
	pd.voice.reset();
	pd.audio.reset();

	//This one is actually useful because the server will learn we disconnected faster if we do it properly
	if(client)
		delete client;

	if (localServer)
		delete localServer;
}
