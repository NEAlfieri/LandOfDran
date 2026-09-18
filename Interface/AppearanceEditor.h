#pragma once

#include "../LandOfDran.h"

#include "UserInterface.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/RenderTarget.h"
#include "../GameLoop/PlayerAppearance.h"

/*
	Picks how your player looks, like the old game's avatar picker: a color for each part of the player model, a face from Assets/faces,
	a shirt from Assets/shirts, and a hat from Assets/brickhead/parts, which is colored like any other part
	Saved under appearance/ in settings and sent to servers as you join them, see PlayerAppearance
	Opened from the server browser, LoopClient draws the model with renderPreview while it's open
*/
class AppearanceEditor : public Window
{
	friend class Window;
	friend class UserInterface;

	std::shared_ptr<SettingManager> settings;
	std::shared_ptr<TextureManager> textures;

	//File names of the faces in Assets/faces, in decal array order, see ClientProgramData::faceNames
	const std::vector<std::string>* faceNames = nullptr;

	//The same images as plain textures for the face buttons, parallel to faceNames
	std::vector<Texture*> faceIcons;

	//File names of the shirts in Assets/shirts, see ClientProgramData::shirtNames, and their buttons' textures
	const std::vector<std::string>* shirtNames = nullptr;
	std::vector<Texture*> shirtIcons;

	//File names of the hats in PlayerAppearance::partsFolder, see ClientProgramData::hatNames
	const std::vector<std::string>* hatNames = nullptr;

	//Its own copies of the hat models, parallel to hatNames and loaded with the player model, nullptr for one that failed, and one instance of each
	std::vector<Model*> hatModels;
	std::vector<ModelInstance*> hatInstances;

	//The hat's index in colors and everywhere else a part goes, one past the player model's last mesh, -1 without a model
	int hatPart = -1;

	//File name in PlayerAppearance::partsFolder, empty for no hat, and its size from the slider, see PlayerAppearance::hatScale
	std::string hat = "";
	float hatScale = 1.0f;

	//Its own copy of the player model, loaded the first time the editor opens, nullptr if that failed
	Model* model = nullptr;
	ModelInstance* instance = nullptr;
	bool modelLoadAttempted = false;

	//Parts worth clicking on, leaving out things like the collision box
	std::vector<int> editableMeshes;

	//Where the face goes, and the head, which is colored along with it, -1 without one
	int faceMesh = -1;
	int headMesh = -1;

	//Where the shirt goes, -1 without one
	int shirtMesh = -1;

	//Per mesh index, then the hat at hatPart, alpha 0 shows the model's own look
	std::vector<glm::vec4> colors;

	//File name in Assets/faces, empty for no face
	std::string face = "";

	//File name in Assets/shirts, empty for no shirt
	std::string shirt = "";

	//Whether it was open last frame, so opening it again starts over from what's saved
	bool wasOpen = false;

	//How the model is turned by dragging, and how far away the camera is compared to where the whole model just fits
	float yaw = 0;
	float pitch = 0;
	float zoom = 1;

	//Part under the mouse as of the last renderPreview, -1 for none
	int hoveredMesh = -1;

	//SDL_BUTTON_LEFT or SDL_BUTTON_RIGHT while it's held after being pressed over the model instead of a window, 0 for neither
	int heldButton = 0;

	//How far the mouse has moved since, a little counts as a click and more as turning the model
	float heldMoved = 0;

	//Part whose color window is open, -1 for none, and how it, the face, and the shirt were before, for Revert
	int pickingColorFor = -1;
	glm::vec4 colorBeforePicking = glm::vec4(0);
	std::string faceBeforePicking = "";
	std::string shirtBeforePicking = "";
	std::string hatBeforePicking = "";
	float hatScaleBeforePicking = 1.0f;
	bool colorWindowAppearing = false;
	ImVec2 colorWindowPosition = ImVec2(0, 0);

	//Right clicking a part picks up its color, then left clicking other parts paints them with it
	bool painting = false;
	glm::vec3 paintColor = glm::vec3(1);

	//The part under the mouse is drawn into this single pixel, see renderPreview
	std::shared_ptr<RenderTarget> pickingTarget = nullptr;

	//How far from the left of the screen the panel reached last frame, the model is centered in the rest
	float panelWidth = 0;

	bool saved = false;

	//Loads what's needed the frame it opens and goes back to what's saved
	void prepare();

	void loadModel();

	//Colors and face from settings
	void loadSaved();

	void save();

	void clickMesh(int button, int meshIdx);

	//Colors a part, along with the head or face if it's the other one, so the face never stands out from the head
	void setColor(int meshIdx, const glm::vec4& color);

	//Index of the chosen hat in hatNames, -1 for none
	int hatIndex() const;

	//Readable name of a hat from its file name, like Top Hat from top_hat.txt
	static std::string hatLabel(const std::string& fileName);

	//Whether two parts are always the same color, see setColor
	bool sameColor(int meshA, int meshB) const;

	//Readable name of a part, like Left Shoulder
	std::string partName(int meshIdx) const;

	//Decal array layers of the chosen face and shirt, -1 for none
	int faceDecal() const;
	int shirtDecal() const;

	//Opens the color window for a part, remembering how it, the face, and the shirt were for Revert
	void openColorWindow(int meshIdx, ImVec2 position);

	//The color picker for pickingColorFor, with face buttons if it's the face or head, and shirt buttons if it's the torso
	void renderColorWindow();

	//A grid of buttons for picking one of names (or None) into chosen, icons parallel to names
	void renderDecalChoices(const char* label, const std::vector<std::string>& names, const std::vector<Texture*>& icons, std::string& chosen);

	//A row of buttons naming each hat (and None) for picking into hat, and the slider for its size
	void renderHatChoices();

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;

	AppearanceEditor(std::shared_ptr<SettingManager> _settings, std::shared_ptr<TextureManager> _textures, const std::vector<std::string>* _faceNames, const std::vector<std::string>* _shirtNames, const std::vector<std::string>* _hatNames);

	public:

	//What was last saved in the editor, from appearance/ in settings, for sending to servers
	static PlayerAppearance loadAppearance(std::shared_ptr<SettingManager> settings);

	//Draws the model over a plain background and finds the part under the mouse, call before the GUI each frame it's open
	void renderPreview(std::shared_ptr<ShaderManager> shaders, int screenWidth, int screenHeight, float deltaT);

	//Closes the color window (reverting it) or stops painting, returns false if neither was going so Escape can close the editor
	bool handleEscape();

	//Save was pressed since the last call
	bool takeSaved();

	//Frees the model and picking target, call while the OpenGL context still exists
	void releaseGraphics();

	~AppearanceEditor();
};
