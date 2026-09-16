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

	//Moves item swings along and draws carried items in their holders' hands, or hides them, after the camera moves for the frame
	void placeHeldItems(float deltaT);

	//Puts every dynamic with a name tag (players' names, see serverstart.lua) on screen over its head, for the GUI to draw
	void updateNameTags();

	//Right mouse got us into or out of a vehicle, so it doesn't jet until it's let go
	bool jetSuppressed = false;

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
