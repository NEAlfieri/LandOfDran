#include "ItemHotbar.h"

static constexpr float slideMS = 150.0f;

//How much bigger than its plain layout the bar is drawn, its text included
static constexpr float barScale = 1.5f;

static constexpr float slotSize = 64.0f * barScale;
static constexpr float slotGap = 6.0f * barScale;
static constexpr float padding = 8.0f * barScale;
static constexpr float iconInset = 4.0f * barScale;
static constexpr float slotRounding = 4.0f * barScale;
static constexpr float screenMargin = 10.0f;

static const ImU32 highlightColor = IM_COL32(255, 190, 0, 255);
static const ImU32 panelColor = IM_COL32(20, 20, 25, 190);

void ItemHotbar::toggle()
{
	up = !up;
	changed = true;
}

void ItemHotbar::putAway()
{
	if (!up)
		return;

	up = false;
	changed = true;
}

bool ItemHotbar::scroll(int amount)
{
	if (!up)
		return false;

	//Wheel up moves up the column
	if (amount != 0)
	{
		selected = ((selected - amount) % slotCount + slotCount) % slotCount;
		changed = true;
	}

	return true;
}

void ItemHotbar::setSlot(int slot, bool filled, const std::string& name, Texture* icon, bool iconIsRender)
{
	if (slot < 0 || slot >= slotCount)
		return;

	slots[slot].filled = filled;
	slots[slot].name = name;
	slots[slot].icon = icon;
	slots[slot].iconIsRender = iconIsRender;
}

bool ItemHotbar::takeChange()
{
	bool result = changed;
	changed = false;
	return result;
}

void ItemHotbar::render(ImGuiIO* io)
{
	unsigned int now = SDL_GetTicks();

	//Restart the slide from wherever the last one got to, so quick taps don't make the bar jump
	if (up != drawnUp)
	{
		float progress = std::clamp((now - slideStartMS) / slideMS, 0.0f, 1.0f);
		slideStartMS = now - (unsigned int)((1.0f - progress) * slideMS);
		drawnUp = up;
	}

	float progress = std::clamp((now - slideStartMS) / slideMS, 0.0f, 1.0f);
	float shown = drawnUp ? progress : 1.0f - progress;
	if (shown <= 0.0f)
		return;

	//Ease out
	shown = 1.0f - (1.0f - shown) * (1.0f - shown);

	//Everything the bar draws its own text at, rather than the size the rest of the interface uses
	float fontSize = ImGui::GetFontSize() * barScale;
	auto textSize = [fontSize](const char* text) { return ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text); };

	float titleHeight = fontSize + 6.0f * barScale;
	float width = slotSize + padding * 2.0f;
	float height = titleHeight + slotCount * slotSize + (slotCount - 1) * slotGap + padding;
	float left = io->DisplaySize.x - (width + screenMargin) * shown;
	float top = floor((io->DisplaySize.y - height) / 2.0f);

	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	draw->AddRectFilled(ImVec2(left, top), ImVec2(left + width, top + height), panelColor, 6.0f * barScale);

	const char* title = "Items";
	ImVec2 titleSize = textSize(title);
	draw->AddText(nullptr, fontSize, ImVec2(left + (width - titleSize.x) / 2.0f, top + 3.0f * barScale), IM_COL32_WHITE, title);

	for (int a = 0; a < slotCount; a++)
	{
		ImVec2 min(left + padding, top + titleHeight + a * (slotSize + slotGap));
		ImVec2 max(min.x + slotSize, min.y + slotSize);

		draw->AddRectFilled(min, max, IM_COL32(60, 60, 70, 200), slotRounding);

		if (slots[a].filled)
		{
			if (slots[a].icon && slots[a].icon->isValid())
			{
				//A picture the game drew comes out of OpenGL bottom row first, so its V runs the other way
				ImVec2 topUV = slots[a].iconIsRender ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
				ImVec2 bottomUV = slots[a].iconIsRender ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
				draw->AddImage((ImTextureID)(intptr_t)slots[a].icon->getHandle(), ImVec2(min.x + iconInset, min.y + iconInset), ImVec2(max.x - iconInset, max.y - iconInset), topUV, bottomUV);
			}
			else
			{
				//No icon, its name wrapped inside the slot instead
				draw->AddText(nullptr, fontSize, ImVec2(min.x + iconInset, min.y + iconInset + fontSize), IM_COL32_WHITE, slots[a].name.c_str(), nullptr, slotSize - iconInset * 2.0f);
			}
		}

		std::string number = std::to_string(a + 1);
		draw->AddText(nullptr, fontSize, ImVec2(min.x + 4.0f * barScale, min.y + 2.0f * barScale), highlightColor, number.c_str());

		if (a == selected)
			draw->AddRect(min, max, highlightColor, slotRounding, 0, 3.0f * barScale);
		else
			draw->AddRect(min, max, IM_COL32(110, 110, 120, 200), slotRounding);
	}

	//The picked item's name beside its slot
	std::string label = slots[selected].filled ? slots[selected].name : "Empty";
	//Left at the size the rest of the interface reads at, barScale is for the bar itself
	ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
	float slotTop = top + titleHeight + selected * (slotSize + slotGap);
	ImVec2 labelMax(left - 6.0f, slotTop + (slotSize + labelSize.y) / 2.0f + 4.0f);
	ImVec2 labelMin(labelMax.x - labelSize.x - 12.0f, labelMax.y - labelSize.y - 8.0f);
	draw->AddRectFilled(labelMin, labelMax, panelColor, 4.0f);
	draw->AddText(ImVec2(labelMin.x + 6.0f, labelMin.y + 4.0f), slots[selected].filled ? IM_COL32_WHITE : IM_COL32(170, 170, 180, 255), label.c_str());
}

void ItemHotbar::init()
{
	initalized = true;
}

void ItemHotbar::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
}

ItemHotbar::ItemHotbar()
{
	name = "Item Hot Bar";
}
