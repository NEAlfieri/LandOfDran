#include "BrickSaveMenu.h"
#include "../External/stb_image.h"

#include <ctime>

void BrickSaveMenu::refresh()
{
	std::string previous = selected >= 0 && selected < (int)entries.size() ? entries[selected].listing.fileName : "";

	clearTextures();
	entries.clear();
	selected = -1;

	for (BrickSaveListing& save : listBrickSaves(true))
	{
		Entry entry;
		entry.listing = save;

		std::string jpeg = save.info.thumbnail;
		if (jpeg.empty())
			jpeg = drawThumbnail(save);
		if (!jpeg.empty())
			entry.texture = makeTexture(jpeg, save.fileName);

		//The pictures live on the graphics card now
		entry.listing.info.thumbnail.clear();
		entries.push_back(entry);
	}

	for (size_t a = 0; a < entries.size(); a++)
	{
		if (entries[a].listing.fileName == previous)
			selected = (int)a;
	}
}

void BrickSaveMenu::openMenu()
{
	refresh();
	open();
}

bool BrickSaveMenu::takeSaveRequest(std::string& fileName)
{
	if (!saveRequested)
		return false;

	saveRequested = false;
	fileName = saveName;
	return true;
}

bool BrickSaveMenu::takeLoadRequest(std::string& fileName, bool& clear, glm::ivec3& offsetStuds)
{
	if (!loadRequested)
		return false;

	loadRequested = false;
	fileName = loadName;
	clear = loadClear;
	offsetStuds = loadOffset;
	return true;
}

std::string BrickSaveMenu::drawThumbnail(const BrickSaveListing& save)
{
	std::string path = "Saves/" + save.fileName;

	std::error_code errorCode;
	uintmax_t size = std::filesystem::file_size(path, errorCode);
	std::filesystem::file_time_type written = std::filesystem::last_write_time(path, errorCode);

	auto found = drawnThumbnails.find(path);
	if (found != drawnThumbnails.end() && found->second.size == size && found->second.written == written)
		return found->second.jpeg;

	std::vector<Brick> bricks;
	std::ifstream file(path, std::ios::binary);
	if (file.is_open())
	{
		auto keep = [&bricks](Brick& brick) { bricks.push_back(brick); };
		if (save.blockland)
			readBlocklandBricks(file, *types, prints, BlocklandAttachmentLookup(), keep);
		else
			readLodBricks(file, types, prints, keep);
	}

	DrawnThumbnail& drawn = drawnThumbnails[path];
	drawn.size = size;
	drawn.written = written;
	drawn.jpeg = drawBricksFromAbove(bricks);
	return drawn.jpeg;
}

GLuint BrickSaveMenu::makeTexture(const std::string& jpeg, const std::string& fileName)
{
	int width = 0, height = 0, channels = 0;
	unsigned char* pixels = stbi_load_from_memory((const unsigned char*)jpeg.data(), (int)jpeg.size(), &width, &height, &channels, 3);
	if (!pixels || width <= 0 || height <= 0)
	{
		if (pixels)
			stbi_image_free(pixels);
		error("Couldn't read the picture in " + fileName);
		return 0;
	}

	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(pixels);
	return texture;
}

void BrickSaveMenu::clearTextures()
{
	for (Entry& entry : entries)
	{
		if (entry.texture != 0)
			glDeleteTextures(1, &entry.texture);
		entry.texture = 0;
	}
}

void BrickSaveMenu::clearEntries()
{
	clearTextures();
	entries.clear();
	selected = -1;
	saveRequested = false;
	loadRequested = false;
	askOverwrite = false;
	askReplace = false;
	askPartial = false;
}

std::string BrickSaveMenu::stemOf(const std::string& fileName)
{
	size_t dot = fileName.rfind('.');
	return dot == std::string::npos || dot == 0 ? fileName : fileName.substr(0, dot);
}

