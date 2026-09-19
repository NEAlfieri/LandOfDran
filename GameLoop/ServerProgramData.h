#pragma once

#include "../LandOfDran.h"

#include "../Networking/ObjHolder.h"
#include "../NetTypes/DynamicType.h"
#include "../SimObjects/Dynamic.h"
#include "../SimObjects/StaticObject.h"
#include "../SimObjects/Light.h"
#include "../SimObjects/Emitter.h"
#include "../SimObjects/Vehicle.h"
#include "../SimObjects/Rope.h"
#include "../LuaFunctions/EventManager.h"
#include "../Physics/PhysicsWorld.h"
#include "../Bricks/BrickHolder.h"
#include "../Bricks/BrickTypes.h"
#include "../Bricks/PrintTypes.h"
#include "../Graphics/DayCycle.h"
#include "../Graphics/WorldDecals.h"
#include "../Utility/ContentFiles.h"
#include "ClientData.h"

/*
	Struct holds state that may be needed to process packets from the client
*/
struct ServerProgramData
{
	//Allow people to run Lua commands remotely with a password
	bool useEvalPassword = false;

	//The password, only works if useEvalPassword is true
	std::string evalPassword = "changeme";

	//True for the embedded server behind "Start Server" (single player): the only way in is the host's own
	//client connecting to itself over loopback, so that connection can safely skip the eval password entirely -
	//useEvalPassword is about remote access, which doesn't apply to it. A real (dedicated) server leaves this
	//false, since it's reachable over the network and always needs the password. See LoopServer's constructor
	bool autoAdminForLoopback = false;

	//Should this client be trusted as admin without going through the eval password?
	bool isTrustedLocalAdmin(JoinedClient* client) const
	{
		return autoAdminForLoopback && client->isLoopback();
	}

	//Time of day, see DAY_LENGTH_SECONDS, starts at noon
	double worldTimeSeconds = DAY_LENGTH_SECONDS * 0.5;
	//In-game seconds per real second, 0 freezes the time of day
	float timeScale = 1.0;
	bool waterEnabled = false;
	float waterLevel = 0.0;
	//0 for no rain up to 1 for a downpour, see setRain
	float rainIntensity = 0.0;
	//Sky, fog, and sun colors for each part of the day, and the fog distances
	DayCycle dayCycle;
	//Day and night skyboxes from setSkybox, "" for the plain sky, a .hdr file or the start of five _0.png to _4.png face files
	std::string skyboxPaths[2];
	//Set when the above change other than time passing normally, or someone joins, so clients hear about it on the next tick
	mutable bool worldStateChanged = true;

	//Someone joined, left, became an admin, or had their score text changed, so LoopServer sends everyone the player list this tick
	mutable bool playerListChanged = false;

	std::shared_ptr<PhysicsWorld>	physicsWorld = nullptr;

	lua_State * luaState = nullptr;
	EventManager * eventManager = nullptr;

	//Includes all of the types from each of the vectors below, used to send them all quickly when someone joins and needs types
	std::vector<std::shared_ptr<NetType>> allNetTypes;
	//Various specific kinds of types
	std::vector<std::shared_ptr<DynamicType>> dynamicTypes;

	//Sounds added with Lua's newSoundType, a sound's index is the ID clients know it by
	struct RegisteredSound
	{
		std::string name = "";
		std::string filePath = "";
		bool isMusic = false;
		//Studs from it within which it plays at full volume, see AudioSystem's SoundType
		float fullVolumeDistance = 5.0f;
	};
	std::vector<RegisteredSound> soundTypes;

	//Slash commands Lua's registerChatSuggestion told us about, so clients can list them while typing one, see ChatWindow::addSuggestion
	struct ChatSuggestion
	{
		//Lowercase, without the slash
		std::string command = "";
		//What the client shows for it, the command's full name and its arguments, like "/kick <player> [reason]"
		std::string text = "";
	};
	std::vector<ChatSuggestion> chatSuggestions;

	/*
		Add-on files Lua's addServerFile offered to send joining clients that don't have them, a file's
		index is the ID clients ask for it by, see Networking/ServerFiles.h
	*/
	std::vector<ContentFile> serverFiles;

	//Lua's addDecalType: a name and the material descriptor clients load for it, a type's index is the ID clients know it by
	struct DecalType
	{
		std::string name = "";
		std::string materialPath = "";
	};
	std::vector<DecalType> decalTypes;

