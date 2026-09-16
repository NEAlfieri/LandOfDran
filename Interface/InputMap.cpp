#include "InputMap.h"

std::string GetInputCommandString(InputCommand command)
{
    if (command >= UseBrick1 && command <= UseBrick10)
        return "Use Hot Bar Slot " + std::to_string(command - UseBrick1 + 1);

    switch (command)
    {
        case NoCommand: return "No command";
        case EndOfCommands: return "Error";
        case WalkForward: return "Walk Forward";
        case WalkBackward: return "Walk Backward";
        case WalkLeft: return "Walk Left";
        case WalkRight: return "Walk Right";
        case MouseLock: return "Toggle Mouse Lock";
        case OpenOptionsMenu: return "Open Settings";
        case OpenDebugWindow: return "Open Debug Window";
        case OpenChatWindow: return "Chat";
        case FirstThirdPerson: return "Switch 1st/3rd person";
        case Jump: return "Jump";
        case DebugView: return "Debug View";
        case BrickForward: return "Move Brick Forward";
        case BrickBackward: return "Move Brick Backward";
        case BrickLeft: return "Move Brick Left";
        case BrickRight: return "Move Brick Right";
        case BrickUp: return "Brick Up 1 Plate";
        case BrickDown: return "Brick Down 1 Plate";
        case BrickUpThree: return "Brick Up 3 Plates";
        case BrickDownThree: return "Brick Down 3 Plates";
        case BrickRotate: return "Rotate Brick";
        case BrickRotateBack: return "Rotate Brick Back";
        case BrickSuperShift: return "Toggle Brick Super Shift";
        case PlantBrick: return "Plant Brick";
        case OpenBrickSelector: return "Open Brick Selector";
        case HideGhostBrick: return "Put Bricks Away";
        case UndoBrick: return "Undo Last Brick (with Ctrl)";
        case ResizeToggle: return "Toggle Brick Resize Mode";
        case PushToTalk: return "Voice Chat (toggle)";
        case Zoom: return "Zoom (hold)";
        case Flashlight: return "Flashlight (hold for color)";
        case Wrench: return "Wrench (hold and click)";
        case OpenPaintMenu: return "Paint Palette / Next Column";
        case CustomColor: return "Custom Paint Color";
        case OpenInventory: return "Show/Hide Items";
        case DropItem: return "Drop Item (with Ctrl)";
        case StartSelection: return "Select Bricks for Vehicle";
        default: return "Other error";
    }
}

void InputMap::resetKeyStates()
{
    for (int a = 0; a < InputCommand::EndOfCommands; a++)
    {
        keyPolled[a] = false;
        keyToProcess[a] = false;
        keyPressed[a] = false;
    }
}

void InputMap::setPreferences(std::shared_ptr<SettingManager> settings)
{
    for (unsigned int a = 1; a < InputCommand::EndOfCommands; a++)
        settings->addInt("keybinds/" + std::to_string(a), keyForCommand[a]);
}

