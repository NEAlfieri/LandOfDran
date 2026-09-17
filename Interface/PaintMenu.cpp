#include "PaintMenu.h"
#include "../External/Imgui/imgui_internal.h"

//How long the palette stays up after the last key press or scroll
static constexpr unsigned int hideMS = 2000;
static constexpr float gap = 3.0f;
static constexpr float padding = 8.0f;
static constexpr float barHeight = 4.0f;
static constexpr float screenMargin = 10.0f;

static const ImU32 highlightColor = IM_COL32(255, 190, 0, 255);

//Same count as the old game's palette, the last row is transparent
static const glm::vec4 palette[PaintMenu::paletteRows * PaintMenu::paletteColumns] =
{
	{ 1, 1, 1, 1 }, { 0.9, 0.9, 0.9, 1 }, { 0.75, 0.75, 0.75, 1 }, { 0.5, 0.5, 0.5, 1 }, { 0.3, 0.3, 0.3, 1 }, { 0.15, 0.15, 0.15, 1 }, { 0.05, 0.05, 0.05, 1 }, { 0.6, 0.55, 0.5, 1 },
	{ 0.9, 0, 0, 1 }, { 0.6, 0, 0, 1 }, { 1, 0.4, 0.4, 1 }, { 0.95, 0.45, 0, 1 }, { 1, 0.7, 0.3, 1 }, { 0.9, 0.9, 0, 1 }, { 1, 1, 0.5, 1 }, { 0.75, 0.6, 0.1, 1 },
	{ 0, 0.5, 0.25, 1 }, { 0, 0.75, 0, 1 }, { 0.4, 0.9, 0.4, 1 }, { 0.2, 0.35, 0.1, 1 }, { 0.55, 0.7, 0.2, 1 }, { 0, 0.6, 0.6, 1 }, { 0.4, 0.9, 0.85, 1 }, { 0.1, 0.3, 0.3, 1 },
	{ 0.2, 0, 0.8, 1 }, { 0, 0.3, 0.8, 1 }, { 0.4, 0.6, 1, 1 }, { 0, 0.1, 0.35, 1 }, { 0.5, 0.3, 0.8, 1 }, { 0.75, 0.5, 1, 1 }, { 0.8, 0.1, 0.6, 1 }, { 1, 0.6, 0.85, 1 },
	{ 0.4, 0.25, 0.1, 1 }, { 0.6, 0.4, 0.2, 1 }, { 0.8, 0.65, 0.45, 1 }, { 0.25, 0.15, 0.05, 1 }, { 1, 1, 1, 0.4 }, { 0.6, 0.8, 1, 0.4 }, { 1, 0.3, 0.3, 0.4 }, { 0.3, 1, 0.3, 0.4 }
};

static int wrap(int value, int count)
{
	return (value % count + count) % count;
}

