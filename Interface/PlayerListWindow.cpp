#include "PlayerListWindow.h"

void PlayerListWindow::setPlayers(std::vector<PlayerListEntry>&& newPlayers)
{
	players = std::move(newPlayers);
}

void PlayerListWindow::clear()
{
	players.clear();
}

void PlayerListWindow::show()
{
	open();
	justOpened = true;
}

void PlayerListWindow::toggle()
{
	if (opened)
		close();
	else
		show();
}

void PlayerListWindow::render(ImGuiIO* io)
{
	if (!opened)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), justOpened ? ImGuiCond_Always : ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(680, 380), ImGuiCond_FirstUseEver);
	justOpened = false;

	//Never takes focus, so opening it mid game doesn't take the keyboard away from walking
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	std::string title = "Players (" + std::to_string(players.size()) + ")###Player List";
	if (!ImGui::Begin(title.c_str(), &opened, flags))
	{
		ImGui::End();
		return;
	}

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("players", 3, tableFlags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		//The score is whatever text the server likes, so it gets as much room as names do, and the ping only what "9999 ms" needs
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("9999 ms").x);
		ImGui::TableHeadersRow();

		for (const PlayerListEntry& player : players)
		{
			ImGui::TableNextRow();

			//Our own row is lit up
			if (player.name == ownName)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.6f));

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(player.name.c_str());
			if (player.admin)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(admin)");
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(player.scoreText.c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%u ms", player.pingMS);
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void PlayerListWindow::init()
{
	initalized = true;
}

void PlayerListWindow::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

PlayerListWindow::PlayerListWindow()
{
	name = "Player List";
	hudWindow = true;
}

PlayerListWindow::~PlayerListWindow()
{

}
