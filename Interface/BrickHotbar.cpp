#include "BrickHotbar.h"

static constexpr float slideMS = 150.0f;
static constexpr float slotSize = 64.0f;
static constexpr float slotGap = 6.0f;
static constexpr float padding = 8.0f;

static const ImU32 highlightColor = IM_COL32(255, 190, 0, 255);

void BrickHotbar::add(const HotbarBrick& brick)
{
	for (int a = 0; a < slotCount; a++)
	{
		if (filled[a])
			continue;

		slots[a] = brick;
		filled[a] = true;
		if (a == selected)
			changed = true;
		return;
	}

	for (int a = slotCount - 1; a > 0; a--)
		slots[a] = slots[a - 1];
	slots[0] = brick;

	if (selected != -1)
		changed = true;
}

void BrickHotbar::pressSlot(int slot)
{
	if (slot < 0 || slot >= slotCount)
		return;

	//Double tapping a slot's key puts the bricks away
	if (up && selected == slot)
	{
		putAway();
		return;
	}

	up = true;
	selected = slot;
	changed = true;
}

void BrickHotbar::scroll(int amount)
{
	if (!up || amount == 0)
		return;

	int from = selected == -1 ? 0 : selected;
	selected = ((from + amount) % slotCount + slotCount) % slotCount;
	changed = true;
}

void BrickHotbar::save(std::shared_ptr<SettingManager> settings) const
{
	for (int a = 0; a < slotCount; a++)
	{
		std::string path = "hotbar/slot" + std::to_string(a + 1) + "/";
		settings->addBool(path + "filled", filled[a]);

		//An empty slot's old size and name are left in the file, filled says to ignore them
		if (!filled[a])
			continue;

		settings->addInt(path + "width", slots[a].width, true, "", 1, 255);
		settings->addInt(path + "height", slots[a].height, true, "", 1, 255);
		settings->addInt(path + "length", slots[a].length, true, "", 1, 255);
		settings->addString(path + "name", slots[a].name);
		settings->addBool(path + "special", slots[a].special);
	}
}

void BrickHotbar::load(std::shared_ptr<SettingManager> settings, const std::function<Texture*(const std::string&)>& findIcon)
{
	for (int a = 0; a < slotCount; a++)
	{
		std::string path = "hotbar/slot" + std::to_string(a + 1) + "/";
		filled[a] = settings->getBool(path + "filled");
		if (!filled[a])
			continue;

		HotbarBrick brick;
		brick.width = std::clamp(settings->getInt(path + "width"), 1, 255);
		brick.height = std::clamp(settings->getInt(path + "height"), 1, 255);
		brick.length = std::clamp(settings->getInt(path + "length"), 1, 255);
		brick.name = settings->getString(path + "name");
		brick.special = settings->getBool(path + "special");
		brick.icon = findIcon(brick.name);
		slots[a] = brick;
	}
}

void BrickHotbar::putAway()
{
	if (!up && selected == -1)
		return;

	up = false;
	selected = -1;
	changed = true;
}

bool BrickHotbar::isBuilding() const
{
	return up && selected != -1 && filled[selected];
}

bool BrickHotbar::getSelected(HotbarBrick& brick) const
{
	if (!isBuilding())
		return false;

	brick = slots[selected];
	return true;
}

bool BrickHotbar::takeChange()
{
	bool result = changed;
	changed = false;
	return result;
}

void BrickHotbar::render(ImGuiIO* io)
{
	//Nothing of the game's own HUD while the appearance editor is up
	if (userInterface->hideGameHud)
		return;

	unsigned int now = SDL_GetTicks();

	//Restart the slide from wherever the last one got to, so quick taps don't make the bar jump
	bool raise = up || peek;
	if (raise != drawnUp)
	{
		float progress = std::clamp((now - slideStartMS) / slideMS, 0.0f, 1.0f);
		slideStartMS = now - (unsigned int)((1.0f - progress) * slideMS);
		drawnUp = raise;
	}

	float progress = std::clamp((now - slideStartMS) / slideMS, 0.0f, 1.0f);
	float shown = drawnUp ? progress : 1.0f - progress;
	if (shown <= 0.0f)
		return;

	//Ease out
	shown = 1.0f - (1.0f - shown) * (1.0f - shown);

	float titleHeight = ImGui::GetFontSize() + 6.0f;
	float width = slotCount * slotSize + (slotCount - 1) * slotGap + padding * 2.0f;
	float height = titleHeight + slotSize + padding;
	float left = floor((io->DisplaySize.x - width) / 2.0f);
	float top = io->DisplaySize.y - (height + 10.0f) * shown;

	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	draw->AddRectFilled(ImVec2(left, top), ImVec2(left + width, top + height), IM_COL32(20, 20, 25, 190), 6.0f);

	std::string title = "Press 1-0 to build with a slot, B to fill slots";
	if (selected != -1)
		title = filled[selected] ? slots[selected].name : "Empty slot, press B to pick a brick";
	ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
	draw->AddText(ImVec2(left + (width - titleSize.x) / 2.0f, top + 3.0f), IM_COL32_WHITE, title.c_str());

	for (int a = 0; a < slotCount; a++)
	{
		ImVec2 min(left + padding + a * (slotSize + slotGap), top + titleHeight);
		ImVec2 max(min.x + slotSize, min.y + slotSize);

		draw->AddRectFilled(min, max, IM_COL32(60, 60, 70, 200), 4.0f);

		if (filled[a] && slots[a].icon && slots[a].icon->isValid())
			draw->AddImage((ImTextureID)(intptr_t)slots[a].icon->getHandle(), ImVec2(min.x + 4.0f, min.y + 4.0f), ImVec2(max.x - 4.0f, max.y - 4.0f));

		//Keys 1 through 9, then 0 for the last slot
		std::string number = std::to_string((a + 1) % 10);
		draw->AddText(ImVec2(min.x + 4.0f, min.y + 2.0f), highlightColor, number.c_str());

		if (a == selected)
			draw->AddRect(min, max, highlightColor, 4.0f, 0, 3.0f);
		else
			draw->AddRect(min, max, IM_COL32(110, 110, 120, 200), 4.0f);
	}
}

void BrickHotbar::init()
{
	initalized = true;
}

void BrickHotbar::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
}

BrickHotbar::BrickHotbar()
{
	name = "Brick Hot Bar";
}
