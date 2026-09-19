#include "ContentDownloadWindow.h"

void ContentDownloadWindow::init()
{
	name = "Custom Content";
	initalized = true;
}

void ContentDownloadWindow::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

void ContentDownloadWindow::show(const std::string& server, const std::vector<ContentFile>& missing)
{
	entries.clear();
	for (const ContentFile& file : missing)
	{
		Entry entry;
		entry.file = file;
		entry.wanted = true;
		entries.push_back(entry);
	}

	serverName = server;
	awaitingChoice = true;
	joinPicked = false;
	cancelPicked = false;
	downloading = false;

	open();
}

bool ContentDownloadWindow::takeChoice(std::vector<uint16_t>& wanted)
{
	if (!joinPicked)
		return false;

	joinPicked = false;
	awaitingChoice = false;
	downloading = true;

	wanted.clear();
	for (const Entry& entry : entries)
	{
		if (entry.wanted)
			wanted.push_back(entry.file.id);
	}

	//Nothing to wait for, so the window has nothing left to show
	if (wanted.empty())
	{
		downloading = false;
		close();
	}

	return true;
}

bool ContentDownloadWindow::takeCancel()
{
	//Closing the window is the same as cancelling: the server is waiting on an answer that would never come
	if (awaitingChoice && !opened)
		cancelPicked = true;

	if (!cancelPicked)
		return false;

	cancelPicked = false;
	awaitingChoice = false;
	downloading = false;
	return true;
}

void ContentDownloadWindow::reset()
{
	entries.clear();
	serverName = "";
	awaitingChoice = false;
	joinPicked = false;
	cancelPicked = false;
	downloading = false;
	close();
}

void ContentDownloadWindow::render(ImGuiIO* io)
{
	if (!opened)
		return;

	//The files asked for are arriving, there's nothing left to pick
	if (downloading)
	{
		if (!contentFiles().downloading())
		{
			downloading = false;
			close();
			return;
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (!ImGui::Begin("Downloading Content", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("Downloading from %s", serverName.c_str());
		ImGui::ProgressBar(contentFiles().getProgress(), ImVec2(ImGui::GetFontSize() * 20.0f, 0));
		ImGui::TextDisabled("%s of %s",
			contentFileSizeName((uint32_t)contentFiles().getDownloadDone()).c_str(),
			contentFileSizeName((uint32_t)contentFiles().getDownloadTotal()).c_str());

		ImGui::End();
		return;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::Begin("Custom Content", &opened, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("%s wants to send you files it uses that you don't have.", serverName.c_str());
	ImGui::TextDisabled("They're kept in %s, your own copies of anything are left alone.", contentDownloadFolder);
	ImGui::Separator();

	float width = ImGui::GetFontSize() * 34.0f;
	float height = ImGui::GetTextLineHeightWithSpacing() * std::clamp((float)entries.size() + 1.0f, 4.0f, 14.0f);

	if (ImGui::BeginTable("##Files", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(width, height)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Get", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.5f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.0f);
		ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.0f);
		ImGui::TableHeadersRow();

		for (size_t a = 0; a < entries.size(); a++)
		{
			Entry& entry = entries[a];

			ImGui::TableNextRow();
			ImGui::PushID((int)a);

			ImGui::TableSetColumnIndex(0);
			ImGui::Checkbox("##Wanted", &entry.wanted);

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(contentFileKindName(entry.file.kind).c_str());

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(entry.file.path.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", entry.file.path.c_str());

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(contentFileSizeName(entry.file.size).c_str());

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	unsigned int pickedFiles = 0;
	uint64_t pickedBytes = 0;
	for (const Entry& entry : entries)
	{
		if (!entry.wanted)
			continue;
		pickedFiles++;
		pickedBytes += entry.file.size;
	}

	ImGui::Text("Downloading %u of %u files, %s", pickedFiles, (unsigned int)entries.size(), contentFileSizeName((uint32_t)pickedBytes).c_str());

	if (ImGui::Button("All"))
	{
		for (Entry& entry : entries)
			entry.wanted = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("None"))
	{
		for (Entry& entry : entries)
			entry.wanted = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Join"))
		joinPicked = true;
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		cancelPicked = true;

	ImGui::TextDisabled("Anything you skip just won't be there: models won't show and sounds won't play.");

	ImGui::End();
}