std::string BrickSaveMenu::dateText(int64_t unixSeconds)
{
	if (unixSeconds <= 0)
		return "";

	time_t when = (time_t)unixSeconds;
	std::tm* local = std::localtime(&when);
	if (!local)
		return "";

	char text[64];
	strftime(text, sizeof(text), "%Y-%m-%d %H:%M", local);
	return text;
}

bool BrickSaveMenu::matchesSearch(const Entry& entry) const
{
	std::string search = lowercase(searchBuffer);
	size_t start = search.find_first_not_of(" \t");
	if (start == std::string::npos)
		return true;
	search = search.substr(start, search.find_last_not_of(" \t") - start + 1);

	return lowercase(entry.listing.fileName).find(search) != std::string::npos || lowercase(entry.listing.info.savedBy).find(search) != std::string::npos;
}

void BrickSaveMenu::renderEntry(int index, float thumbnailSize)
{
	Entry& entry = entries[index];
	const LodSaveInfo& info = entry.listing.info;
	ImGui::PushID(index);

	ImVec2 rowStart = ImGui::GetCursorPos();

	if (ImGui::Selectable("##save", selected == index, 0, ImVec2(0, thumbnailSize)))
	{
		selected = index;
		std::string stem = stemOf(entry.listing.fileName);
		strncpy(nameBuffer, stem.c_str(), sizeof(nameBuffer) - 1);
		nameBuffer[sizeof(nameBuffer) - 1] = 0;
	}
	ImVec2 rowEnd = ImGui::GetCursorPos();

	//The picture and text sit on top of the selectable, which is what takes the click
	ImGui::SetCursorPos(rowStart);
	if (entry.texture != 0)
		ImGui::Image((ImTextureID)(intptr_t)entry.texture, ImVec2(thumbnailSize, thumbnailSize));
	else
	{
		ImVec2 corner = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(corner, ImVec2(corner.x + thumbnailSize, corner.y + thumbnailSize), ImGui::GetColorU32(ImGuiCol_FrameBg));
		ImGui::Dummy(ImVec2(thumbnailSize, thumbnailSize));
	}

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextUnformatted(entry.listing.fileName.c_str());
	ImGui::TextDisabled("%u bricks%s", info.brickCount, entry.listing.blockland ? ", Blockland save" : "");
	if (!info.savedBy.empty())
		ImGui::TextDisabled("Saved by %s", info.savedBy.c_str());
	std::string date = dateText(info.savedAt);
	if (!date.empty())
		ImGui::TextDisabled("%s", date.c_str());
	ImGui::EndGroup();

	ImGui::SetCursorPos(rowEnd);
	ImGui::PopID();
}

