#include "LoopServer.h"

#include "../LuaFunctions/Dynamic.h"
#include "../LuaFunctions/Static.h"
#include "../LuaFunctions/LightLua.h"
#include "../LuaFunctions/SoundLua.h"
#include "../LuaFunctions/EmitterLua.h"
#include "../LuaFunctions/BrickLua.h"
#include "../LuaFunctions/SkyLua.h"
#include "../LuaFunctions/ItemLua.h"
#include "../LuaFunctions/VehicleLua.h"

#include <random>

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

	pd.worldTimeSeconds += (deltaT / 1000.0) * pd.timeScale;
	if (pd.worldStateChanged || SDL_GetTicks() - lastWorldStateBroadcast > 1000)
		broadcastWorldState();

	server->run(&pd,pd.luaState,pd.eventManager); //   <---- networking
	endQuietTalkers();
	startUpdateBudgets();
	pd.dynamics->sendRecent();
	updateItems();
	pd.statics->sendRecent();
	pd.lights->sendRecent();
	pd.emitters->sendRecent();
	pd.vehicles->sendRecent();
	sendNewVehicleBricks();
	pd.bricks->sendRecent();
	updateVehicles(deltaT);
	applyWaterForces(deltaT);
	pd.physicsWorld->step(deltaT);

	updateProjectiles();
	updateVehiclesAfterStep();
	playWaterSounds();
	updateEmitters();

	for (unsigned int a = 0; a < Logger::getStorage()->size(); a++)
		server->updateAdminConsoles(Logger::getStorage()->at(a));
	Logger::getStorage()->clear();

	for (unsigned int a = 0; a < pd.clients.size(); a++)
	{
		for (unsigned int b = 0; b < pd.clients[a]->controllers.size(); b++)
		{
			pd.clients[a]->controllers[b].controlWithLastInput(pd.physicsWorld, deltaT, pd.waterEnabled ? pd.waterLevel : PlayerController::noWater);
		}
	}

	updatePlayerAbilities();

	//Drive any dynamics currently snapped to a client's cursor (see dynamic:snapToCursor)
	for (unsigned int a = 0; a < pd.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = pd.dynamics->get(a);
		if (!dynamic->isSnappedToCursor())
			continue;

		std::shared_ptr<JoinedClient> owner = dynamic->snappedToClient.lock();
		std::shared_ptr<ClientData> ownerData = owner ? pd.getClient(owner) : nullptr;

		//Owning client disconnected (or otherwise lost its controller) since this was snapped - drop it back into normal physics
		if (!ownerData || ownerData->controllers.size() == 0)
		{
			dynamic->unsnapFromCursor();
			continue;
		}

		dynamic->updateCursorSnapPosition(ownerData->controllers[0].lastCameraPosition, ownerData->controllers[0].lastCameraDirection);
	}

	scheduler->run(pd.luaState);
}

/*
	A client's update budget is spent across every sendRecent call in the tick, so it's handed out here once rather
	than per object type. Where they're watching from is their player, or whatever they're riding, since a player on
	a vehicle is out of the physics world and their body sits wherever it last was

	Whichever type sends first has first claim on it, dynamics as the run loop stands, so a budget small enough to
	actually bind would starve vehicles and lights before it starved players. It's meant as an overload backstop
	rather than a target - the distance bands are what shape normal traffic - so keep hosting/updatebytespertick
	well above what a busy scene needs
*/
void LoopServer::startUpdateBudgets()
{
	for (unsigned int a = 0; a < pd.clients.size(); a++)
	{
		const std::shared_ptr<ClientData>& data = pd.clients[a];
		if (!data || !data->client)
			continue;

		JoinedClient& client = *data->client;

		/*
			A bad connection gets a smaller share rather than nothing at all. Cutting them off entirely, as this used
			to, leaves them quietly desynced with no way back short of their ping recovering on its own
		*/
		float share = 1.0f;
		if (client.getPing() > 1500.0f)
			share = NetRelevanceSettings::veryHighPingFactor;
		else if (client.getPing() > 400.0f)
			share = NetRelevanceSettings::highPingFactor;

		client.updateByteBudget = (int)(NetRelevanceSettings::bytesPerTick * share);

		client.hasRelevancePosition = false;

		std::shared_ptr<Vehicle> vehicle = data->vehicle.lock();
		if (vehicle && vehicle->body)
		{
			client.relevancePosition = b2g3(vehicle->body->getWorldTransform().getOrigin());
			client.hasRelevancePosition = true;
			continue;
		}

		for (unsigned int b = 0; b < data->controlledObjects.size(); b++)
		{
			if (!data->controlledObjects[b])
				continue;

			client.relevancePosition = b2g3(data->controlledObjects[b]->getPosition());
			client.hasRelevancePosition = true;
			break;
		}
	}
}

