#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"
#include "../Bricks/Brick.h"

/*
	The building color and material, picked from a small palette in the bottom left corner
	The palette only draws over the game and never takes the mouse: its key shows it and moves one column along each press,
	the mouse wheel moves rows, and it hides itself once neither has been used for a moment
	The last column is the material list
	The custom color picker is the one part that's a real window, it counts as open and takes the mouse while it's up
*/
class PaintMenu : public Window
{
	public:

	static constexpr int paletteColumns = 8;
	static constexpr int paletteRows = 5;
	//Palette columns, then the material column
	static constexpr int columnCount = paletteColumns + 1;

	private:

	std::shared_ptr<InputMap> input = nullptr;

	glm::vec4 color = glm::vec4(1, 1, 1, 1);

	//A BrickMaterial
	int material = BrickMaterial_None;

	bool shown = false;
	//SDL_GetTicks() of the last key press or scroll, the palette hides once hideMS have gone by
	unsigned int lastUsedMS = 0;

	//0 to columnCount - 1, and the row within the palette, kept while in the material column
	int column = 0;
	int colorRow = 0;

	//A drag in the custom color picker that hasn't counted as a change yet
	bool pickerEditing = false;

	bool changed = false;

	//The palette's key is down, see updatePaintKey: a column move waits for it to come back up, and doesn't happen if the wheel changed the material meanwhile
	bool keyHeld = false;
	bool advanceOnRelease = false;
	bool scrolledWhileHeld = false;

	void applyPaletteColor();

	//The palette's top line, with the keys currently bound
	std::string hintText() const;
	//The palette's full size in pixels, at the current UI scale
	ImVec2 paletteSize() const;

	void renderPalette(ImGuiIO* io);
	void renderPicker(ImGuiIO* io);

	public:

	//The palette's key, shows the palette or moves one column right, wrapping around
	void pressNextColumn();

	/*
		Call every frame with whether the palette's key was just pressed
		The first press shows the palette, a later tap moves one column once the key comes back up, and the palette stays up while it's held
	*/
	void updatePaintKey(bool pressed);

	//Mouse wheel, moves amount rows up in the current column, wrapping around, only while the palette shows
	//While the palette's key is held it moves through materials instead, showing the palette, without changing the color
	//Returns false if the palette isn't showing and the key isn't held, so the scroll can go to something else
	bool scroll(int amount);

	//Opens or closes the custom color picker
	void toggleCustomColor();

	//Hides the palette, for the item bar or brick bar taking over, see LoopClient::handleInput
	void putAway();

	bool isPaletteShown() const { return shown; }

	glm::u8vec4 getColor() const;
	unsigned char getMaterial() const { return (unsigned char)material; }

	//True once after the building color or material changes
	bool takeChange();

	//The building color and material are kept as hotbar/color and hotbar/material
	void save(std::shared_ptr<SettingManager> settings) const;
	void load(std::shared_ptr<SettingManager> settings);

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	PaintMenu(std::shared_ptr<InputMap> _input);
};