void BrickSaveMenu::renderPreview(float previewSize)
{
	ImGui::BeginGroup();

	const Entry* entry = selected >= 0 && selected < (int)entries.size() ? &entries[selected] : nullptr;

	if (entry && entry->texture != 0)
		ImGui::Image((ImTextureID)(intptr_t)entry->texture, ImVec2(previewSize, previewSize));
	else
	{
		ImVec2 corner = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(corner, ImVec2(corner.x + previewSize, corner.y + previewSize), ImGui::GetColorU32(ImGuiCol_FrameBg));
		const char* text = entry ? "No picture" : "Pick a save";
		ImVec2 textSize = ImGui::CalcTextSize(text);
		ImGui::GetWindowDrawList()->AddText(ImVec2(corner.x + (previewSize - textSize.x) * 0.5f, corner.y + (previewSize - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
		ImGui::Dummy(ImVec2(previewSize, previewSize));
	}

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + previewSize);
	if (entry)
	{
		const LodSaveInfo& info = entry->listing.info;
		ImGui::TextUnformatted(entry->listing.fileName.c_str());
		ImGui::TextDisabled("%u bricks", info.brickCount);
		if (entry->listing.blockland)
			ImGui::TextDisabled("Blockland save");
		if (!info.savedBy.empty())
			ImGui::TextDisabled("Saved by %s", info.savedBy.c_str());
		std::string date = dateText(info.savedAt);
		if (!date.empty())
			ImGui::TextDisabled("%s", date.c_str());
	}
	ImGui::PopTextWrapPos();

	ImGui::EndGroup();
}

void BrickSaveMenu::renderPopups()
{
	if (askOverwrite)
	{
		ImGui::OpenPopup("Replace save?");
		askOverwrite = false;
	}
	if (askReplace)
	{
		ImGui::OpenPopup("Replace bricks?");
		askReplace = false;
	}
	if (askPartial)
	{
		ImGui::OpenPopup("Save without everything?");
		askPartial = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Save without everything?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("You aren't an admin here, so the save will have only what your game can see of the bricks:");
		ImGui::TextUnformatted("no brick names, lights, music, emitters, spawns, or who built what.");
		ImGui::TextUnformatted("An admin gets all of it from the server.");
		if (ImGui::Button("Save anyway"))
		{
			if (partialExists)
				ImGui::OpenPopup("Replace save?");
			else
			{
				saveRequested = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		//Replacing an existing save is asked about on top of this, and closing that closes this too
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Replace save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("You already have a save called %s.", saveName.c_str());
			ImGui::TextUnformatted("Replace it with the bricks on this server?");
			bool done = false;
			if (ImGui::Button("Replace"))
			{
				saveRequested = true;
				done = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				done = true;
			if (done)
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			if (done)
				ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Replace save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You already have a save called %s.", saveName.c_str());
		ImGui::TextUnformatted("Replace it with the bricks on this server?");
		if (ImGui::Button("Replace"))
		{
			saveRequested = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Replace bricks?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Every brick on the server goes away, and %s is loaded in their place.", loadName.c_str());
		ImGui::TextUnformatted("Save them first if anyone wants them back.");
		if (ImGui::Button("Replace"))
		{
			loadRequested = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void BrickSaveMenu::render(ImGuiIO* io)
{
	if (!opened)
		return;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::Begin("Saved Bricks", &opened, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		return;
	}

	float font = ImGui::GetFontSize();
	float thumbnailSize = font * 4.5f;
	float listWidth = font * 24.0f;
	//Room for the title, the search box, the controls, and the note for non-admins under the list
	float tallest = std::max(thumbnailSize * 2.0f, ImGui::GetMainViewport()->WorkSize.y - font * 16.5f);
	//The picked save's picture and details beside the list, smaller on a screen too short for the full size
	float previewSize = std::min(font * 12.0f, tallest - ImGui::GetTextLineHeightWithSpacing() * 5.0f);
	//Five saves at a time, fewer on a short screen, and never shorter than the picture beside it, so the window doesn't grow as one is picked
	float listHeight = thumbnailSize * 5.0f + ImGui::GetStyle().ItemSpacing.y * 4.0f;
	listHeight = std::min(listHeight, tallest);
	listHeight = std::max(listHeight, previewSize + ImGui::GetTextLineHeightWithSpacing() * 5.0f);

	//Narrows the list down by name or saver
	ImGui::SetNextItemWidth(listWidth);
	ImGui::InputTextWithHint("##search", "Search saves", searchBuffer, sizeof(searchBuffer));

	//The list, with the picked save's picture beside it
	if (ImGui::BeginChild("##saves", ImVec2(listWidth, listHeight), ImGuiChildFlags_Borders))
	{
		if (entries.empty())
			ImGui::TextDisabled("No saves yet. Save the bricks on a server to make one.");
		else
		{
			int shown = 0;
			for (int a = 0; a < (int)entries.size(); a++)
			{
				if (!matchesSearch(entries[a]))
					continue;
				renderEntry(a, thumbnailSize);
				shown++;
			}
			if (shown == 0)
				ImGui::TextDisabled("No save has that in its name.");
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	renderPreview(previewSize);

	ImGui::Separator();

	//Loading
	const Entry* picked = selected >= 0 && selected < (int)entries.size() ? &entries[selected] : nullptr;

	ImGui::BeginDisabled(!admin || !picked);
	if (ImGui::Button("Load"))
	{
		loadName = picked->listing.fileName;
		loadClear = clearFirst;
		loadOffset = glm::ivec3(offset[0], offset[1], offset[2]);
		if (clearFirst)
			askReplace = true;
		else
			loadRequested = true;
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		if (!admin)
			ImGui::SetTooltip("Only admins can load saves");
		else if (!picked)
			ImGui::SetTooltip("Pick a save from the list");
		else
			ImGui::SetTooltip("Uploads it to the server, which loads it on top of the bricks that are there, where it was saved from plus the offset");
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!admin);
	ImGui::Checkbox("Replace the bricks that are there", &clearFirst);
	ImGui::EndDisabled();
	if (!admin && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Only admins can load saves");

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
		refresh();

	//Where it goes compared to where it was saved, whole bricks over
	ImGui::BeginDisabled(!admin);
	ImGui::TextUnformatted("Offset");
	ImGui::SameLine();
	const char* offsetLabels[3] = { "##offsetX", "##offsetY", "##offsetZ" };
	for (int axis = 0; axis < 3; axis++)
	{
		ImGui::SetNextItemWidth(font * 4.5f);
		ImGui::InputInt(offsetLabels[axis], &offset[axis], 0, 0);
		offset[axis] = std::clamp(offset[axis], -100000, 100000);
		ImGui::SameLine();
	}
	ImGui::TextDisabled("x, y, z in studs, plates, studs");
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Moves the loaded bricks that far from where they were saved, so a save can be loaded beside what's there");

	ImGui::Separator();

	//Saving
	auto requestSave = [this]()
	{
		std::string name = nameBuffer;
		size_t start = name.find_first_not_of(" \t");
		size_t end = name.find_last_not_of(" \t");
		name = start == std::string::npos ? "" : name.substr(start, end - start + 1);
		if (name.empty())
			return;

		//Always written as .lod, so that's the name that could already be taken
		std::string extension = name.length() > 4 ? lowercase(name.substr(name.length() - 4)) : "";
		if (extension != ".lod")
			name += ".lod";
		saveName = name;

		bool exists = false;
		for (const Entry& entry : entries)
		{
			if (lowercase(entry.listing.fileName) == lowercase(name))
				exists = true;
		}

		if (!admin)
		{
			partialExists = exists;
			askPartial = true;
		}
		else if (exists)
			askOverwrite = true;
		else
			saveRequested = true;
	};

	if (!admin)
	{
		ImGui::PushTextWrapPos(listWidth + previewSize + ImGui::GetStyle().ItemSpacing.x);
		ImGui::TextDisabled("Not an admin here, so the save has only what your game can see: no brick names, lights, music, emitters, spawns, or who built what.");
		ImGui::PopTextWrapPos();
	}

	ImGui::TextUnformatted("Save every brick as");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(font * 14.0f);
	bool entered = ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	bool nameEmpty = std::string(nameBuffer).find_first_not_of(" \t") == std::string::npos;
	ImGui::BeginDisabled(nameEmpty);
	bool clicked = ImGui::Button("Save");
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(admin ? "Asks the server for a copy of every brick and writes it as a .lod file in your Saves folder, with a picture of the build as it looks now"
			: "Writes the bricks your game can see as a .lod file in your Saves folder, with a picture of the build as it looks now");

	if ((entered || clicked) && !nameEmpty)
		requestSave();

	renderPopups();

	ImGui::End();
}

void BrickSaveMenu::init()
{

}

void BrickSaveMenu::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

BrickSaveMenu::BrickSaveMenu(const BrickTypes* _types, const PrintTypes* _prints) : types(_types), prints(_prints)
{
	name = "Saved Bricks";
}

BrickSaveMenu::~BrickSaveMenu()
{
	clearTextures();
}