void LoopServer::endQuietTalkers()
{
	unsigned int now = SDL_GetTicks();

	for (unsigned int a = 0; a < pd.clients.size(); a++)
	{
		std::shared_ptr<ClientData> talker = pd.clients[a];
		if (!talker->talking || now - talker->lastVoiceMS < ServerProgramData::voiceTimeoutMS)
			continue;

		talker->talking = false;

		pushClientLua(pd.luaState, talker->client);
		pd.eventManager->callEvent(pd.luaState, "ClientStopTalking", 1);
		lua_settop(pd.luaState, 0);
	}
}

void LoopServer::updateItems()
{
	for (std::shared_ptr<ClientData>& client : pd.clients)
	{
		std::shared_ptr<Dynamic> holder = client->controllers.empty() ? nullptr : client->controllers[0].target.lock();

		for (int slot = 0; slot < inventorySize; slot++)
		{
			std::shared_ptr<Item> item = client->inventory[slot].lock();
			if (!item)
				continue;

			//Goes along with its holder, so it comes back out of their inventory next to them and getPosition says where they are
			if (holder)
			{
				btTransform transform = item->body->getWorldTransform();
				transform.setOrigin(holder->getPosition());
				item->body->setWorldTransform(transform);
			}

			//Everyone else draws it in the hand of whoever holds it, while it's picked
			netIDType holderID = holder ? holder->getID() : NO_ID;
			if (holderID != item->sentHolderID || item->isEquipped() != item->sentEquipped)
				pd.markItemChanged(item);
		}
	}

	for (std::weak_ptr<Item>& changed : pd.changedItems)
	{
		std::shared_ptr<Item> item = changed.lock();
		if (!item)
			continue;

		item->stateChanged = false;
		server->broadcast(item->makeStatePacket(), OtherReliable);
	}
	pd.changedItems.clear();
}

void LoopServer::sendNewVehicleBricks()
{
	for (std::weak_ptr<Vehicle>& waiting : pd.vehiclesAwaitingBricks)
	{
		std::shared_ptr<Vehicle> vehicle = waiting.lock();
		if (!vehicle)
			continue;

		//Clients hold onto these until the vehicle's creation packet arrives
		for (ENetPacket* packet : vehicle->makeBrickPackets())
			server->broadcast(packet, OtherReliable);
	}
	pd.vehiclesAwaitingBricks.clear();
}

