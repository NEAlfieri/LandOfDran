#pragma once

#include "../LandOfDran.h"

#include "../Networking/Client.h"
#include "ClientProgramData.h"
#include "Simulation.h"
#include "../Networking/ClientPacketCreators.h"
#include "LoopServer.h"

/*
	This is the big bad class that allows us to separate our client playing loop from
	our server hosting loop along with all the variables and structures specific to it
*/
class LoopClient
{
	//Stuff we need to *play* the game, as opposed to host it, except our net interface itself
	ClientProgramData pd;

	//Anything with state specific to and maintained by the current server we're playing on
	Simulation simulation;

	//Network connection manager, its methods take ClientProgramData as a parameter, so it's separate
	//This could be nullptr so be careful, cmdArgs should be NotInGame if that's the case as well
	Client* client = nullptr;

	//Non-null only for single player: a server hosted in-process that we also connect to as a client
	LoopServer* localServer = nullptr;

	bool valid = false;

	//Per land of dran kino agent special request
	//Technically some UI specific calculations might happen during rendering, oh well
	//Fits the sun's shadow cascades and picks which are redrawn this frame, see the definition
	void pickShadowCascades(bool* drawCascade);

	void renderEverything(float deltaT);

	//Last window of timings written to the log by -profile, so each is only written once
	unsigned int loggedProfilerWindow = 0;

