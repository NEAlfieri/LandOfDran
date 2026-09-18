#pragma once

#include "../Networking/JoinedClient.h"
#include "../Networking/ClickAction.h"
#include "../SimObjects/Dynamic.h"
#include "../SimObjects/Light.h"
#include "../SimObjects/Item.h"
#include "PlayerController.h"
#include "PlayerAppearance.h"

#include <set>

struct ServerProgramData;
class Vehicle;

//Basically JoinedClient is lower level and used by the server for networking, ClientData is used in server-side packet functions
//ClientData contains references to a JoinedClient but also anything else that client 'owns' like a player, a camera, bricks, etc.
struct ClientData
{
	//Smart pointer to give to owned objects that want to refer to this object, managed by ServerProgramData methods
	std::shared_ptr<ClientData> me;

	//Lower level networking stuff
	std::shared_ptr<JoinedClient> client;

	//The client also keeps one of these client-side
	//Attaches to one or more dynamics like players etc, handles movement from movement keys
	std::vector<PlayerController> controllers;

	//Physics objects like the player that this client handles primary simulation of
	std::vector<std::shared_ptr<Dynamic>> controlledObjects;

	//IDs of bricks this client planted, newest last, for undo
	std::vector<netIDType> plantedBricks;

	//The brick in the last wrench dialog sent to them, the only one a WrenchSubmit from them can change, NO_ID once they've submitted
	netIDType wrenchedBrickID = NO_ID;

	//Same for the last vehicle wrench dialog
	netIDType wrenchedVehicleID = NO_ID;

	//The brick in the last print menu sent to them, the only one a PrintSubmit from them can change, NO_ID once they've submitted
	netIDType printedBrickID = NO_ID;

	//Voice chat, see Networking/PacketsFromClient/VoiceFrame.cpp
	//Lua's client:setVoiceMuted, their voice is dropped while it's set
	bool voiceMuted = false;
	//Between the ClientStartTalking and ClientStopTalking events
	bool talking = false;
	//SDL_GetTicks of their last voice packet, see LoopServer::endQuietTalkers
	unsigned int lastVoiceMS = 0;
	//Net IDs of clients whose names this client has been sent, so it can show who's talking
	std::set<netIDType> knownTalkers;

	//Lua's client:setJetsEnabled and client:setFlashlightEnabled, both allowed unless Lua says otherwise
	bool jetsEnabled = true;
	bool flashlightEnabled = true;

	/*
		Lua's client:setFreeCameraEnabled: whether they can drop their camera off their player and fly it around.
		Off for everyone but admins, who are given it as they log in, since it sees the whole map, see setFreeCamera
	*/
	bool freeCameraEnabled = false;

	//Whether their camera is off flying right now, from FreeCameraRequest packets
	bool freeCamera = false;

	//The light that marks where their loose camera is while it flies, see setFreeCamera
	std::weak_ptr<Light> freeCameraLight;

	//Their flashlight while it's on, held by the target of their first controller, see setFlashlight
	std::weak_ptr<Light> flashlight;

	//How they want their player to look, from their game as they connect, see Networking/PacketsFromClient/AppearanceChoice.cpp
	PlayerAppearance appearance;

	//The last dynamic Lua's client:applyAppearance put it on, which gets their changes if they save new ones while playing
	std::weak_ptr<Dynamic> appearanceTarget;

	//The items they carry by slot, an expired one is an empty slot, see Item
	std::weak_ptr<Item> inventory[inventorySize];

	//The slot their item bar has picked and whether it's out, which puts that slot's item in their player's hand, from InventorySelect packets
	int selectedSlot = 0;
	bool inventoryOpen = false;

	//An item in their hand that isn't in any of their slots, Lua's client:setHandItem, which their item bar can't reach and is held instead of whatever it has picked
	//Inventory.lua puts a paint can here while their palette is out
	std::weak_ptr<Item> handItem;

	//Whether their paint palette wants a paint can in their hand, from PaintCanRequest packets, see Lua's ClientPaintCan
	bool paintCanOut = false;

	//The color and material their paint palette has picked, from PaintChoice packets, see Lua's client:getPaintColor
	glm::u8vec4 paintColor = glm::u8vec4(255, 255, 255, 255);
	unsigned char paintMaterial = 0;