void LoopServer::updateVehicles(float deltaT)
{
	//A wheel this far above the water already floats a little, and floats hardest this far below it, like the old game
	static constexpr float floatAbove = 2.0f;
	static constexpr float floatBelow = 7.0f;
	//How much of the vehicle's weight each wheel holds up all the way under, spread between its wheels
	static constexpr float wheelBuoyancy = 1.6f;

	unsigned int now = SDL_GetTicks();

	for (unsigned int a = 0; a < pd.vehicles->size(); a++)
	{
		std::shared_ptr<Vehicle> vehicle = pd.vehicles->get(a);
		if (!vehicle->body)
			continue;

		std::shared_ptr<ClientData> driver = vehicle->driver.lock();

		//A driver whose client left, or whose player was destroyed or swapped out, gets out
		if (vehicle->driverID != NO_ID)
		{
			std::shared_ptr<Dynamic> player = pd.dynamics->find(vehicle->driverID);
			bool stillDriving = driver && player && !driver->controllers.empty() && driver->controllers[0].target.lock() == player;
			if (!stillDriving)
			{
				if (driver)
					exitVehicle(*driver, true);
				else
				{
					vehicle->driverID = NO_ID;
					server->broadcast(vehicle->makeDriverPacket(), OtherReliable);
				}
				driver = nullptr;
			}
		}

		//Same for passengers
		for (int s = 0; s < (int)vehicle->passengerSeats.size(); s++)
		{
			PassengerSeat& seat = vehicle->passengerSeats[s];
			if (seat.riderID == NO_ID)
				continue;

			std::shared_ptr<ClientData> rider = seat.rider.lock();
			std::shared_ptr<Dynamic> player = pd.dynamics->find(seat.riderID);
			bool onThisSeat = rider && rider->vehicle.lock() == vehicle && rider->vehicleSeat == s;
			if (onThisSeat && player && !rider->controllers.empty() && rider->controllers[0].target.lock() == player)
				continue;

			if (onThisSeat)
				exitVehicle(*rider, true);
			else
			{
				seat.riderID = NO_ID;
				seat.rider.reset();
				server->broadcast(vehicle->makeDriverPacket(), OtherReliable);
			}
		}

		if (driver)
		{
			const PlayerController& keys = driver->controllers[0];
			bool speeding = vehicle->drive(keys.lastForward, keys.lastBackward, keys.lastLeft, keys.lastRight, keys.lastJumpHeld);
			if (speeding && driver->client && now - vehicle->lastSpeedWarningMS > 5000)
			{
				vehicle->lastSpeedWarningMS = now;
				driver->client->sendCenterPrint("You have reached this vehicle's max speed!", 3000, 1.0f, 1.0f, 1.0f);
			}
		}
		else
			vehicle->park();

		if (!pd.waterEnabled || vehicle->wheels.empty() || vehicle->body->getInvMass() <= 0)
			continue;

		btScalar weight = vehicle->body->getGravity().length() / vehicle->body->getInvMass();
		const btVector3& origin = vehicle->body->getWorldTransform().getOrigin();
		bool floating = false;

		for (int w = 0; w < (int)vehicle->wheels.size(); w++)
		{
			btVector3 wheel = vehicle->getWheelTransform(w).getOrigin();
			float depth = pd.waterLevel + floatAbove - wheel.y();
			if (depth <= 0)
				continue;

			float amount = std::clamp(depth / (floatAbove + floatBelow), 0.0f, 1.0f);
			vehicle->body->applyForce(btVector3(0, weight * wheelBuoyancy * amount / vehicle->wheels.size(), 0), wheel - origin);
			floating = true;
		}

		if (floating)
			vehicle->body->activate();
	}
}

