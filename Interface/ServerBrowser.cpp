#include "ServerBrowser.h"

void ServerBrowser::render(ImGuiIO* io)
{
	if (!opened)
		return;

	//Server browser window will already display errors relating to not being able to connect
	if (lastKickReason == KickReason::ConnectionRejected)
		lastKickReason = NotKicked;

	//Server just disconnected us, show a pop-up to the user with why until they acknowledge it
	if (lastKickReason != NotKicked)
	{
		ImGui::OpenPopup("Disconnected");
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Disconnected", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			std::string message = "";

			switch (lastKickReason)
			{
			case KickReason::LuaKick:
				message = "You were kicked by someone using an add-on.";
				break;
			case KickReason::ServerShutdown:
				message = "The server has shut down.";
				break;
			case KickReason::OtherReason:
				message = "Connected for an unknown reason, server may have been forcibly stopped.";
				break;
			}

			ImGui::Text(message.c_str());
			if (ImGui::Button("Cancel")) //acknowledged
				lastKickReason = NotKicked;
			ImGui::EndPopup();
		}
	}

	if (!ImGui::Begin("Server Browser", &opened))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Naturally, this is just a placeholder.");
	if (ImGui::InputText("IP", serverAddressBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue))
		serverPicked = true; //Hit enter while this text box is selected
	ImGui::InputInt("Port", &serverPort);
	if (ImGui::InputText("Username", userNameBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue))
		serverPicked = true; //Hit enter while this text box is selected

	bool inputOkay = true;

	//I assume 7 characters is the absolute min for an ip for example 1.1.1.1
	std::string ip = std::string(serverAddressBuffer);
	std::string userName = std::string(userNameBuffer);
	if (ip.length() < 7)
		inputOkay = false;
	if (serverPort < 0 || serverPort > 65535)
		inputOkay = false;
	if (userName.length() < 1 || userName.length() > 64)
		inputOkay = false;

	if (!inputOkay)
	{
		serverPicked = false;
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Join Server"))
		serverPicked = true;
	if(!inputOkay)
		ImGui::EndDisabled();

	ImGui::SameLine();

	bool nameOkay = userName.length() >= 1 && userName.length() <= 64;
	if (!nameOkay)
		ImGui::BeginDisabled();
	if (ImGui::Button("Start Server"))
		singlePlayerPicked = true;
	if (!nameOkay)
		ImGui::EndDisabled();

	if (connectionNote.length() > 0)
		ImGui::Text(connectionNote.c_str());
	else
		ImGui::NewLine();

	if (desiredTypes != 0)
	{
		float progress = ((float)loadedTypes) / ((float)desiredTypes);
		ImGui::ProgressBar(progress);
	}

	ImGui::End();
}

void ServerBrowser::passDefaultSettings(std::string ip, int port, std::string username)
{
	memcpy(serverAddressBuffer, ip.c_str(), ip.length());
	serverAddressBuffer[ip.length()] = 0;
	serverPort = port;
	memcpy(userNameBuffer, username.c_str(), username.length());
	userNameBuffer[username.length()] = 0;
}

void ServerBrowser::passLoadProgress(int _desiredTypes, int _loadedTypes)
{
	desiredTypes = _desiredTypes;
	loadedTypes = _loadedTypes;
}

void ServerBrowser::setConnectionNote(const std::string& message)
{
	connectionNote = message;
}

void ServerBrowser::getServerData(std::string& ip, int& port,std::string &username)
{
	serverPicked = false;
	ip = std::string(serverAddressBuffer);
	port = serverPort;
	username = std::string(userNameBuffer);
}

void ServerBrowser::init()
{

}

void ServerBrowser::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

ServerBrowser::ServerBrowser()
{
	name = "Server Browser";

	const char* defaultAddress = "localhost";
	memcpy(serverAddressBuffer, defaultAddress, strlen(defaultAddress));
	serverAddressBuffer[9] = 0; 

	const char* defaultName = "Guest";
	memcpy(userNameBuffer, defaultName, strlen(defaultName));
	userNameBuffer[5] = 0;
}

ServerBrowser::~ServerBrowser()
{

}