	//The vehicle their player is driving or riding on, see LuaFunctions/VehicleLua.h
	std::weak_ptr<Vehicle> vehicle;
	//Which of its passenger seats they ride on, Vehicle::driverSeat while driving it
	int vehicleSeat = -1;

	//SDL_GetTicks of the last time they honked while driving, and sliced bricks into a vehicle
	unsigned int lastHonkMS = 0;
	unsigned int lastSliceMS = 0;

	//SDL_GetTicks of the last time they saved a vehicle to their computer and loaded one from it, see Networking/PacketsFromClient/VehicleFiles.cpp
	unsigned int lastVehicleSaveMS = 0;
	unsigned int lastVehicleLoadMS = 0;

	//A vehicle save they're uploading, as much of it as has arrived, and the ID their game gave that upload
	std::string vehicleUpload;
	uint32_t vehicleUploadID = 0;

	//The same for a save of bricks they're uploading to load, see Networking/PacketsFromClient/BrickSaveFiles.cpp
	std::string brickUpload;
	uint32_t brickUploadID = 0;

	//SDL_GetTicks of the last time they asked for a save of the bricks and loaded one they uploaded
	unsigned int lastBrickSaveMS = 0;
	unsigned int lastBrickLoadMS = 0;

	//Puts an item that's on the ground into a slot, or the first empty one for -1. Returns the slot, or -1 if that slot is taken or none are free
	int addItem(const ServerProgramData* pd, const std::shared_ptr<Item>& item, int slot = -1);

	//Takes the item out of a slot and back into the world just in front of their player, or where it last was without one. nullptr for an empty slot
	std::shared_ptr<Item> removeItem(const ServerProgramData* pd, int slot);

	//Puts an item that's on the ground in their hand without using a slot, or takes the one that's there back out into the world for nullptr
	//Returns whatever was in their hand before, which also goes back into the world, or nullptr
	std::shared_ptr<Item> setHandItem(const ServerProgramData* pd, const std::shared_ptr<Item>& item);

	//The item in their player's hand, whatever setHandItem put there, otherwise the slot their item bar has picked while it's out. nullptr for an empty hand
	std::shared_ptr<Item> getHeldItem() const;

	//Where an item they're carrying goes when it leaves their inventory, just in front of their player or where it last was without one
	btTransform droppedItemTransform(const Item& item) const;

	//Empties the slot an item is in without putting it anywhere, for an item being destroyed
	void forgetItem(const Item& item);

	//Everything they carry goes back into the world, for when they leave
	void dropAllItems(const ServerProgramData* pd);

	//Tells their game which items are in which of their slots
	void sendInventory() const;

	/*
		Tells their game what to play the moment they click with an item, before it hears back about
		the click at all, see Networking/ClickAction.h. Only the look of it: the shot itself is still
		worked out here. Send one with no item to stop predicting anything
	*/
	void sendClickAction(const ClickAction& action) const;

	//Goes up with every click action sent, so their game can tell a replaced one from the one running
	mutable uint32_t clickActionGeneration = 0;

	//Turns their flashlight on in color (or just recolors it), or off, playing LightOn or LightOff from their player for anyone nearby
	//Won't turn it on while flashlightEnabled is off, or without a player from setDefaultController to hold it
	void setFlashlight(const ServerProgramData* pd, bool on, const glm::vec3& color);

	/*
		Their camera comes off their player to fly around, or goes back on it. Their player stays where it is either way:
		their game is what moves it, see LoopClient::dropPlayerAtCamera
		While it's loose a light marks where it is for everyone, moved to their camera each tick by LoopServer::updatePlayerAbilities
		Won't come loose while freeCameraEnabled is off
	*/
	void setFreeCamera(const ServerProgramData* pd, bool on);

	//Where their camera is, which is where they're watching from while it flies, see setFreeCamera
	glm::vec3 getCameraPosition() const;

	//Tells their game whether jets and the flashlight are enabled, so it doesn't jet on its own when the server won't
	void sendAbilities() const;

	//Removes their flashlight and jet flames, for when they leave
	void removeEffects(const ServerProgramData* pd);

	//Lets their player out of whatever they're driving, without ClientExitVehicle, for when they leave
	void leaveVehicle();
};