void LoopServer::updateVehiclesAfterStep()
{
	static constexpr unsigned int splashCooldownMS = 1000;

	unsigned int now = SDL_GetTicks();

	//Backwards, since removing one moves the ones after it down
	for (int a = (int)pd.vehicles->size() - 1; a >= 0; a--)
	{
		std::shared_ptr<Vehicle> vehicle = pd.vehicles->get(a);
		if (!vehicle->body)
			continue;

		//Like the old game, a vehicle the physics sent flying off is removed before it takes anything with it
		const btVector3& position = vehicle->body->getWorldTransform().getOrigin();
		std::string problem = "";
		if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || !std::isfinite(position.z()))
			problem = "its position stopped being a number";
		else if (vehicle->body->getLinearVelocity().length() > 1000)
			problem = "it was going faster than 1000 studs a second";
		else if (vehicle->body->getAngularVelocity().length() > 300)
			problem = "it was spinning too fast";
		else if (position.length() > 10000)
			problem = "it went more than 10000 studs from the middle of the world";

		if (!problem.empty())
		{
			error("Removed vehicle " + std::to_string(vehicle->getID()) + " because " + problem);
			destroyVehicle(vehicle);
			continue;
		}

		vehicle->updateWheelStates();

		if (vehicle->driverID != NO_ID)
		{
			if (std::shared_ptr<Dynamic> player = pd.dynamics->find(vehicle->driverID))
			{
				if (!player->isInWorld())
					player->body->setWorldTransform(vehicle->getSeatTransform(false));
			}
		}

		//Passengers turn to face where they look
		for (int s = 0; s < (int)vehicle->passengerSeats.size(); s++)
		{
			netIDType riderID = vehicle->passengerSeats[s].riderID;
			std::shared_ptr<Dynamic> player = riderID != NO_ID ? pd.dynamics->find(riderID) : nullptr;
			if (player && !player->isInWorld())
				player->body->setWorldTransform(vehicle->getPassengerTransform(s, *player, player->lookDirection, false));
		}

		bool underwater = pd.waterEnabled && position.y() < pd.waterLevel;
		vehicle->body->setDamping(underwater ? 0.3f : 0.0f, underwater ? 0.2f : vehicle->steering.angularDamping);

		for (int w = 0; w < (int)vehicle->wheels.size(); w++)
		{
			if (!pd.waterEnabled)
			{
				vehicle->wheelInWater[w] = false;
				continue;
			}

			btVector3 wheel = vehicle->getWheelTransform(w).getOrigin();

			//The same gap between going in and coming out as the old game
			if (vehicle->wheelInWater[w])
			{
				if (wheel.y() > pd.waterLevel + 2.0f)
					vehicle->wheelInWater[w] = false;
			}
			else if (wheel.y() < pd.waterLevel - 1.0f)
			{
				vehicle->wheelInWater[w] = true;
				if (now - vehicle->lastSplashMS[w] > splashCooldownMS)
				{
					vehicle->lastSplashMS[w] = now;
					glm::vec3 surface(wheel.x(), pd.waterLevel, wheel.z());
					playSoundAt("Splash", surface, 1.0f, 1.0f);
					spawnEmitterAt("playerBubbleEmitter", surface);
				}
			}
		}
	}
}

void LoopServer::applyWaterForces(float deltaT)
{
	if (!pd.waterEnabled)
		return;

	/*
		Includes the dynamics clients simulate themselves: their physics packets only come every 100 ms or so, and without
		water in between the server would drop them and snap them back up, which everyone else would see
	*/
	for (unsigned int a = 0; a < pd.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = pd.dynamics->get(a);
		if (dynamic->isSnappedToCursor() || !dynamic->isInWorld())
			continue;

		dynamic->applyWaterForces(pd.waterLevel, deltaT);
	}
}

void LoopServer::playWaterSounds()
{
	//Vertical speeds, world units per second, below which going in or out of the water is silent
	static constexpr float splashSpeed = 6.0f;
	static constexpr float exitSpeed = 6.0f;
	static constexpr unsigned int soundCooldownMS = 700;

	static std::mt19937 random(std::random_device{}());
	std::uniform_real_distribution<float> pitchVariation(0.9f, 1.1f);

	for (unsigned int a = 0; a < pd.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = pd.dynamics->get(a);

		//Carried items don't splash, and do if they're thrown back in
		if (!pd.waterEnabled || !dynamic->isInWorld())
		{
			dynamic->inWater = false;
			continue;
		}

		btVector3 aabbMin, aabbMax;
		dynamic->body->getAabb(aabbMin, aabbMax);

		//A little gap between going in and coming out, so something floating at the surface doesn't keep doing both
		bool wasInWater = dynamic->inWater;
		if (!wasInWater && aabbMin.y() < pd.waterLevel)
			dynamic->inWater = true;
		else if (wasInWater && aabbMin.y() > pd.waterLevel + 0.5f)
			dynamic->inWater = false;

		if (dynamic->inWater == wasInWater || SDL_GetTicks() - dynamic->lastWaterSoundMS < soundCooldownMS)
			continue;

		float verticalSpeed = dynamic->getVelocity().y();
		btVector3 center = (aabbMin + aabbMax) * 0.5f;
		glm::vec3 surface(center.x(), pd.waterLevel, center.z());

		//Bigger things sound deeper, and no two splashes sound quite the same
		float size = (aabbMax - aabbMin).length();
		float pitch = std::clamp(1.3f - size * 0.06f, 0.6f, 1.3f) * pitchVariation(random);

		if (dynamic->inWater && -verticalSpeed > splashSpeed)
		{
			playSoundAt("Splash", surface, pitch, std::clamp(0.3f + (-verticalSpeed - splashSpeed) / 40.0f, 0.3f, 1.0f));
			//The old game's splash effect, if Lua defined it
			spawnEmitterAt("playerBubbleEmitter", surface);
			dynamic->lastWaterSoundMS = SDL_GetTicks();
		}
		else if (!dynamic->inWater && verticalSpeed > exitSpeed)
		{
			playSoundAt("ExitWater", surface, pitch, std::clamp(0.2f + (verticalSpeed - exitSpeed) / 60.0f, 0.2f, 0.7f));
			dynamic->lastWaterSoundMS = SDL_GetTicks();
		}
	}
}

