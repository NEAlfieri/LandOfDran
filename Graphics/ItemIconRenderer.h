#pragma once

#include "../LandOfDran.h"
#include "RenderTarget.h"
#include "ShaderSpecification.h"

class Model;
class ModelInstance;

/*
	Draws a little picture of the model of each item you're carrying into its own texture, which the item bar
	shows in place of the flat icon an item type came with, see ItemHotbar and graphics/itemicons3d.

	Nothing here goes near the world: a slot's model is drawn on its own, non instanced, with the model matrix
	in BasicUniforms rather than one of the instances in the model's buffers, so an item sitting hidden in
	someone's inventory can still be drawn here (see Mesh::renderOnce). It is still the live ModelInstance's
	mesh transforms though, so whatever the item is animating in hand animates in its picture too.

	A slot is only drawn again when what it's showing changes, so the four items you aren't holding cost
	nothing per frame and only the picked one, which turns on the spot, is redrawn.
*/
class ItemIconRenderer
{
	//Each slot's picture is this many pixels square, enough for the 64 pixel slots to stay sharp on a high dpi screen
	static constexpr int resolution = 128;

	//How far the model is tipped toward the camera, radians, so a flat item isn't seen edge on
	static constexpr float tilt = 0.35f;

	struct Slot
	{
		std::shared_ptr<RenderTarget> target = nullptr;
		//What was last drawn into it, so a slot showing the same thing isn't drawn again
		const Model* model = nullptr;
		float angle = 0;
		bool drawn = false;
	};

	std::vector<Slot> slots;
	std::shared_ptr<TextureManager> textures;

	//Sets up the shader, lighting and GL state the pictures are drawn with, and puts it all back
	void begin(std::shared_ptr<ShaderManager> shaders);
	void end(std::shared_ptr<ShaderManager> shaders, int screenWidth, int screenHeight);

	//Everything begin took a copy of to give back in end
	EnvironmentUniforms previousEnvironment;
	CameraUniforms previousCamera;
	GLint previousPointLightCount = 0;

	public:

	//One item bar slot's worth of what to draw, an empty slot or one whose item has no model left as it is
	struct Request
	{
		Model* model = nullptr;
		//Which instance's mesh transforms to draw it in, nullptr to draw it in its rest pose
		const ModelInstance* instance = nullptr;
		//How far the model is turned around its own up axis, radians
		float angle = 0;
	};

	//Draws whichever of the slots have changed since the last call. Leaves the screen selected and its viewport set
	void render(std::shared_ptr<ShaderManager> shaders, const std::vector<Request>& requests, int screenWidth, int screenHeight);

	//The picture to show for a slot, nullptr while nothing has been drawn into it
	Texture* getIcon(int slot) const;

	//Frees every slot's texture, for when 3d icons are turned off
	void clear();

	ItemIconRenderer(std::shared_ptr<TextureManager> _textures) : textures(_textures) {}
};
