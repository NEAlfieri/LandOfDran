#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"
#include "../Bricks/BrickAttachments.h"
#include "../Bricks/BrickTypes.h"

//A brick's settings as the wrench dialog edits them, sent back to the server in a WrenchSubmit packet, or a vehicle's in a VehicleWrenchSubmit packet
struct WrenchSubmission
{
	netIDType brickID = NO_ID;
	bool collides = true;
	std::string name = "";
	BrickAttachments attachments;

	//A wheel or steering wheel brick gets a section for its vehicle settings
	VehiclePart part = VehiclePart_None;

	//Set instead of brickID for a vehicle, which only has music
	netIDType vehicleID = NO_ID;
};

/*
	Changes a brick's collision, name, music loop, light, and emitter. Its print isn't here, the print gun's menu
	puts that on, see Interface/PrintMenu.h
	The server opens it when a player wrenches a brick, or when Lua calls client:openWrenchDialog, see OpenWrenchDialogPacket
*/
class WrenchDialog : public Window
{
	WrenchSubmission editing;

	//What kind of brick it is, shown at the top
	std::string brickLabel = "";

	//What the server has to pick from, plus the brick's own pick if Lua gave it one that isn't listed
	std::vector<std::string> musicNames;
	std::vector<std::string> emitterNames;

	//The spotlight's direction as sliders, in degrees, a pitch of -90 points straight down
	float lightYaw = 0;
	float lightPitch = 0;

	//What checking Spotlight again brings the cone angle back to
	float lastConeAngle = 60;

	bool submitted = false;

	//The Copy button at the top, which stays on between windows: what the last one held when it closed, put onto the next one it can go on
	bool copying = false;
	bool hasCopy = false;
	bool copiedFromVehicle = false;
	WrenchSubmission copied;

	//Whether the last frame drew the window, so closing it any way, even the X or another dialog opening, is noticed
	bool wasOpen = false;

	//Remembers what was being edited as the window closes, if Copy is on
	void stashCopy();

	//Puts the remembered settings that apply to what's opening onto it, over what the server sent
	void applyCopy();

	//A vehicle's Save: the file name typed, and whether it was clicked
	std::string saveName = "";
	bool saveRequested = false;

	//A vehicle's Remove, once its confirmation is clicked
	bool removeRequested = false;

	//Focused the frame after it opens, and centered for a few frames, since its size is only known once the settings have been measured
	bool justOpened = false;
	int framesToCenter = 0;

	public:

	//Shows a brick's settings from the server, replacing anything that was being edited
	void openFor(const WrenchSubmission& settings, const std::string& label, const std::vector<std::string>& music, const std::vector<std::string>& emitters);

	//True once after Apply is clicked, with what to send
	bool takeSubmission(WrenchSubmission& submission);

	//True once after a vehicle's Save is clicked with a usable name, with the vehicle and the file in Saves/Vehicles to write it to
	bool takeSaveRequest(netIDType& vehicleID, std::string& path);

	//True once after a vehicle's removal is confirmed, with the vehicle
	bool takeRemoveRequest(netIDType& vehicleID);

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	WrenchDialog();
	~WrenchDialog();
};