//How close a projectile has to have come to something to hit it, a little past touching since the step may have already pushed it back out
static constexpr btScalar projectileHitDistance = 0.05f;

void LoopServer::updateProjectiles()
{
	//Copies, since ProjectileHit listeners can make and remove dynamics
	std::vector<std::shared_ptr<Dynamic>> projectiles;
	for (unsigned int a = 0; a < pd.dynamics->size(); a++)
	{
		std::shared_ptr<Dynamic> dynamic = pd.dynamics->get(a);
		if (dynamic->isProjectile)
			projectiles.push_back(dynamic);
	}

	for (std::shared_ptr<Dynamic>& projectile : projectiles)
	{
		//A listener for an earlier hit removed it
		if (pd.dynamics->find(projectile->getID()) != projectile || !projectile->isInWorld())
			continue;

		//Its shooter is gone, and a new body could be made where theirs was
		if (projectile->ignoredShooterBody && projectile->projectileShooter.expired())
		{
			projectile->body->setIgnoreCollisionCheck(projectile->ignoredShooterBody, false);
			projectile->ignoredShooterBody = nullptr;
		}

		btVector3 point;
		btRigidBody* hit = pd.physicsWorld->getFirstContact(projectile->body, projectileHitDistance, point);
		if (!hit)
		{
			projectile->faceVelocity();
			continue;
		}

		if (pd.eventManager)
		{
			lua_State* L = pd.luaState;
			lua_settop(L, 0);
			pd.dynamics->pushLua(L, projectile);
			//nil for the ground
			pushRaycastResult(L, hit);
			lua_pushnumber(L, point.x());
			lua_pushnumber(L, point.y());
			lua_pushnumber(L, point.z());
			lua_pushstring(L, projectile->projectileTag.c_str());
			pd.eventManager->callEvent(L, "ProjectileHit", 6);
			lua_settop(L, 0);
		}

		if (pd.dynamics->find(projectile->getID()) == projectile)
			pd.dynamics->destroy(projectile);
	}
}

void LoopServer::updateEmitters()
{
	unsigned int now = SDL_GetTicks();

	//Backwards, since destroying one moves the ones after it down
	for (int a = (int)pd.emitters->size() - 1; a >= 0; a--)
	{
		std::shared_ptr<Emitter> emitter = pd.emitters->get(a);

		uint16_t typeID = emitter->getTypeID();
		//Ones on bricks and vehicles last as long as they do, their particles still live out their own lifetimes
		bool lasting = emitter->getAttachKind() == EmitterAttachBrick || emitter->getAttachKind() == EmitterAttachVehicle;
		bool expired = !lasting && typeID < pd.emitterTypes.size() && pd.emitterTypes[typeID].lifetimeMS > 0 && now - emitter->getCreationTime() > pd.emitterTypes[typeID].lifetimeMS;
		bool dynamicGone = emitter->getAttachKind() == EmitterAttachDynamic && emitter->dynamic.expired();
		bool vehicleGone = emitter->getAttachKind() == EmitterAttachVehicle && emitter->vehicle.expired();
		bool brickGone = emitter->brickID != NO_ID && !pd.bricks->find(emitter->brickID);

		if (expired || dynamicGone || vehicleGone || brickGone)
			pd.emitters->destroy(emitter);
	}
}

