#include "PrintMenu.h"

#include <cctype>

/*
	The letter prints are named after the key they show, except for the punctuation, which can't be a file
	name on every system. These are the names the Blockland letters add-on gives them, so typing one of
	these puts that print on, see printForCharacter
*/
static const std::unordered_map<char, std::string> punctuationPrints = {
	{ '&', "-and" }, { '\'', "-apostrophe" }, { '*', "-asterisk" }, { '@', "-at" }, { '!', "-bang" },
	{ '^', "-caret" }, { '$', "-dollar" }, { '=', "-equals" }, { '>', "-greater_than" }, { '<', "-less_than" },
	{ '-', "-minus" }, { '%', "-percent" }, { '.', "-period" }, { '+', "-plus" }, { '#', "-pound" },
	{ '?', "-qmark" }, { ' ', "-space" }
};

//"Letters/X" is the letter X, the part of a print's name after its group
static std::string printImageName(const std::string& printName)
{
	size_t slash = printName.rfind('/');
	return slash == std::string::npos ? printName : printName.substr(slash + 1);
}

//The print a typed character puts on, "" if none of them is that character
static std::string printForCharacter(const std::vector<std::string>& names, char typed)
{
	std::string wanted;
	if (isalnum((unsigned char)typed))
		wanted = lowercase(std::string(1, typed));
	else
	{
		auto found = punctuationPrints.find(typed);
		if (found == punctuationPrints.end())
			return "";
		wanted = found->second;
	}

	for (const std::string& name : names)
	{
		if (lowercase(printImageName(name)) == wanted)
			return name;
	}

	return "";
}

PrintMenu::PrintMenu(std::shared_ptr<TextureManager> _textures, const PrintTypes* _prints) : textures(_textures), prints(_prints)
{
	name = "Print Menu";
}

PrintMenu::~PrintMenu()
{
	for (const auto& [printName, icon] : iconCache)
	{
		if (icon)
			icon->markForCleanup();
	}
}

void PrintMenu::init()
{
	initalized = true;
}

Texture* PrintMenu::iconFor(const std::string& printName)
{
	auto cached = iconCache.find(printName);
	if (cached != iconCache.end())
		return cached->second;

	//A print the server has that this game doesn't, or one that ships without a picture, like a video
	std::string path = prints ? prints->getIconPath(printName) : "";
	Texture* icon = path.empty() ? nullptr : textures->createTexture(path);
	if (icon && !icon->isValid())
	{
		icon->markForCleanup();
		icon = nullptr;
	}

	iconCache[printName] = icon;
	return icon;
}

void PrintMenu::openFor(netIDType brick, const std::string& currentPrint, const std::vector<std::string>& printNames)
{
	brickID = brick;
	current = currentPrint;
	names = printNames;

	icons.clear();
	icons.reserve(names.size());
	for (const std::string& printName : names)
		icons.push_back(iconFor(printName));

	submitted = false;
	chosen = "";
	justOpened = true;
	framesToCenter = 3;
	open();
}

void PrintMenu::apply(const std::string& printName)
{
	if (std::find(names.begin(), names.end(), printName) == names.end())
		return;

	chosen = printName;
	submitted = true;
	close();
}

bool PrintMenu::takeSubmission(netIDType& brick, std::string& printName)
{
	if (!submitted)
		return false;

	submitted = false;
	brick = brickID;
	printName = chosen;
	return brick != NO_ID;
}

void PrintMenu::render(ImGuiIO* io)
{
	if (!opened)
		return;

	if (framesToCenter > 0)
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		framesToCenter--;
	}

	if (justOpened)
	{
		ImGui::SetNextWindowFocus();
		justOpened = false;
	}

	if (!ImGui::Begin("Print", &opened, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();

	if (names.empty())
		ImGui::TextDisabled("The server has no prints");
	else
	{
		float buttonSize = ImGui::GetFontSize() * 2.6f;
		float cellWidth = buttonSize + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
		int columns = std::max(1, (int)((ImGui::GetFontSize() * 24.0f) / cellWidth));

		//Scrolls once there are more prints than fit in most of the screen, so Cancel stays in view
		int rows = (int)(names.size() + columns - 1) / columns;
		float rowHeight = buttonSize + style.FramePadding.y * 2.0f + style.ItemSpacing.y;
		float chrome = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y * 2.0f;
		float gridHeight = std::min(rows * rowHeight, std::max(rowHeight, viewport->WorkSize.y * 0.9f - chrome));

		ImVec4 chosenColor = style.Colors[ImGuiCol_ButtonActive];

		ImGui::BeginChild("Prints", ImVec2(columns * cellWidth + style.ScrollbarSize, gridHeight));

		for (size_t a = 0; a < names.size(); a++)
		{
			if (a % columns != 0)
				ImGui::SameLine();

			ImGui::PushID((int)a);
			bool isCurrent = names[a] == current;
			if (isCurrent)
				ImGui::PushStyleColor(ImGuiCol_Button, chosenColor);

			bool clicked;
			if (a < icons.size() && icons[a])
				clicked = ImGui::ImageButton("##print", (ImTextureID)(intptr_t)icons[a]->getHandle(), ImVec2(buttonSize, buttonSize));
			else
				clicked = ImGui::Button("?", ImVec2(buttonSize + style.FramePadding.x * 2.0f, buttonSize + style.FramePadding.y * 2.0f));
			ImGui::SetItemTooltip("%s", names[a].c_str());

			if (isCurrent)
				ImGui::PopStyleColor();
			ImGui::PopID();

			//Picking one puts it on the brick right away, there's nothing else here to apply
			if (clicked)
			{
				apply(names[a]);
				break;
			}
		}

		ImGui::EndChild();
	}

	if (ImGui::Button("Cancel"))
		close();

	ImGui::End();
}

void PrintMenu::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
	//Typing into a text box, like chat, isn't picking a print. WantCaptureKeyboard is no good here:
	//keyboard navigation turns it on for any focused window, this one included
	if (!opened || e.type != SDL_TEXTINPUT || ImGui::GetIO().WantTextInput)
		return;

	//One key at a time, so nothing pasted in counts
	if (e.text.text[0] == '\0' || e.text.text[1] != '\0')
		return;

	std::string printName = printForCharacter(names, e.text.text[0]);
	if (!printName.empty())
		apply(printName);
}
