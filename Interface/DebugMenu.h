#pragma once

#include "../LandOfDran.h"

#include "UserInterface.h"
#include "../Utility/GlobalStartup.h" //getTicksMS

//Stuff we pass to DebugMenu to display
struct NetInfo
{
	float ping = 0;
	float incomingData = 0;
	float outgoingData = 0;
	float serverLastLongestFrame = 0;
	float serverAverageFrame = 0;
};

class DebugMenu : public Window
{
	friend class Window;
	friend class UserInterface; 

	glm::vec3 cameraPosition = glm::vec3(0,0,0);
	glm::vec3 cameraDirection = glm::vec3(0,0,0);
	float lastPing = 0.0;
	float incomingData = 0.0;
	float outgoingData = 0.0;
	float maxIncoming = 0.0;
	float maxOutgoing = 0.0;
	float serverLastLongestFrame = 0.0;
	float serverAverageFrame = 0.0;

	std::vector<std::string> extraLines;

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	bool showVerboseLogging = false;
	bool scrollToBottom = true;
	bool enterBehavior = true;

	//Last command sent, can be retrieved with repeat button
	std::string lastCommand = "";

	char consoleCommandBuffer[10000];
	char passwordBuffer[256];

	//You can comment this out and use the local variable in render if you want client-side stuff to be logged instead
	//By default this logs server console output if authenticated
	std::deque<loggerLine> lastLoggedLines;

	//Have we gotten eval access to the server we are on?
	bool autheticated = false;

	//Has the user entered a password to authenticate?
	bool wantsToAuthenticate = false;

	//We typed something in the lua console and we have authenticated
	bool luaCommandWaiting = false;
	std::string waitingLuaCommand = "";

	DebugMenu();

public:

	//I got tired of extending passDetails for each new thing
	void addExtraLine(const std::string& line) { extraLines.push_back(line); }

	bool isCommandWaiting() const { return luaCommandWaiting; }

	std::string getLuaCommand() { luaCommandWaiting = false; return waitingLuaCommand; }

	void authenticate() { wantsToAuthenticate = false; autheticated = true; }

	//Does getPassword have a password to return?
	bool passwordSubmitted() const { return wantsToAuthenticate; }

	//Resets passwordSubmitted
	std::string getPassword() { wantsToAuthenticate = false; return std::string(passwordBuffer); }

	std::string adminLoginComment = "";

	//Toggled by the DebugView command (LoopClient.cpp) - while true, all dynamics render off their live physics
	//transform instead of the interpolated one, for comparing "true" physics state against what's being displayed.
	//A toggle (rather than held-key) since holding a key while also typing in the Lua console isn't practical
	bool showDebugPhysicsView = false;

	//When leaving a server
	void reset();

	void addLogLine(loggerLine line);

	//Pass info to the debug menu to render and display later on
	void passDetails(const glm::vec3& camPos, const glm::vec3& camDir, const NetInfo& netInfo);

	~DebugMenu();
};