void LoopServer::updatePlayerAbilities()
{
	//How far where a client looks has to turn before their flashlight turns with it, so holding still doesn't keep sending updates
	static const float flashlightTurnCosine = std::cos(glm::radians(1.0f));
	static const char* feet[2] = { "Left_Foot", "Right_Foot" };

	for (std::shared_ptr<ClientData>& client : pd.clients)
	{
		for (PlayerController& controller : client->controllers)
		{
			std::shared_ptr<Dynamic> target = controller.target.lock();
			//Not while driving, when right mouse is how they get out
			bool jetting = target && target->isInWorld() && controller.lastJet && controller.jetsAllowed;
			bool flaming = !controller.jetEmitters[0].expired() || !controller.jetEmitters[1].expired();

			if (jetting && !flaming)
			{
				//The old game's flames under each foot, if Lua defined them
				for (int a = 0; a < 2; a++)
				{
					int mesh = target->getType()->getModel()->getMeshIdx(feet[a]);
					//A model without feet gets one from its middle
					if (mesh == -1 && a == 1)
						break;

					std::shared_ptr<Emitter> flame = spawnEmitterAt("playerJetEmitter", b2g3(target->getPosition()));
					if (!flame)
						break;

					flame->attachToDynamic(target, mesh);
					controller.jetEmitters[a] = flame;
				}
			}
			else if (!jetting && flaming)
			{
				for (std::weak_ptr<Emitter>& jet : controller.jetEmitters)
				{
					if (std::shared_ptr<Emitter> flame = jet.lock())
						pd.emitters->destroy(flame);
					jet.reset();
				}
			}
		}

		std::shared_ptr<Light> light = client->flashlight.lock();
		if (!light)
			continue;

		std::shared_ptr<Dynamic> holder = client->controllers.empty() ? nullptr : client->controllers[0].target.lock();
		if (!holder)
		{
			client->setFlashlight(&pd, false, light->getColor());
			continue;
		}

		glm::vec3 look = client->controllers[0].lastCameraDirection;
		if (glm::length(look) > 0.0001f && glm::dot(glm::normalize(look), light->getDirection()) < flashlightTurnCosine)
			light->setDirection(look);
	}
}

