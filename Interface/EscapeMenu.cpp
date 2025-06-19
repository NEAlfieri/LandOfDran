#include "EscapeMenu.h"

//Get last button press type since the last time this was called
EscapeButtonPressed EscapeMenu::getLastButtonPress()
{
	EscapeButtonPressed ret = lastButtonPress;
	lastButtonPress = EscapeButtonPressed::None;
	return ret;
}

void EscapeMenu::render(ImGuiIO* io)
{
	if (!opened)
		return;

	if (!ImGui::Begin("Escape Menu", &opened))
	{
		ImGui::End();
		return;
	}
	if (showLeaveServer)
	{
		if (ImGui::Button("Leave Server"))
		{
			lastButtonPress = EscapeButtonPressed::LeaveServer;
			close();
		}
	}
	else
	{
		if (ImGui::Button("Join Server"))
		{
			lastButtonPress = EscapeButtonPressed::JoinServer;
			close();
		}
	}
	if (ImGui::Button("Exit Land of Dran"))
	{
		lastButtonPress = EscapeButtonPressed::LeaveGame;
		close();
	}
	if (ImGui::Button("Open Chat (c)"))
	{
		lastButtonPress = EscapeButtonPressed::OpenChat;
		close();
	}
	if (ImGui::Button("Open Settings (o)"))
	{
		lastButtonPress = EscapeButtonPressed::OpenSettings;
		close();
	}
	if (ImGui::Button("Open Debug Menu (`)"))
	{
		lastButtonPress = EscapeButtonPressed::OpenDebugMenu;
		close();
	}

	ImGui::End();
}

void EscapeMenu::init()
{

}

void EscapeMenu::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

EscapeMenu::EscapeMenu()
{
	name = "Escape Menu";
}

EscapeMenu::~EscapeMenu()
{

}
