#pragma once

#include "../LandOfDran.h"
#include "../Utility/SettingManager.h"

/*
	A list of commands that can be bound to keys 
*/
enum InputCommand
{
	NoCommand = 0,
	WalkForward = 1,
	WalkBackward = 2,
	WalkLeft = 3,
	WalkRight = 4,
	MouseLock = 5,
    OpenOptionsMenu = 6,
    OpenDebugWindow = 7,
    OpenChatWindow = 8,
    FirstThirdPerson = 9,
    Jump = 10,
    DebugView = 11,
    BrickForward = 12,
    BrickBackward = 13,
    BrickLeft = 14,
    BrickRight = 15,
    BrickUp = 16,
    BrickDown = 17,
    BrickUpThree = 18,
    BrickDownThree = 19,
    BrickRotate = 20,
    BrickRotateBack = 21,
    BrickSuperShift = 22,
    PlantBrick = 23,
    OpenBrickSelector = 24,
    HideGhostBrick = 25,
    UndoBrick = 26,
    ResizeToggle = 27,
    //Hot bar slots, keep these 10 in order, see BrickHotbar
    UseBrick1 = 28,
    UseBrick2 = 29,
    UseBrick3 = 30,
    UseBrick4 = 31,
    UseBrick5 = 32,
    UseBrick6 = 33,
    UseBrick7 = 34,
    UseBrick8 = 35,
    UseBrick9 = 36,
    UseBrick10 = 37,
    PushToTalk = 38,
    Zoom = 39,
    Flashlight = 40,
    Wrench = 41,
    OpenPaintMenu = 42,
    CustomColor = 43,
    OpenInventory = 44,
    DropItem = 45,
    StartSelection = 46,
    Crawl = 47,
    DropCameraAtPlayer = 48,
    DropPlayerAtCamera = 49,
    CycleSeat = 50,
    TogglePlayerList = 51,
    EndOfCommands = 52
};

//For user interface
std::string GetInputCommandString(InputCommand command);

/*
    Basically this is an intermediate layer that allows the player to make custom key binds
    It uses the SDL event polling frame work, filters it through a preference file, and provides its own event polling framework
    Call InputMap::handleInput in the start of your SDL_PollEvent loop
*/
class InputMap
{
    //One possible command for each key
    SDL_Scancode keyForCommand[InputCommand::EndOfCommands];

    //Is there a previous key press that still needs to be handled?
    bool keyToProcess[InputCommand::EndOfCommands];

    //We can't process a new instance of a command until the key has been released, and the last instance has been processed:
    bool keyPressed[InputCommand::EndOfCommands];
    bool keyPolled[InputCommand::EndOfCommands];

    public:
    //Data directly from SDL_GetKeyboardState, you have to read it in
    const Uint8* keystates = nullptr;

    //We're typing in a text box or something where generally we don't want any keypresses to do anything in-game
    bool supressed = false;

    //Call this during a loop like while(SDL_PollEvent(&e))
    void handleInput(SDL_Event& event);

    //Returns true if a full key press occured on this key since the last poll
    bool pollCommand(InputCommand command);

    //Is the key for this command *currently* down, we don't care about full key presses or anything here
    bool isCommandKeydown(InputCommand command) const;
    
    //Sets what key is bound to what command
    void bindKey(InputCommand command, SDL_Scancode key);

    SDL_Scancode getKeyBind(InputCommand command) const;

    //Clears any currently waiting commands to be polled
    void resetKeyStates();

    //Passes key binds to SettingsManager usually so SettingsManager can then export to file
    void setPreferences(std::shared_ptr<SettingManager> settings);

    //Loads key binds from a preference file if one is given
    InputMap(std::shared_ptr<SettingManager> settings);
};