	/*
		Every decal in the world like a bullet hole, oldest first, kept so clients who join later see them too
		They don't fade with time: no more than maxDecals exist at once and the oldest make room, which clients do for
		themselves in the same order, see WorldDecals::trim. One on a brick goes when its brick does
	*/
	mutable std::deque<WorldDecal> decals;
	unsigned int maxDecals = 256;
	//Made since the last tick, they go out after that tick's new bricks so none arrives before the brick it's on, see sendNewDecals
	mutable std::vector<WorldDecal> pendingDecals;
	//Lua's setMaxDecals lowered the limit, which clients hear about after whatever is still in pendingDecals
	mutable bool decalLimitChanged = false;

	//Looping sounds started from Lua, kept so clients who join later hear them too
	struct ActiveSoundLoop
	{
		unsigned int id = 0;
		uint16_t soundID = 0;
		SoundLocationKind kind = SoundLocationFlat;
		glm::vec3 position = glm::vec3(0);
		//Only for SoundLocationDynamic, the loop ends when this does
		std::weak_ptr<Dynamic> dynamic;
		//Only for SoundLocationVehicle, the same
		std::weak_ptr<Vehicle> vehicle;
		float pitch = 1.0f;
		float volume = 1.0f;
	};
	std::vector<ActiveSoundLoop> soundLoops;
	unsigned int nextSoundLoopID = 0;

	//See Audio/ReverbPresets.h, sent to clients as they finish loading
	std::string reverbPreset = "auto";

	//Added with Lua's addParticleType and addEmitterType, an index is the ID clients know it by
	std::vector<ParticleTypeData> particleTypes;
	std::vector<EmitterTypeData> emitterTypes;

	//Lua's addBlocklandLight: the light settings a Blockland light type becomes on a brick loadBlocklandSave loads, by lowercase uiName
	std::unordered_map<std::string, BrickAttachments> blocklandLights;
	//Lua's addBlocklandEmitter: emitter type names by lowercase Blockland uiName, before emitter types' own uiNames are tried
	std::unordered_map<std::string, std::string> blocklandEmitters;

	//Lua's setVoiceRange: how many studs from a talker a client's camera can be and still hear them, 0 for nobody
	float voiceRange = 128.0f;
	//Someone who hasn't sent any voice for this long has stopped talking, see LoopServer::endQuietTalkers
	static constexpr unsigned int voiceTimeoutMS = 500;

	//Lua's setVehicleDirtEmitter: the emitter type new vehicles' wheels throw dirt with, "" for none
	std::string vehicleDirtEmitter = "vehicleDirtEmitter";

	/*
		Lua's registerVehicleSpawn: the vehicles a Vehicle Spawn brick's wrench dialog can pick from, in the order they were registered
		Each is a name for the dialog and the global Lua function that spawns one, called with x, y, z and the brick, see spawnRegisteredVehicle
	*/
	struct VehicleSpawnType
	{
		std::string name = "";
		std::string functionName = "";
	};
	std::vector<VehicleSpawnType> vehicleSpawns;

	/*
		Dynamics Lua walks itself with dynamic:setBotInput, each with the same PlayerController a client's player
		has, run the same way every tick by LoopServer::run. One whose dynamic is gone is dropped there
	*/
	std::vector<PlayerController> botControllers;

	//The controller walking a dynamic, made if there isn't one yet, or nullptr for a dynamic that can't have one
	PlayerController* getBotController(const std::shared_ptr<Dynamic>& dynamic)
	{
		if (!dynamic)
			return nullptr;

		for (PlayerController& controller : botControllers)
		{
			if (controller.target.lock() == dynamic)
				return &controller;
		}

		botControllers.emplace_back();
		PlayerController& made = botControllers.back();
		made.target = dynamic;
		made.serverSide = true;
		made.bot = true;
		return &made;
	}

	//Stops walking a dynamic, whether or not anything was
	void removeBotController(const std::shared_ptr<Dynamic>& dynamic)
	{
		for (unsigned int a = 0; a < botControllers.size(); a++)
		{
			if (botControllers[a].target.lock() == dynamic)
			{
				botControllers.erase(botControllers.begin() + a);
				return;
			}
		}
	}

	//ObjHolders created and destroyed with ServerLoop class
	//All dynamic objects:
	ObjHolder<Dynamic>* dynamics = nullptr;
	ObjHolder<StaticObject> * statics = nullptr;
	ObjHolder<Light> * lights = nullptr;
	ObjHolder<Emitter> * emitters = nullptr;
	ObjHolder<Vehicle> * vehicles = nullptr;
	ObjHolder<Rope> * ropes = nullptr;

	//Vehicles made since the last tick, whose bricks go out once their creation packets have, see LoopServer::run
	mutable std::vector<std::weak_ptr<Vehicle>> vehiclesAwaitingBricks;

	//Created and destroyed with ServerLoop class, like the ObjHolders above
	BrickHolder* bricks = nullptr;

