#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"
#include "../Graphics/Texture.h"

/*
	The items you carry, in a column on the right of the screen
	Its key slides it out, which puts the picked slot's item in your player's hand, and pressing it again puts it away
	While it's out the mouse wheel picks another slot
	Never counts as an open window, it only draws over the game
*/
class ItemHotbar : public Window
{
	public:

	//Same as inventorySize in SimObjects/Item.h
	static constexpr int slotCount = 5;

	private:

	struct Slot
	{
		bool filled = false;
		std::string name = "";
		//Not owned, nullptr to show the name instead
		Texture* icon = nullptr;
		//The icon is a picture the game drew of the item's model rather than an image file, so its rows
		//run bottom to top and it's read upside down, see ItemIconRenderer
		bool iconIsRender = false;
	};

	Slot slots[slotCount];

	bool up = false;
	int selected = 0;
	bool changed = false;

	//Slide animation state, see render
	bool drawnUp = false;
	unsigned int slideStartMS = 0;

	public:

	//The item bar's key, slides it out or back in
	void toggle();

	void putAway();

	bool isUp() const { return up; }

	//0 to slotCount - 1, kept while it's put away
	int getSelected() const { return selected; }

	//Mouse wheel, moves the picked slot up by amount, wrapping around
	//Returns false if the bar isn't out so the scroll can go to something else
	bool scroll(int amount);

	//What a slot shows, set each frame from the items the server says we carry
	void setSlot(int slot, bool filled, const std::string& name, Texture* icon, bool iconIsRender = false);

	//True once after the bar comes out or goes away, or the picked slot changes
	bool takeChange();

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	ItemHotbar();
};
