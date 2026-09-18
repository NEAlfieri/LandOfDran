#pragma once

#include "../NetTypes/DynamicType.h"
#include "../SimObjects/Dynamic.h"
#include "../SimObjects/StaticObject.h"
#include "../SimObjects/Light.h"
#include "../SimObjects/Emitter.h"
#include "../SimObjects/Item.h"
#include "../SimObjects/Vehicle.h"
#include "../Networking/ObjHolder.h"
#include "../Graphics/PlayerCamera.h"
#include "../GameLoop/PlayerController.h"
#include "../Bricks/BrickHolder.h"
#include "../Bricks/BrickDebris.h"
#include "../Graphics/DayCycle.h"

/*
	Client only
	A colored vignette wobbling the picture like being under the water, from Lua's client:setVignette, see VignettePacket
	Drawn by underwater.frag over the finished scene, alpha and strength both fade to nothing as elapsedMS reaches durationMS
*/
struct ScreenVignette
{
	glm::vec3 color = glm::vec3(1, 0, 0);
	//How opaque the color is at the edges of the screen as the effect starts
	float alpha = 0.0f;
	//How hard the picture wobbles as the effect starts, 0 for none, 1 about as much as being underwater
	float strength = 0.0f;
	float durationMS = 0.0f;
	float elapsedMS = 0.0f;

	bool active() const { return durationMS > 0.0f && elapsedMS < durationMS; }

	//1 as the effect starts, down to 0 as its duration runs out
	float fade() const { return active() ? 1.0f - elapsedMS / durationMS : 0.0f; }

	void clear() { durationMS = 0.0f; elapsedMS = 0.0f; }
};

/*
	Client only
	SimObjects and SimObjectTypes
	Any in-game object that has state managed by the server
	Created on server join, deleted when leaving server
*/
struct Simulation
{
	float serverLastSlowestFrame = 0.0;
	float serverAverageFrame = 0.0;

	//See WorldStateUpdatePacket, time also advances locally every frame between updates
	double worldTimeSeconds = DAY_LENGTH_SECONDS * 0.5;
	float timeScale = 1.0;
	bool waterEnabled = false;
	float waterLevel = 0.0;
	//0-1 as the server set it, the client eases toward it, see Rain
	float rainIntensity = 0.0;
	DayCycle dayCycle;

	//Day and night skybox paths, "" for the plain sky, see SkyboxPathsPacket and Skybox::update
	std::string skyboxPaths[2];

	//Whether the server lets us use jets and a flashlight, see PlayerAbilitiesPacket
	bool jetsEnabled = true;
	bool flashlightEnabled = true;

	//Whether it lets us drop the camera off our player and fly it around, which is admins only unless its Lua says otherwise
	bool freeCameraEnabled = false;

	//The vignette Lua's client:setVignette has over our screen, if any, see VignettePacket
	ScreenVignette vignette;

	//Only stored if we succesfully managed to log in to the server we're currently playing on
	std::string evalPassword = "";

	/*
		How many snapshots to hold onto, so we don't need to poll SettingsManager each updateSimObjects packet
	*/
	float idealBufferSize = 5.0;

	std::shared_ptr<Camera>			camera = nullptr;

	//Types:
	std::vector<std::shared_ptr<DynamicType>> dynamicTypes;

	//See TakeOverPhysicsPacket which can add or remove to this list
	std::vector<std::shared_ptr<Dynamic>> controlledDynamics;

	//Probably a lot of overlap between targets and controlledDynamics
	std::vector<std::shared_ptr<PlayerController>> controllers;

	//Net IDs of the items in each of our inventory slots, NO_ID for an empty one, see InventoryContentsPacket
	netIDType inventory[inventorySize] = { NO_ID, NO_ID, NO_ID, NO_ID, NO_ID };

	//Objects (object holders):
	ObjHolder<Dynamic>* dynamics = nullptr;
	ObjHolder<StaticObject>* statics = nullptr;
	ObjHolder<Light>* lights = nullptr;
	ObjHolder<Emitter>* emitters = nullptr;
	ObjHolder<Vehicle>* vehicles = nullptr;

	//Goes up whenever statics are added, removed, or changed, so point light shadows know to redraw
	unsigned int staticsChanged = 0;
	BrickHolder* bricks = nullptr;
	BrickDebris* brickDebris = nullptr;

	/*
		Special brick types are matched by name, since the server and client may have found theirs in a different order
		Indexed by the server's type ID, gives ours, or 0 for a type we don't have, see SpecialBrickTypesPacket
	*/
	std::vector<uint16_t> brickTypeFromServer;
	//Indexed by our type ID, gives the server's, or 0 if the server doesn't have it
	std::vector<uint16_t> brickTypeToServer;

	/*
		Prints are matched by name the same way, see BrickPrintTypesPacket
		Indexed by the server's print ID, gives ours, or 0 for a print we don't have
	*/
	std::vector<uint16_t> printFromServer;
	//Every print name the server offers, in its own order, for the wrench dialog to pick from
	std::vector<std::string> serverPrintNames;

	//A vehicle save we asked for from its wrench dialog, where it goes and as much of it as has arrived, see VehicleSaveDataPacket
	struct PendingVehicleSave
	{
		std::string path;
		std::string bytes;
	};
	//By vehicle net ID
	std::map<netIDType, PendingVehicleSave> vehicleSaves;

	//A save of every brick we asked for from the saved bricks window, with the picture we drew to put in it, see BrickSaveDataPacket
	struct PendingBrickSave
	{
		std::string path;
		std::string thumbnail;
		std::string bytes;
	};
	//By the request ID we sent
	std::map<uint32_t, PendingBrickSave> brickSaves;
};