InputMap::InputMap(std::shared_ptr<SettingManager> settings)
{
    keyForCommand[NoCommand] = SDL_SCANCODE_UNKNOWN;

    //Default key bindings here, anything saved in the settings file replaces them below
    //Forget a key and it'll crash!
    {
        bindKey(WalkForward, SDL_SCANCODE_W);
        bindKey(WalkBackward, SDL_SCANCODE_S);
        bindKey(WalkRight, SDL_SCANCODE_D);
        bindKey(WalkLeft, SDL_SCANCODE_A);
        bindKey(MouseLock, SDL_SCANCODE_M);
        bindKey(OpenOptionsMenu, SDL_SCANCODE_O);
        bindKey(OpenDebugWindow, SDL_SCANCODE_GRAVE);
        bindKey(OpenChatWindow, SDL_SCANCODE_T);
        bindKey(FirstThirdPerson, SDL_SCANCODE_TAB);
        bindKey(Jump, SDL_SCANCODE_SPACE);
        bindKey(DebugView, SDL_SCANCODE_F2);

        //Same as the old game's building controls
        bindKey(BrickForward, SDL_SCANCODE_I);
        bindKey(BrickBackward, SDL_SCANCODE_K);
        bindKey(BrickLeft, SDL_SCANCODE_J);
        bindKey(BrickRight, SDL_SCANCODE_L);
        bindKey(BrickUp, SDL_SCANCODE_PERIOD);
        bindKey(BrickDown, SDL_SCANCODE_COMMA);
        bindKey(BrickUpThree, SDL_SCANCODE_P);
        bindKey(BrickDownThree, SDL_SCANCODE_SEMICOLON);
        bindKey(BrickRotate, SDL_SCANCODE_U);
        bindKey(BrickRotateBack, SDL_SCANCODE_KP_7);
        bindKey(BrickSuperShift, SDL_SCANCODE_LALT);
        bindKey(PlantBrick, SDL_SCANCODE_RETURN);
        bindKey(OpenBrickSelector, SDL_SCANCODE_B);
        bindKey(HideGhostBrick, SDL_SCANCODE_SLASH);
        bindKey(UndoBrick, SDL_SCANCODE_Z);
        bindKey(ResizeToggle, SDL_SCANCODE_LSHIFT);
        bindKey(PushToTalk, SDL_SCANCODE_V);
        bindKey(Zoom, SDL_SCANCODE_F);
        bindKey(Flashlight, SDL_SCANCODE_RIGHTBRACKET);
        //Wrenches without holding the wrench item
        bindKey(Wrench, SDL_SCANCODE_INSERT);
        bindKey(OpenPaintMenu, SDL_SCANCODE_E);
        bindKey(CustomColor, SDL_SCANCODE_RSHIFT);
        bindKey(OpenInventory, SDL_SCANCODE_Q);
        //Shares W with walking forward, dropping is Ctrl plus the bound key
        bindKey(DropItem, SDL_SCANCODE_W);
        //The old game's Insert is the wrench now
        bindKey(StartSelection, SDL_SCANCODE_G);

        //Number keys 1 through 9 then 0, SDL's scancodes for them are in that order
        for (int a = 0; a < 10; a++)
            bindKey((InputCommand)(UseBrick1 + a), (SDL_Scancode)(SDL_SCANCODE_1 + a));
    }

    if (settings)
    {
        //Load key binds from file, these used to be loaded before the defaults above, which then overwrote them
        for (unsigned int a = 1; a < InputCommand::EndOfCommands; a++)
        {
            const PreferencePair* pref = settings->getPreference("keybinds/" + std::to_string(a));
            if (!pref)
                continue;

            keyForCommand[a] = (SDL_Scancode)settings->getInt("keybinds/" + std::to_string(a));
        }

        //Chat used to open its window with C by default, and every bind gets saved, so a saved C is the old default rather than a choice
        if (keyForCommand[OpenChatWindow] == SDL_SCANCODE_C)
            keyForCommand[OpenChatWindow] = SDL_SCANCODE_T;

        for (unsigned int a = 1; a < InputCommand::EndOfCommands; a++)
            settings->addInt("keybinds/" + std::to_string(a), keyForCommand[a]);
    }

    resetKeyStates();
}

SDL_Scancode InputMap::getKeyBind(InputCommand command) const
{
    if (command == NoCommand || command == InputCommand::EndOfCommands)
        return SDL_NUM_SCANCODES;
    return keyForCommand[command];
}

void InputMap::bindKey(InputCommand command, SDL_Scancode key)
{
    if (command == NoCommand || command == InputCommand::EndOfCommands)
        return;
    keyForCommand[command] = key;
}

void InputMap::handleInput(SDL_Event& event)
{
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return;

    //Letting go still counts while a window has the keyboard, since a key that opens a window is usually let go after it opens,
    //and a release that's dropped leaves the command looking held, so its next press is ignored
    if (supressed && event.type == SDL_KEYDOWN)
        return;

    for (unsigned int a = 1; a < InputCommand::EndOfCommands; a++)
    {
        if (keyForCommand[a] == event.key.keysym.scancode)
        {
            if (event.type == SDL_KEYDOWN)
            {
                keyPressed[a] = true;
                if (!keyToProcess[a])
                {
                    keyToProcess[a] = true;
                    keyPolled[a] = false;
                }
            }
            else
            {
                keyPressed[a] = false;
                if (keyPolled[a])
                {
                    keyToProcess[a] = false;
                    keyPolled[a] = false;
                }
            }

            //No break, one key can be bound to more than one command, like W walking and Ctrl+W dropping an item
        }
    }
}

bool InputMap::pollCommand(InputCommand command)
{
    if (supressed)
        return false;

    if (command == InputCommand::EndOfCommands || command == NoCommand)
        return false;

    if (keyToProcess[command])
    {
        if (!keyPolled[command])
        {
            if (!keyPressed[command])
            {
                keyPolled[command] = false;
                keyToProcess[command] = false;
                keyPressed[command] = false;
            }
            else
                keyPolled[command] = true;
            return true;
        }
    }

    return false;
}


bool InputMap::isCommandKeydown(InputCommand command) const
{
    if (supressed)
        return false;

    if (keystates == nullptr)
    {
        scope("inputMap::commandKeyDown");
        error("Need to call getKeyStates first");
        return false;
    }

    if (command == InputCommand::EndOfCommands || command == InputCommand::NoCommand)
        return false;

    SDL_Scancode key = keyForCommand[command];
    return keystates[key];
}