static glm::u8vec4 toBytes(const glm::vec4& color)
{
	return glm::u8vec4(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static ImU32 toImColor(const glm::vec4& color)
{
	glm::u8vec4 bytes = toBytes(color);
	return IM_COL32(bytes.r, bytes.g, bytes.b, bytes.a);
}

//Palette cells and material rows are this tall, so they grow with the UI scale
static float cellSize()
{
	return std::max(18.0f, floor(ImGui::GetFontSize() * 1.6f));
}

static float materialColumnWidth()
{
	float widest = 0;
	for (int a = 0; a < BrickMaterialCount; a++)
		widest = std::max(widest, ImGui::CalcTextSize(brickMaterialNames[a]).x);
	return widest + 12.0f;
}

void PaintMenu::applyPaletteColor()
{
	const glm::vec4& picked = palette[colorRow * paletteColumns + column];
	if (toBytes(picked) == toBytes(color))
		return;

	color = picked;
	changed = true;
}

void PaintMenu::pressNextColumn()
{
	lastUsedMS = SDL_GetTicks();

	if (shown)
	{
		column = (column + 1) % columnCount;
		if (column < paletteColumns)
			applyPaletteColor();
		return;
	}

	//The first press only shows the palette, starting on the current color if it's one of the palette's
	shown = true;
	if (column >= paletteColumns)
		return;

	for (int a = 0; a < paletteRows * paletteColumns; a++)
	{
		if (toBytes(palette[a]) == toBytes(color))
		{
			column = a % paletteColumns;
			colorRow = a / paletteColumns;
			break;
		}
	}
}

void PaintMenu::updatePaintKey(bool pressed)
{
	//The first press only shows the palette, later ones move a column once the key comes back up
	if (pressed)
	{
		if (!shown)
			pressNextColumn();
		else
			advanceOnRelease = true;
		keyHeld = true;
	}

	if (!keyHeld)
		return;

	//Stays up while the key is held
	if (shown)
		lastUsedMS = SDL_GetTicks();

	if (input->isCommandKeydown(OpenPaintMenu))
		return;

	//Scrolling through materials while it was held doesn't move a column, so the color stays
	if (advanceOnRelease && !scrolledWhileHeld)
		pressNextColumn();

	keyHeld = false;
	advanceOnRelease = false;
	scrolledWhileHeld = false;
}

void PaintMenu::putAway()
{
	shown = false;

	//A palette key still held shouldn't move a column or bring it back when it comes up
	keyHeld = false;
	advanceOnRelease = false;
	scrolledWhileHeld = false;
}

bool PaintMenu::scroll(int amount)
{
	//Holding the palette's key, the wheel goes through materials without touching the color
	if (input->isCommandKeydown(OpenPaintMenu))
	{
		shown = true;
		lastUsedMS = SDL_GetTicks();
		if (amount != 0)
		{
			material = wrap(material - amount, BrickMaterialCount);
			changed = true;
			scrolledWhileHeld = true;
		}
		return true;
	}

	if (!shown)
		return false;

	lastUsedMS = SDL_GetTicks();
	if (amount == 0)
		return true;

	//Wheel up moves up the column
	if (column < paletteColumns)
	{
		colorRow = wrap(colorRow - amount, paletteRows);
		applyPaletteColor();
	}
	else
	{
		material = wrap(material - amount, BrickMaterialCount);
		changed = true;
	}

	return true;
}

void PaintMenu::toggleCustomColor()
{
	opened = !opened;
}

glm::u8vec4 PaintMenu::getColor() const
{
	return toBytes(color);
}

bool PaintMenu::takeChange()
{
	bool result = changed;
	changed = false;
	return result;
}

void PaintMenu::save(std::shared_ptr<SettingManager> settings) const
{
	settings->addColor("hotbar/color", color);
	settings->addString("hotbar/material", brickMaterialNames[material]);
}

void PaintMenu::load(std::shared_ptr<SettingManager> settings)
{
	//Defaults to white when it's never been saved
	color = glm::clamp(settings->getColor("hotbar/color"), 0.0f, 1.0f);

	//None when it's never been saved
	material = std::max(findBrickMaterial(settings->getString("hotbar/material")), 0);
}

std::string PaintMenu::hintText() const
{
	std::string paintKey = SDL_GetScancodeName(input->getKeyBind(OpenPaintMenu));
	return "Hold " + paintKey + " + scroll material, " + SDL_GetScancodeName(input->getKeyBind(CustomColor)) + " custom";
}

ImVec2 PaintMenu::paletteSize() const
{
	float cell = cellSize();
	float gridWidth = paletteColumns * cell + (paletteColumns - 1) * gap;
	float gridHeight = paletteRows * cell + (paletteRows - 1) * gap;
	float swatchWidth = ImGui::GetFontSize() + 6.0f;

	float contentWidth = std::max(gridWidth + gap * 3.0f + materialColumnWidth(), swatchWidth + ImGui::CalcTextSize(hintText().c_str()).x);
	return ImVec2(contentWidth + padding * 2.0f, padding + ImGui::GetFontSize() + 6.0f + gridHeight + gap * 2.0f + barHeight + padding);
}

void PaintMenu::renderPalette(ImGuiIO* io)
{
	float cell = cellSize();
	float gridWidth = paletteColumns * cell + (paletteColumns - 1) * gap;
	float gridHeight = paletteRows * cell + (paletteRows - 1) * gap;
	float materialWidth = materialColumnWidth();
	float titleHeight = ImGui::GetFontSize() + 6.0f;
	float swatchWidth = ImGui::GetFontSize() + 6.0f;

	std::string hint = hintText();
	ImVec2 size = paletteSize();
	float contentWidth = size.x - padding * 2.0f;

	ImVec2 min(screenMargin, io->DisplaySize.y - screenMargin - size.y);
	ImVec2 max(min.x + size.x, min.y + size.y);

	//Drawn over every window, but it isn't one, so it never takes the mouse or keyboard
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	draw->AddRectFilled(min, max, IM_COL32(20, 20, 25, 210), 6.0f);

	//The color being built with, then how to use the palette
	float x = min.x + padding;
	float y = min.y + padding;
	float swatch = ImGui::GetFontSize();
	ImGui::RenderColorRectWithAlphaCheckerboard(draw, ImVec2(x, y), ImVec2(x + swatch, y + swatch), toImColor(color), swatch / 2.0f, ImVec2(0, 0), 2.0f);
	draw->AddRect(ImVec2(x, y), ImVec2(x + swatch, y + swatch), IM_COL32(200, 200, 200, 255), 2.0f);
	draw->AddText(ImVec2(x + swatchWidth, y), IM_COL32(200, 200, 200, 255), hint.c_str());
	y += titleHeight;

	for (int row = 0; row < paletteRows; row++)
	{
		for (int col = 0; col < paletteColumns; col++)
		{
			const glm::vec4& entry = palette[row * paletteColumns + col];
			ImVec2 cellMin(x + col * (cell + gap), y + row * (cell + gap));
			ImVec2 cellMax(cellMin.x + cell, cellMin.y + cell);

			ImGui::RenderColorRectWithAlphaCheckerboard(draw, cellMin, cellMax, toImColor(entry), cell / 3.0f, ImVec2(0, 0), 3.0f);

			if (column == col && colorRow == row)
				draw->AddRect(ImVec2(cellMin.x - 1, cellMin.y - 1), ImVec2(cellMax.x + 1, cellMax.y + 1), highlightColor, 3.0f, 0, 3.0f);
			else if (toBytes(entry) == toBytes(color))
				draw->AddRect(cellMin, cellMax, IM_COL32_WHITE, 3.0f, 0, 2.0f);
			else
				draw->AddRect(cellMin, cellMax, IM_COL32(90, 90, 100, 200), 3.0f);
		}
	}

	//Materials, a column that shows as many rows as the palette and follows the current one
	float materialX = x + gridWidth + gap * 3.0f;
	int visible = std::min(paletteRows, (int)BrickMaterialCount);
	int first = std::clamp(material - visible / 2, 0, BrickMaterialCount - visible);
	for (int a = 0; a < visible; a++)
	{
		int index = first + a;
		ImVec2 rowMin(materialX, y + a * (cell + gap));
		ImVec2 rowMax(rowMin.x + materialWidth, rowMin.y + cell);

		//Highlighted like the palette's cursor while it's what the wheel changes
		bool current = index == material;
		bool scrolling = column == paletteColumns || input->isCommandKeydown(OpenPaintMenu);
		draw->AddRectFilled(rowMin, rowMax, current ? IM_COL32(70, 70, 85, 230) : IM_COL32(40, 40, 48, 200), 3.0f);
		if (current)
			draw->AddRect(ImVec2(rowMin.x - 1, rowMin.y - 1), ImVec2(rowMax.x + 1, rowMax.y + 1), scrolling ? highlightColor : IM_COL32_WHITE, 3.0f, 0, scrolling ? 3.0f : 2.0f);

		ImVec2 textSize = ImGui::CalcTextSize(brickMaterialNames[index]);
		draw->AddText(ImVec2(rowMin.x + 6.0f, rowMin.y + (cell - textSize.y) / 2.0f), current ? IM_COL32_WHITE : IM_COL32(170, 170, 170, 255), brickMaterialNames[index]);
	}

	//Little arrows on the column's right edge when there are more materials above or below
	float arrow = std::max(3.0f, cell * 0.18f);
	float arrowX = materialX + materialWidth - arrow - 4.0f;
	if (first > 0)
	{
		float top = y + 4.0f;
		draw->AddTriangleFilled(ImVec2(arrowX, top), ImVec2(arrowX - arrow, top + arrow), ImVec2(arrowX + arrow, top + arrow), IM_COL32(200, 200, 200, 255));
	}
	if (first + visible < BrickMaterialCount)
	{
		float bottom = y + gridHeight - 4.0f;
		draw->AddTriangleFilled(ImVec2(arrowX, bottom), ImVec2(arrowX + arrow, bottom - arrow), ImVec2(arrowX - arrow, bottom - arrow), IM_COL32(200, 200, 200, 255));
	}

	//Drains until the palette hides
	y += gridHeight + gap * 2.0f;
	float left = 1.0f - std::clamp((SDL_GetTicks() - lastUsedMS) / (float)hideMS, 0.0f, 1.0f);
	draw->AddRectFilled(ImVec2(x, y), ImVec2(x + contentWidth, y + barHeight), IM_COL32(60, 60, 70, 200), 2.0f);
	if (left > 0.0f)
		draw->AddRectFilled(ImVec2(x, y), ImVec2(x + contentWidth * left, y + barHeight), highlightColor, 2.0f);
}

void PaintMenu::renderPicker(ImGuiIO* io)
{
	//Beside where the palette shows, so both fit on screen at once, it's too tall to go above it at large UI scales
	ImGui::SetNextWindowPos(ImVec2(screenMargin + paletteSize().x + 6.0f, io->DisplaySize.y - screenMargin), ImGuiCond_Always, ImVec2(0.0f, 1.0f));

	//No keyboard navigation, so the paint keys still work while it's up
	ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
	if (!ImGui::Begin("Custom Color", &opened, flags))
	{
		ImGui::End();
		return;
	}

	//Pulled back onto a small screen, overlapping the palette rather than going off the edge
	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();
	ImVec2 onScreen(std::max(0.0f, std::min(pos.x, io->DisplaySize.x - size.x)), std::max(0.0f, std::min(pos.y, io->DisplaySize.y - size.y)));
	if (onScreen.x != pos.x || onScreen.y != pos.y)
		ImGui::SetWindowPos(onScreen);

	//Only counts as changed once an edit finishes, not every frame of a drag, since changes get saved to file
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
	if (ImGui::ColorPicker4("##customColor", &color[0], ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf))
		pickerEditing = true;

	if (ImGui::Button("Done"))
		opened = false;

	if (pickerEditing && (!ImGui::IsAnyItemActive() || !opened))
	{
		pickerEditing = false;
		changed = true;
	}

	ImGui::End();
}

void PaintMenu::render(ImGuiIO* io)
{
	//Nothing of the game's own HUD while the appearance editor is up, the custom color picker included
	if (userInterface->hideGameHud)
		return;

	if (shown && SDL_GetTicks() - lastUsedMS >= hideMS)
		shown = false;

	if (shown)
		renderPalette(io);

	if (opened)
		renderPicker(io);
	else if (pickerEditing)
	{
		//Closed with Escape mid edit
		pickerEditing = false;
		changed = true;
	}
}

void PaintMenu::init()
{
	initalized = true;
}

void PaintMenu::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
}

PaintMenu::PaintMenu(std::shared_ptr<InputMap> _input) : input(_input)
{
	name = "Paint Menu";
}