	//Named brick sizes, for loading Blockland saves
	BrickTypes brickTypes;

	//Print names from Assets/brick/prints, which clients pick from in the wrench dialog
	PrintTypes prints;

	//All clients:
	std::vector<std::shared_ptr<ClientData>> clients;

	//Items whose state changed since it was last sent, see LoopServer::updateItems
	mutable std::vector<std::weak_ptr<Item>> changedItems;

	//Sends an item's state to everyone on the next tick, once however many times it changes before then
	void markItemChanged(const std::shared_ptr<Item>& item) const
	{
		if (item->stateChanged)
			return;

		item->stateChanged = true;
		changedItems.push_back(item);
	}

	/*
		Items in the hands of dynamics nobody plays, see dynamic:setHeldItem. A client's items are found through
		their inventory, and a bot has none, so they're kept here for LoopServer::updateItems to carry along with
		their holders and to drop when a holder is gone
	*/
	std::vector<std::weak_ptr<Item>> botHeldItems;

	//What a dynamic nobody plays is holding, or nullptr
	std::shared_ptr<Item> getBotHeldItem(const std::shared_ptr<Dynamic>& holder) const
	{
		if (!holder)
			return nullptr;

		for (const std::weak_ptr<Item>& held : botHeldItems)
		{
			std::shared_ptr<Item> item = held.lock();
			if (item && item->botHolder.lock() == holder)
				return item;
		}

		return nullptr;
	}

	//Takes an item out of the world and puts it in the hand of a dynamic nobody plays, like ClientData::setHandItem does for a client
	void giveBotItem(const std::shared_ptr<Dynamic>& holder, const std::shared_ptr<Item>& item)
	{
		item->removeFromWorld();
		item->botHolder = holder;
		item->slot = -1;
		botHeldItems.push_back(item);
		markItemChanged(item);
	}

	/*
		Drops an item a bot was holding back into the world where its holder stands, which is where updateItems has
		been keeping it. A client's is thrown out in front of them instead, but a bot has no camera to throw it along
	*/
	void dropBotItem(const std::shared_ptr<Item>& item)
	{
		//Whether it was in a bot's hand at all, which is what this list says: its holder may have been destroyed
		bool wasHeld = false;
		for (unsigned int a = 0; a < botHeldItems.size(); a++)
		{
			if (botHeldItems[a].lock() == item)
			{
				botHeldItems.erase(botHeldItems.begin() + a);
				wasHeld = true;
				break;
			}
		}

		if (!wasHeld)
			return;

		item->botHolder.reset();
		item->returnToWorld(item->body->getWorldTransform());
		markItemChanged(item);
	}

	//Basically JoinedClient is lower level and used by the server for networking, ClientData is passed to server-side packet functions
	//ClientData contains references to a JoinedClient but also anything else that client 'owns' like a player, a camera, bricks, etc.
	//Called in Server::run
	void makeClient(std::shared_ptr<JoinedClient> src) const
	{
		auto client = std::make_shared<ClientData>();
		client->me = client;
		client->client = src;
		src->userData = client.get();
		std::vector<std::shared_ptr<ClientData>>* c = const_cast<std::vector<std::shared_ptr<ClientData>>*>( & clients);
		c->push_back(client);
	}

	//O(1) access to client data in packet functions, can return nullptr
	std::shared_ptr<ClientData> getClient(std::shared_ptr<JoinedClient> source) const
	{
		if (!source->userData)
			return nullptr;

		ClientData* ret = (ClientData*)source->userData;
		if (!ret)
			return nullptr;

		return ret->me;
	}

	//Called in Server::run when client leaves
	void removeClient(std::shared_ptr<JoinedClient> src) const
	{
		if (!src->userData)
			return;

		std::vector<std::shared_ptr<ClientData>>* c = const_cast<std::vector<std::shared_ptr<ClientData>>*>(&clients);
		for (unsigned int a = 0; a < c->size(); a++)
		{
			if (c->at(a).get() == src->userData)
			{
				//Out of whatever they were driving, while their player is still around to be let out
				c->at(a)->leaveVehicle();

				//These objects don't have to dissapear if Lua modders don't want them to
				//But we need to clarify all of these objects are *only* owned by Lua now
				c->at(a)->controlledObjects.clear();

				//Whatever Lua left in their inventory goes back into the world
				c->at(a)->dropAllItems(this);

				//Their flashlight and jet flames go with them though, even if their player stays
				c->at(a)->removeEffects(this);

				playerListChanged = true;

				//Get rid of ClientData and JoinedClient structures themselves
				src->userData = nullptr;
				c->at(a)->me.reset();
				c->at(a)->client.reset();
				c->erase(c->begin() + a);
				return;
			}
		}
	}
};