void LoopServer::broadcastWorldState()
{
	lastWorldStateBroadcast = SDL_GetTicks();
	pd.worldStateChanged = false;

	ENetPacket* packet = enet_packet_create(NULL, 1 + sizeof(double) + sizeof(float) * 2 + 1 + sizeof(float) * DayCycle::networkFloats + sizeof(float), getFlagsFromChannel(OtherReliable));
	enet_uint8* data = packet->data;

	data[0] = (unsigned char)WorldStateUpdate;
	data++;

	memcpy(data, &pd.worldTimeSeconds, sizeof(double));
	data += sizeof(double);

	memcpy(data, &pd.timeScale, sizeof(float));
	data += sizeof(float);

	memcpy(data, &pd.waterLevel, sizeof(float));
	data += sizeof(float);

	data[0] = pd.waterEnabled ? 1 : 0;
	data++;

	auto writeColor = [&data](const glm::vec3& color)
	{
		memcpy(data, &color[0], sizeof(float) * 3);
		data += sizeof(float) * 3;
	};
	for (const SkyKeyframe& phase : pd.dayCycle.phases)
	{
		writeColor(phase.skyColor);
		writeColor(phase.fogColor);
		writeColor(phase.lightColor);
		writeColor(phase.ambientColor);
	}

	memcpy(data, &pd.dayCycle.fogStart, sizeof(float));
	data += sizeof(float);

	memcpy(data, &pd.dayCycle.fogEnd, sizeof(float));
	data += sizeof(float);

	memcpy(data, &pd.dayCycle.fogHeight, sizeof(float));
	data += sizeof(float);

	memcpy(data, &pd.rainIntensity, sizeof(float));

	server->broadcast(packet, OtherReliable);
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

	//Each band has to reach at least as far as the one inside it, whatever the settings file says
	NetRelevanceSettings::nearDistance = settings->getFloat("hosting/updatenear");
	NetRelevanceSettings::midDistance = std::max(settings->getFloat("hosting/updatemid"), NetRelevanceSettings::nearDistance);
	NetRelevanceSettings::farDistance = std::max(settings->getFloat("hosting/updatefar"), NetRelevanceSettings::midDistance);
	NetRelevanceSettings::bytesPerTick = settings->getInt("hosting/updatebytespertick");

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
	registerSoundFunctions(pd.luaState);
	registerSkyFunctions(pd.luaState);

	///Server just has one physics world that's started when the program starts and stays until shutdown, unlike client
	pd.physicsWorld = std::make_shared<PhysicsWorld>();
	SimObject::world = pd.physicsWorld;

	pd.dynamics = new ObjHolder<Dynamic>(SimObjectType::DynamicTypeId, server);
	pd.dynamics->makeLuaMetatable(pd.luaState, "metatable_dynamic", getDynamicFunctions(pd.luaState));
	//Items copy every dynamic function, so they come after
	registerItemFunctions(pd.luaState);
	pd.statics = new ObjHolder<StaticObject>(SimObjectType::StaticTypeId, server);
	pd.statics->makeLuaMetatable(pd.luaState, "metatable_static", getStaticFunctions(pd.luaState));
	pd.lights = new ObjHolder<Light>(SimObjectType::LightTypeId, server);
	pd.lights->makeLuaMetatable(pd.luaState, "metatable_light", getLightFunctions(pd.luaState));
	pd.emitters = new ObjHolder<Emitter>(SimObjectType::EmitterTypeId, server);
	pd.emitters->makeLuaMetatable(pd.luaState, "metatable_emitter", getEmitterFunctions(pd.luaState));
	pd.vehicles = new ObjHolder<Vehicle>(SimObjectType::VehicleTypeId, server);
	pd.vehicles->makeLuaMetatable(pd.luaState, "metatable_vehicle", getVehicleFunctions(pd.luaState));
	pd.bricks = new BrickHolder(pd.physicsWorld, &pd.brickTypes, server);
	//Music, lights, and emitters put on bricks come and go with them
	pd.bricks->spawnAttachments = updateBrickAttachments;
	pd.bricks->removeAttachments = removeBrickAttachments;
	pd.brickTypes.load("Assets/brick/types");
	pd.prints.load("Assets/brick/prints");
	pd.bricks->makeLuaMetatable(pd.luaState, "metatable_brick", getBrickFunctions(pd.luaState));

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
	/*
		Single player shares the one SimObject::world with the client, which points it back at its own world after
		every tick of ours, and takes it away entirely when it leaves a server. Our objects take their bodies out of
		whatever it points at as they're destroyed below, so aim it at ours first, then hand back what was there
	*/
	std::shared_ptr<PhysicsWorld> clientWorld = SimObject::world == pd.physicsWorld ? nullptr : SimObject::world;
	SimObject::world = pd.physicsWorld;

	delete pd.eventManager;
	delete scheduler;

	LUA_args = nullptr;
	LUA_pd = nullptr;

	//Removes brick bodies, so it has to go before the physics world
	delete pd.bricks;
	pd.bricks = nullptr;

	//Their bodies and wheels are in the physics world too
	if (pd.vehicles)
	{
		pd.vehicles->destroyAll();
		delete pd.vehicles;
		pd.vehicles = nullptr;
	}

	pd.physicsWorld.reset();
	SimObject::world = clientWorld;

	delete pd.dynamics;
	delete pd.statics;
	delete pd.lights;
	delete pd.emitters;

	pd.dynamicTypes.clear();
	pd.allNetTypes.clear();

	delete server;
	LUA_server = nullptr;
}