	//Called every frame the program runs. Every frame: in a game, not in a game, loading into a game...
	void handleInput(float deltaT, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	unsigned int lastSentControlledObjects = 0;

	//The server browser comes back the frame the appearance editor closes, but only when it was the browser that opened it
	bool appearanceEditorWasOpen = false;
	bool appearanceEditorFromBrowser = false;

	//The voice chat key toggles talking on and off rather than being held down, see handleInput
	bool voiceToggled = false;

	//The mouse goes back to playing the frame the wrench dialog closes
	bool wrenchDialogWasOpen = false;

	//The display item our crosshair is on within reach, outlined so it's clear it can be taken, NO_ID for none, see updateDisplayItemHighlight
	netIDType highlightedDisplayItem = NO_ID;

	//Outlines the display item (an item a brick offers, see Item::display) under our crosshair within reach, and takes the outline off the last one
	void updateDisplayItemHighlight();

	//Same for the print menu the print gun opens
	bool printMenuWasOpen = false;

	//Same for the custom paint color picker
	bool colorPickerWasOpen = false;

	/*
		Opening the palette puts a paint can in our hand, which stays there until the item bar or the brick bar takes it away
		paletteWasShown catches the moment it comes out, and paintCanSent is the last thing PaintCanRequest told the server
	*/
	bool paletteWasShown = false;
	bool paintCanOut = false;
	bool paintCanSent = false;

	//And the saved vehicles window
	bool vehicleLoaderWasOpen = false;

	//How long Ctrl+undo has been held, and since the last repeated undo, see handleInput
	float undoHeldMS = 0;
	float undoSinceRepeatMS = 0;

	//Flashlight key, see updateFlashlight: whether we asked for it on, how long the key's been down, and where it is in the color cycle (0 is white)
	bool flashlightOn = false;
	float flashlightHeldMS = 0;
	bool flashlightCycling = false;
	float flashlightCycle = 0;
	float flashlightSinceSentMS = 0;
	bool flashlightColorUnsent = false;

	//A tap of the flashlight key turns it on or off, holding it turns it on and cycles its color, sending the server each change
	void updateFlashlight(float deltaT);

	//Puts a light held by a dynamic just past its right hand (or in front of its eyes without one), pointing where our camera does if it's our player. False if the dynamic isn't here
	bool placeHeldLight(Light& light, glm::vec3& position, glm::vec3& direction);

	//Item icons by path, loaded the first time the item bar shows one, nullptr for ones that couldn't be
	std::map<std::string, Texture*> itemIcons;

	//An item type's icon, see itemIcons
	Texture* findItemIcon(const std::string& path);

	//Fills the item bar's slots from the items the server says we carry
	void updateItemHotbar();

	//Draws the model of each item we carry into its own little texture for the item bar to show, see ItemIconRenderer
	void renderItemIcons();

	//Moves item swings along and draws carried items in their holders' hands, or hides them, after the camera moves for the frame
	//The item our own item bar has picked, which is what a click acts with
	std::shared_ptr<Item> getOwnEquippedItem() const;

	void placeHeldItems(float deltaT);

	//Puts every dynamic with a name tag (players' names, see serverstart.lua) on screen over its head, for the GUI to draw
	void updateNameTags();

	/*
		Shines the light the wrench dialog is editing while it's open, in place of the brick's real one, so a change to a
		light shows before it's applied. Ours alone: the server hears nothing about it, and closing the window without
		applying just stops it being drawn, which puts the real light back
		hiddenLightID comes back with the brick's real light, or NO_ID when nothing is being previewed
	*/
	void updateLightPreview(std::vector<PointLightSource>& lights, netIDType& hiddenLightID);

	//The preview itself, which keeps its own flicker, blink, and spin going, nullptr while no light is being edited
	std::shared_ptr<Light> lightPreview = nullptr;

	//Right mouse got us into or out of a vehicle, so it doesn't jet until it's let go
	bool jetSuppressed = false;

	/*
		The free camera (Drop Camera At Player / Drop Player At Camera, F7 and F8): our camera comes off our player and
		flies around with no collision while our player stays where it is, and the server shines a light where it is
		Only if the server allows it, which is admins only unless its Lua says otherwise, see Simulation::freeCameraEnabled
	*/
	bool freeCamera = false;

	//What the camera was bound to before it came loose, put back when our player is dropped at it
	std::weak_ptr<Dynamic> cameraTargetBeforeFlying;
	bool cameraFreePositionBeforeFlying = false;
	bool cameraFreeDirectionBeforeFlying = false;
	bool cameraFreeUpVectorBeforeFlying = false;

	//Drops the camera off our player to fly around, or puts it back on it, telling the server either way
	//Putting it back doesn't move our player, see dropPlayerAtCamera
	void setFreeCamera(bool on);

	//Our player is teleported to where the camera is flying and the camera goes back onto it
	void dropPlayerAtCamera();

	//Our own player, the first dynamic the server gave us control of, nullptr if we have none
	std::shared_ptr<Dynamic> getOwnPlayer() const;

	//Every vehicle's bricks and where they're drawn this frame, for each pass that draws bricks, see renderEverything
	std::vector<InstancedBrickRenderer::GroupDraw> vehicleDraws;

	//The vehicle our player is driving, nullptr if they aren't
	std::shared_ptr<Vehicle> getDrivenVehicle() const;

	//The vehicle our player is driving or riding on as a passenger, nullptr if neither
	std::shared_ptr<Vehicle> getRiddenVehicle() const;

	//Takes drivers and passengers out of the physics world and stands them in their vehicles' seats, and lets out anyone who got out, before the camera moves
	void placeVehicleDrivers(float deltaT);

	//Puts each vehicle wheel's tire where it's drawn, before models update
	void placeVehicleWheels();

	//Draws every model worn on a dynamic, like players' hats, the way the dynamic types' models were just drawn
	void renderPartModels(bool useMaterials) const;

	//Send simulation.controlledObjects physics/transform data to server
	void sendControlledObjects();

	//Handle controllers bound to dynamics
	void updateControllers(float deltaT);

	//Gives dynamics the player's own body is actually touching an immediate local visual reaction instead of waiting
	//for the server to notice the same contact and broadcast a correction. Purely a client-side prediction - see
	//Dynamic::predictLocallyUntil for the tradeoffs
	void predictLocalCollisions();

	//Water surface mesh: waterGridCells by waterGridCells quads reaching at least waterRadius out from the camera, further when the fog ends further out
	static constexpr int waterGridCells = 200;
	static constexpr float waterRadius = 300.0f;

	//Starts ripples for dynamics going into, coming out of, or moving along the water and ages the old ones, after updateSnapshot
	void makeWaterRipples(float deltaT);

	//Has emitters eject their particles and moves every particle along, after the camera and fog distance are set for the frame
	void updateParticles();

	//(Re)creates the water reflection/refraction render targets for the window size and graphics/waterquality
	void createWaterTargets(std::shared_ptr<SettingManager> settings);

	//Picks up graphics/shadowsoftness, and (re)creates the shadow cascades if graphics/shadowresolution changed
	void createShadowTarget(std::shared_ptr<SettingManager> settings);

	//How bright a pixel with nothing but open sky the whole way to the sun comes out, before sunset and camera fades
	static constexpr float godRayStrength = 0.35f;

	//Picks up graphics/godrayquality, and (re)creates the god ray mask for the window size, or frees it when they're off
	void createGodRayTarget(std::shared_ptr<SettingManager> settings);

	//Rays of sunlight past whatever is between the camera and the sun, blended onto the finished scene
	void renderGodRays();

	/*
		Copies graphics/drawdistance out of the camera, which read it as its far plane, into everything that culls
		by distance on the CPU. Call after Camera::updateSettings, which is where the setting is actually read
	*/
	void applyDrawDistance();

	//Sky, models, grass, and bricks from the currently uploaded camera into the currently bound frame buffer
	void renderScene(bool clipAtWater);

	//Transparent bricks and the ghost brick, which don't write depth, so they go after renderScene and the water surface
	void renderTransparent(bool clipAtWater);

public:

	//Constructor have any issues?
	bool isValid() const { return valid; }

	//Clean up performed when leaving a server to return to main menu or joining another server
	void leaveServer(ExecutableArguments& cmdArgs);

	//Joining new server from main menu or another server
	void connectToServer(std::string ip,unsigned int port,std::string userName, ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	//Hosts a server in-process on localhost, then connects to it as a client. Used for the "Start Server" button
	void hostSinglePlayer(ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	//Called every frame the program runs. Every frame: in a game, not in a game, loading into a game...
	void run(float deltaT,ExecutableArguments& cmdArgs, std::shared_ptr<SettingManager> settings);

	/*
		Only called when the program starts initially
		LoopClient is *not* created and destroyed each time you join a server
	*/
	LoopClient(ExecutableArguments & cmdArgs,std::shared_ptr<SettingManager> settings);
	//Only called when the program finally shuts down, *not* called when leaving a server
	~LoopClient();
};
