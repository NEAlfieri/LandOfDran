#include "AppearanceEditor.h"

//The player model serverstart.lua gives players, and its scale there
static constexpr const char* playerModelPath = "Assets/brickhead/brickhead.txt";
static constexpr float playerModelScale = 0.02f;

//Pixels the mouse can move while held and still count as a click instead of turning the model
static constexpr float dragThreshold = 4.0f;

//Radians the model turns per pixel dragged
static constexpr float turnPerPixel = 0.01f;

//Furthest the model tips toward or away from the camera
static constexpr float maxPitch = 1.2f;

//Vertical field of view of the editor's camera, in degrees
static constexpr float fieldOfView = 35.0f;

//How the model starts turned, so its face points at the camera
static constexpr float startYaw = 3.14159265f;

static const glm::vec3 backgroundColor = glm::vec3(0.16f, 0.18f, 0.22f);

//Index of a file name in a list of faces, shirts, or hats, -1 if it isn't there
static int findName(const std::vector<std::string>* names, const std::string& name)
{
	if (!names || name.empty())
		return -1;

	auto found = std::find(names->begin(), names->end(), name);
	return found == names->end() ? -1 : (int)(found - names->begin());
}

AppearanceEditor::AppearanceEditor(std::shared_ptr<SettingManager> _settings, std::shared_ptr<TextureManager> _textures, const std::vector<std::string>* _faceNames, const std::vector<std::string>* _shirtNames, const std::vector<std::string>* _hatNames)
	: settings(_settings), textures(_textures), faceNames(_faceNames), shirtNames(_shirtNames), hatNames(_hatNames)
{
	name = "Appearance Editor";
}

AppearanceEditor::~AppearanceEditor()
{
	for (std::vector<Texture*>* icons : { &faceIcons, &shirtIcons })
	{
		for (Texture* icon : *icons)
		{
			if (icon)
				icon->markForCleanup();
		}
	}

	releaseGraphics();
}

void AppearanceEditor::releaseGraphics()
{
	pickingTarget.reset();

	for (ModelInstance* hatInstance : hatInstances)
		delete hatInstance;
	hatInstances.clear();
	for (Model* hatModel : hatModels)
		delete hatModel;
	hatModels.clear();

	delete instance;
	instance = nullptr;

	delete model;
	model = nullptr;
}

void AppearanceEditor::init()
{
	initalized = true;
}

PlayerAppearance AppearanceEditor::loadAppearance(std::shared_ptr<SettingManager> settings)
{
	PlayerAppearance appearance;
	appearance.face = settings->getString("appearance/face");
	appearance.shirt = settings->getString("appearance/shirt");
	appearance.hat = settings->getString("appearance/hat");

	//0 when it was never saved, which getFloat gives for a missing setting
	float hatScale = settings->getFloat("appearance/hatscale");
	if (hatScale > 0)
		appearance.hatScale = glm::clamp(hatScale, PlayerAppearance::minHatScale, PlayerAppearance::maxHatScale);

	//Every color under appearance/colors, which meshes a player model has isn't known without loading it, and the hat's if it has one
	settings->startPreferenceBindingSearch();
	std::string path;
	while (PreferencePair* pref = settings->nextPreferenceBinding(path))
	{
		if (path == "appearance/colors" && pref->type == PreferenceColor)
			appearance.colors.emplace_back(pref->name, glm::clamp(glm::vec3(pref->color[0], pref->color[1], pref->color[2]), 0.0f, 1.0f));
		else if (path == "appearance" && pref->type == PreferenceColor && pref->name == "hatcolor")
			appearance.hatColor = glm::vec4(glm::clamp(glm::vec3(pref->color[0], pref->color[1], pref->color[2]), 0.0f, 1.0f), 1.0f);
	}

	return appearance;
}

void AppearanceEditor::loadModel()
{
	scope("AppearanceEditor::loadModel");

	modelLoadAttempted = true;

	model = new Model(playerModelPath, textures, glm::vec3(playerModelScale));
	if (!model->isValid() || model->getNumMeshes() < 1)
	{
		error("Couldn't load the player model " + std::string(playerModelPath) + " for the appearance editor");
		delete model;
		model = nullptr;
		return;
	}

	instance = new ModelInstance(model);

	for (int a = 0; a < model->getNumMeshes(); a++)
	{
		if (model->isMeshDrawn(a))
			editableMeshes.push_back(a);
	}

	faceMesh = model->getFaceMeshIdx();
	headMesh = model->getMeshIdxIgnoringCase("Head");
	shirtMesh = model->getShirtMeshIdx();

	//The hat is one more part after the model's meshes
	hatPart = model->getNumMeshes();
	colors.assign(model->getNumMeshes() + 1, glm::vec4(0));

	//Its own copies of the hats, the game's have every player's hat as an instance and would draw them all here
	if (hatNames)
	{
		for (const std::string& hatName : *hatNames)
		{
			Model* hatModel = new Model(PlayerAppearance::partsFolder + hatName, textures, glm::vec3(1.0f));
			if (!hatModel->isValid() || hatModel->getNumMeshes() < 1)
			{
				error("Couldn't load the hat " + hatName + " for the appearance editor");
				delete hatModel;
				hatModel = nullptr;
			}

			hatModels.push_back(hatModel);
			hatInstances.push_back(hatModel ? new ModelInstance(hatModel) : nullptr);
		}
	}
}

void AppearanceEditor::loadSaved()
{
	PlayerAppearance appearance = loadAppearance(settings);
	face = appearance.face;
	shirt = appearance.shirt;
	hat = appearance.hat;
	hatScale = appearance.hatScale;

	std::fill(colors.begin(), colors.end(), glm::vec4(0));
	if (model)
	{
		for (const auto& [meshName, color] : appearance.colors)
		{
			int meshIdx = model->getMeshIdxIgnoringCase(meshName);
			if (meshIdx != -1)
				colors[meshIdx] = glm::vec4(color, 1.0f);
		}

		colors[hatPart] = appearance.hatColor;
	}
}

void AppearanceEditor::prepare()
{
	if (wasOpen)
		return;
	wasOpen = true;

	if (!modelLoadAttempted)
		loadModel();

	auto loadIcons = [&](const std::vector<std::string>* names, const std::string& folder, std::vector<Texture*>& icons)
	{
		if (!names)
			return;

		for (size_t a = icons.size(); a < names->size(); a++)
		{
			Texture* icon = textures->createTexture(folder + (*names)[a]);
			icons.push_back(icon && icon->isValid() ? icon : nullptr);
		}
	};
	loadIcons(faceNames, "Assets/faces/", faceIcons);
	loadIcons(shirtNames, "Assets/shirts/", shirtIcons);

	loadSaved();

	yaw = 0;
	pitch = 0;
	zoom = 1;
	hoveredMesh = -1;
	heldButton = 0;
	pickingColorFor = -1;
	painting = false;
}

void AppearanceEditor::save()
{
	settings->remove("appearance");
	settings->addString("appearance/face", face);
	settings->addString("appearance/shirt", shirt);
	settings->addString("appearance/hat", hat);
	settings->addFloat("appearance/hatscale", hatScale, true, "", PlayerAppearance::minHatScale, PlayerAppearance::maxHatScale);

	if (model)
	{
		for (int meshIdx : editableMeshes)
		{
			if (colors[meshIdx].a > 0)
				settings->addColor("appearance/colors/" + lowercase(model->getMeshName(meshIdx)), glm::vec4(glm::vec3(colors[meshIdx]), 1.0f));
		}

		if (colors[hatPart].a > 0)
			settings->addColor("appearance/hatcolor", glm::vec4(glm::vec3(colors[hatPart]), 1.0f));
	}

	settings->exportToFile("Config/settings.txt");
	saved = true;
}

bool AppearanceEditor::takeSaved()
{
	bool result = saved;
	saved = false;
	return result;
}

bool AppearanceEditor::sameColor(int meshA, int meshB) const
{
	if (meshA == meshB)
		return true;

	if (faceMesh == -1 || headMesh == -1)
		return false;

	return (meshA == faceMesh && meshB == headMesh) || (meshA == headMesh && meshB == faceMesh);
}

void AppearanceEditor::setColor(int meshIdx, const glm::vec4& color)
{
	//The hat isn't one of the model's meshes
	if (meshIdx == hatPart)
	{
		colors[hatPart] = color;
		return;
	}

	for (int other : editableMeshes)
	{
		if (sameColor(meshIdx, other))
			colors[other] = color;
	}
}

int AppearanceEditor::hatIndex() const
{
	return findName(hatNames, hat);
}

std::string AppearanceEditor::hatLabel(const std::string& fileName)
{
	std::string result;
	bool wordStart = true;
	for (char c : fileName.substr(0, fileName.find_last_of('.')))
	{
		if (c == '_' || c == '-' || c == ' ')
		{
			result += ' ';
			wordStart = true;
			continue;
		}

		result += wordStart ? (char)std::toupper((unsigned char)c) : c;
		wordStart = false;
	}

	return result;
}

int AppearanceEditor::faceDecal() const
{
	return findName(faceNames, face);
}

int AppearanceEditor::shirtDecal() const
{
	//Shirts' layers come after every face, see ClientProgramData::getDecal
	int index = findName(shirtNames, shirt);
	if (index == -1)
		return -1;
	return (faceNames ? (int)faceNames->size() : 0) + index;
}

std::string AppearanceEditor::partName(int meshIdx) const
{
	if (meshIdx == hatPart)
		return "Hat";

	if (meshIdx == faceMesh && faceMesh != headMesh)
		return "Face";

	const std::string& meshName = model->getMeshName(meshIdx);
	std::string result;
	for (size_t a = 0; a < meshName.length(); a++)
	{
		char c = meshName[a];
		if (c == '_')
		{
			result += ' ';
			continue;
		}

		if (a > 0 && std::isupper((unsigned char)c) && std::islower((unsigned char)meshName[a - 1]))
			result += ' ';
		result += c;
	}

	return result;
}

void AppearanceEditor::clickMesh(int button, int meshIdx)
{
	if (button == SDL_BUTTON_RIGHT)
	{
		//Right clicking nothing puts the color down
		if (meshIdx == -1)
		{
			painting = false;
			return;
		}

		painting = true;
		paintColor = colors[meshIdx].a > 0 ? glm::vec3(colors[meshIdx]) : glm::vec3(1.0f);
		pickingColorFor = -1;
		return;
	}

	//Left clicking nothing closes the color window, keeping the color
	if (meshIdx == -1)
	{
		pickingColorFor = -1;
		return;
	}

	if (painting)
	{
		setColor(meshIdx, glm::vec4(paintColor, 1.0f));
		return;
	}

	openColorWindow(meshIdx, ImVec2(ImGui::GetIO().MousePos.x + 24.0f, ImGui::GetIO().MousePos.y - 40.0f));
}

void AppearanceEditor::openColorWindow(int meshIdx, ImVec2 position)
{
	pickingColorFor = meshIdx;
	colorBeforePicking = colors[meshIdx];
	faceBeforePicking = face;
	shirtBeforePicking = shirt;
	hatBeforePicking = hat;
	hatScaleBeforePicking = hatScale;
	colorWindowAppearing = true;
	colorWindowPosition = position;
}

bool AppearanceEditor::handleEscape()
{
	if (pickingColorFor != -1)
	{
		setColor(pickingColorFor, colorBeforePicking);
		face = faceBeforePicking;
		shirt = shirtBeforePicking;
		hat = hatBeforePicking;
		hatScale = hatScaleBeforePicking;
		pickingColorFor = -1;
		return true;
	}

	if (painting)
	{
		painting = false;
		return true;
	}

	return false;
}

void AppearanceEditor::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
	if (!opened)
	{
		heldButton = 0;
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (e.type == SDL_MOUSEBUTTONDOWN && (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT))
	{
		if (io.WantCaptureMouse || heldButton != 0)
			return;

		heldButton = e.button.button;
		heldMoved = 0;
	}
	else if (e.type == SDL_MOUSEMOTION && heldButton != 0)
	{
		heldMoved += std::abs((float)e.motion.xrel) + std::abs((float)e.motion.yrel);
		if (heldMoved > dragThreshold)
		{
			yaw += e.motion.xrel * turnPerPixel;
			pitch = std::clamp(pitch + e.motion.yrel * turnPerPixel, -maxPitch, maxPitch);
		}
	}
	else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == heldButton)
	{
		if (heldMoved <= dragThreshold)
			clickMesh(heldButton, hoveredMesh);
		heldButton = 0;
	}
	else if (e.type == SDL_MOUSEWHEEL && !io.WantCaptureMouse)
	{
		float amount = (float)(e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y);
		zoom = std::clamp(zoom * std::pow(0.88f, amount), 0.35f, 2.0f);
	}
}

void AppearanceEditor::renderColorWindow()
{
	if (pickingColorFor < 0 || !model)
		return;

	//Top right corner, out of the way of the model and never off the edge of the screen
	if (colorWindowAppearing)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - 10.0f, viewport->Pos.y + 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		colorWindowAppearing = false;
	}

	bool windowOpen = true;
	std::string title = partName(pickingColorFor) + "###AppearancePartColor";
	if (ImGui::Begin(title.c_str(), &windowOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
	{
		//Parts showing the model's own look start the picker at white
		glm::vec4 current = colors[pickingColorFor];
		float rgb[3] = { 1.0f, 1.0f, 1.0f };
		if (current.a > 0)
		{
			rgb[0] = current.r;
			rgb[1] = current.g;
			rgb[2] = current.b;
		}

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
		if (ImGui::ColorPicker3("##partColor", rgb, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar))
			setColor(pickingColorFor, glm::vec4(rgb[0], rgb[1], rgb[2], 1.0f));

		if (ImGui::Button("Done"))
			windowOpen = false;
		ImGui::SameLine();
		if (ImGui::Button("Revert"))
		{
			setColor(pickingColorFor, colorBeforePicking);
			face = faceBeforePicking;
			shirt = shirtBeforePicking;
			hat = hatBeforePicking;
			hatScale = hatScaleBeforePicking;
		}
		ImGui::SameLine();
		if (ImGui::Button("Default"))
			setColor(pickingColorFor, glm::vec4(0));
		ImGui::SetItemTooltip("%s", "Go back to how the player model looks without a color");

		if (faceMesh != -1 && sameColor(pickingColorFor, faceMesh) && faceNames && !faceNames->empty())
			renderDecalChoices("Face", *faceNames, faceIcons, face);

		if (shirtMesh != -1 && pickingColorFor == shirtMesh && shirtNames && !shirtNames->empty())
			renderDecalChoices("Shirt", *shirtNames, shirtIcons, shirt);

		//The hat's own window offers the others, and the head's offers them too since there's no hat to click without one
		bool headPicked = headMesh != -1 && sameColor(pickingColorFor, headMesh);
		if (hatPart != -1 && (pickingColorFor == hatPart || headPicked) && hatNames && !hatNames->empty())
			renderHatChoices();
	}
	ImGui::End();

	if (!windowOpen)
		pickingColorFor = -1;
}

void AppearanceEditor::renderDecalChoices(const char* label, const std::vector<std::string>& names, const std::vector<Texture*>& icons, std::string& chosen)
{
	ImGui::Separator();
	ImGui::TextUnformatted(label);

	float buttonSize = ImGui::GetFontSize() * 2.6f;
	ImGuiStyle& style = ImGui::GetStyle();
	float cellWidth = buttonSize + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
	int columns = std::max(1, (int)((ImGui::GetFontSize() * 14.0f) / cellWidth));

	ImVec4 chosenColor = style.Colors[ImGuiCol_ButtonActive];

	//Scrolls once there are more choices than fit in a third of the screen
	int rows = (int)(names.size() + 1 + columns - 1) / columns;
	float rowHeight = buttonSize + style.FramePadding.y * 2.0f + style.ItemSpacing.y;
	float gridHeight = std::min(rows * rowHeight, ImGui::GetMainViewport()->Size.y * 0.33f);

	ImGui::PushID(label);
	ImGui::BeginChild("##choices", ImVec2(columns * cellWidth + style.ScrollbarSize, gridHeight));

	bool noneChosen = chosen.empty();
	if (noneChosen)
		ImGui::PushStyleColor(ImGuiCol_Button, chosenColor);
	if (ImGui::Button("None", ImVec2(buttonSize + style.FramePadding.x * 2.0f, buttonSize + style.FramePadding.y * 2.0f)))
		chosen = "";
	if (noneChosen)
		ImGui::PopStyleColor();

	for (size_t a = 0; a < names.size(); a++)
	{
		if ((a + 1) % columns != 0)
			ImGui::SameLine();

		ImGui::PushID((int)a);
		bool isChosen = names[a] == chosen;
		if (isChosen)
			ImGui::PushStyleColor(ImGuiCol_Button, chosenColor);

		bool clicked;
		if (a < icons.size() && icons[a])
			clicked = ImGui::ImageButton("##choice", (ImTextureID)(intptr_t)icons[a]->getHandle(), ImVec2(buttonSize, buttonSize));
		else
			clicked = ImGui::Button("?", ImVec2(buttonSize + style.FramePadding.x * 2.0f, buttonSize + style.FramePadding.y * 2.0f));
		ImGui::SetItemTooltip("%s", names[a].c_str());

		if (isChosen)
			ImGui::PopStyleColor();
		ImGui::PopID();

		if (clicked)
			chosen = names[a];
	}

	ImGui::EndChild();
	ImGui::PopID();
}

void AppearanceEditor::renderHatChoices()
{
	ImGui::Separator();
	ImGui::TextUnformatted("Hat");

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4 chosenColor = style.Colors[ImGuiCol_ButtonActive];

	//Buttons go across as far as the color picker is wide, then wrap
	float rowWidth = ImGui::GetFontSize() * 14.0f;
	float rowLeft = ImGui::GetCursorScreenPos().x;

	auto choice = [&](const std::string& label, const std::string& value, bool first)
	{
		float width = ImGui::CalcTextSize(label.c_str()).x + style.FramePadding.x * 2.0f;
		if (!first && ImGui::GetItemRectMax().x + style.ItemSpacing.x + width - rowLeft <= rowWidth)
			ImGui::SameLine();

		bool isChosen = hat == value;
		if (isChosen)
			ImGui::PushStyleColor(ImGuiCol_Button, chosenColor);
		if (ImGui::Button(label.c_str()))
			hat = value;
		if (isChosen)
			ImGui::PopStyleColor();
	};

	ImGui::PushID("hats");
	choice("None", "", true);
	for (size_t a = 0; a < hatNames->size(); a++)
	{
		ImGui::PushID((int)a);
		choice(hatLabel((*hatNames)[a]), (*hatNames)[a], false);
		ImGui::PopID();
	}

	//Its size, which is kept when they pick another hat
	if (!hat.empty())
	{
		float percent = hatScale * 100.0f;
		ImGui::SetNextItemWidth(rowWidth);
		if (ImGui::SliderFloat("##hatSize", &percent, PlayerAppearance::minHatScale * 100.0f, PlayerAppearance::maxHatScale * 100.0f, "Size: %.0f%%", ImGuiSliderFlags_AlwaysClamp))
			hatScale = percent / 100.0f;
		ImGui::SetItemTooltip("%s", "How big the hat is, it stays sitting on top of the head");
	}
	ImGui::PopID();
}

void AppearanceEditor::render(ImGuiIO* io)
{
	if (!opened)
	{
		wasOpen = false;
		return;
	}

	prepare();

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 10.0f, viewport->Pos.y + 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 22.0f, 0.0f), ImGuiCond_Always);
	if (ImGui::Begin("Player Appearance", &opened, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		panelWidth = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - viewport->Pos.x;

		if (!model)
			ImGui::TextWrapped("%s", "Couldn't load the player model, see the error log.");

		ImGui::TextWrapped("%s", "Drag to spin your player around, scroll to zoom.");
		ImGui::TextWrapped("%s", "Left click a part to set its color.");
		ImGui::TextWrapped("%s", "Right click a part to pick up its color, then left click other parts to paint it on.");
		ImGui::TextWrapped("%s", "Click the face or torso to pick a different face or shirt, and the head or hat to pick a hat.");

		if (painting)
		{
			ImGui::Separator();
			ImGui::ColorButton("##paintColor", ImVec4(paintColor.r, paintColor.g, paintColor.b, 1.0f), ImGuiColorEditFlags_NoTooltip);
			ImGui::SameLine();
			ImGui::TextWrapped("%s", "Painting, right click empty space to stop");
			if (ImGui::Button("Stop painting"))
				painting = false;
		}

		ImGui::Separator();

		float iconSize = ImGui::GetFontSize() * 2.6f;
		auto showChoice = [&](const char* label, const std::string& chosen, int index, const std::vector<Texture*>& icons)
		{
			if (index != -1 && index < (int)icons.size() && icons[index])
			{
				ImGui::Image((ImTextureID)(intptr_t)icons[index]->getHandle(), ImVec2(iconSize, iconSize));
				ImGui::SameLine();
			}
			ImGui::Text("%s: %s", label, chosen.empty() ? "None" : chosen.c_str());
		};
		ImVec2 besidePanel(viewport->Pos.x + panelWidth + 10.0f, viewport->Pos.y + 10.0f);

		showChoice("Face", face, findName(faceNames, face), faceIcons);
		if (model && faceMesh != -1 && ImGui::Button("Change face"))
			openColorWindow(faceMesh, besidePanel);

		showChoice("Shirt", shirt, findName(shirtNames, shirt), shirtIcons);
		if (model && shirtMesh != -1 && ImGui::Button("Change shirt"))
			openColorWindow(shirtMesh, besidePanel);

		ImGui::Text("Hat: %s", hat.empty() ? "None" : hatLabel(hat).c_str());
		if (model && hatPart != -1 && hatNames && !hatNames->empty() && ImGui::Button("Change hat"))
			openColorWindow(hatPart, besidePanel);

		ImGui::Separator();

		if (ImGui::Button("Save"))
		{
			save();
			opened = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			opened = false;
	}
	ImGui::End();

	renderColorWindow();

	//The picked up color follows the mouse while painting
	if (painting && !io->WantCaptureMouse)
	{
		ImDrawList* draw = ImGui::GetForegroundDrawList();
		ImVec2 min(io->MousePos.x + 14.0f, io->MousePos.y + 14.0f);
		ImVec2 max(min.x + 18.0f, min.y + 18.0f);
		glm::ivec3 bytes = glm::ivec3(glm::clamp(paintColor, 0.0f, 1.0f) * 255.0f + 0.5f);
		draw->AddRectFilled(min, max, IM_COL32(bytes.r, bytes.g, bytes.b, 255), 3.0f);
		draw->AddRect(min, max, IM_COL32(255, 255, 255, 255), 3.0f);
	}
}

void AppearanceEditor::renderPreview(std::shared_ptr<ShaderManager> shaders, int screenWidth, int screenHeight, float deltaT)
{
	prepare();

	glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	hoveredMesh = -1;
	if (!model || !instance || screenWidth < 1 || screenHeight < 1)
		return;

	//The camera looks at the middle of the collision box from far enough away that the whole model fits, with room over it for a hat
	glm::vec3 center = model->getColOffset();
	float halfHeight = std::max(model->getColHalfExtents().y, 0.1f);
	float fov = glm::radians(fieldOfView);
	float distance = halfHeight * 1.45f / std::tan(fov * 0.5f) * zoom;
	glm::vec3 cameraPosition = center + glm::vec3(0.0f, 0.0f, distance);

	glm::mat4 view = glm::lookAt(cameraPosition, center, glm::vec3(0.0f, 1.0f, 0.0f));

	//Slid over to the middle of the part of the screen the panel doesn't cover
	float shift = std::clamp(panelWidth / (float)screenWidth, 0.0f, 0.5f);
	glm::mat4 projection = glm::translate(glm::vec3(shift, 0.0f, 0.0f)) * glm::perspective(fov, (float)screenWidth / (float)screenHeight, distance * 0.05f, distance * 4.0f);

	instance->setModelTransform(glm::translate(center) * glm::rotate(pitch, glm::vec3(1, 0, 0)) * glm::rotate(yaw + startYaw, glm::vec3(0, 1, 0)) * glm::translate(-center));
	for (int a = 0; a < model->getNumMeshes(); a++)
		instance->setColor(a, colors[a]);
	if (faceMesh != -1)
		instance->setDecal(faceMesh, faceDecal());
	if (shirtMesh != -1)
		instance->setDecal(shirtMesh, shirtDecal());
	model->updateAll(deltaT);

	//The chosen hat rides on the head, drawn and picked as one more part
	int chosenHat = hatIndex();
	Model* hatModel = chosenHat != -1 && chosenHat < (int)hatModels.size() ? hatModels[chosenHat] : nullptr;
	ModelInstance* hatInstance = hatModel ? hatInstances[chosenHat] : nullptr;
	if (hatInstance)
	{
		//A part like the jester's cap keeps its bells gold, see Mesh::fixedColor
		for (int a = 0; a < hatInstance->getNumMeshes(); a++)
		{
			if (hatModel->isMeshPaintable(a))
				hatInstance->setColor(a, colors[hatPart]);
		}

		if (instance->placeAttachment(hatInstance, hatScale))
			hatModel->updateAll(deltaT);
		else
			hatInstance = nullptr;
	}

	CameraUniforms& camera = shaders->cameraUniforms;
	camera.CameraView = view;
	camera.CameraAngle = glm::mat4(glm::mat3(view));
	camera.CameraPosition = cameraPosition;
	camera.CameraDirection = glm::normalize(center - cameraPosition);

	//Lit from above and in front of the camera, with no fog, water, or point lights, all put back afterward
	EnvironmentUniforms& environment = shaders->environmentUniforms;
	EnvironmentUniforms previousEnvironment = environment;
	environment.LightDirection = glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f));
	environment.SunDirection = environment.LightDirection;
	environment.LightColor = glm::vec3(3.0f);
	environment.AmbientColor = glm::vec3(0.5f);
	environment.ShadowStrength = 0.0f;
	environment.FogDistanceMin = distance * 100.0f;
	environment.FogDistanceMax = distance * 200.0f;
	environment.ClipPlane = glm::vec4(0.0f);
	shaders->updateEnvironmentUBO();

	int pointLightCount = shaders->pointLightUniforms.PointLightCount;
	shaders->pointLightUniforms.PointLightCount = 0;
	shaders->updatePointLightUBO();

	Program* program = shaders->modelShader;
	program->use();

	//Nowhere near any shadow cascade, so nothing shadows the model
	glm::mat4 noShadow = glm::translate(glm::vec3(10.0f, 10.0f, 0.0f));
	glm::mat4 lightSpaceMatricies[3] = { noShadow, noShadow, noShadow };
	glUniformMatrix4fv(program->getUniformLocation("lightSpaceMatricies"), 3, GL_FALSE, &lightSpaceMatricies[0][0][0]);
	glUniform1i(program->getUniformLocation("coloredShadows"), 0);
	GLint pickingUniform = program->getUniformLocation("pickingID");
	GLint highlightUniform = program->getUniformLocation("editorHighlight");

	shaders->basicUniforms.nonInstanced = 0;
	shaders->basicUniforms.cameraSpacePosition = 0;
	shaders->updateBasicUBO();

	glDisable(GL_BLEND);

	int mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);
	bool mouseOverView = !ImGui::GetIO().WantCaptureMouse && mouseX >= 0 && mouseY >= 0 && mouseX < screenWidth && mouseY < screenHeight;

	if (mouseOverView)
	{
		if (!pickingTarget)
		{
			RenderTarget::RenderTargetSettings pickingSettings;
			pickingSettings.width = 1;
			pickingSettings.height = 1;
			pickingSettings.channels = 3;
			pickingSettings.useDepth = false;
			pickingSettings.minFilter = GL_NEAREST;
			pickingSettings.magFilter = GL_NEAREST;
			pickingTarget = std::make_shared<RenderTarget>(pickingSettings, textures);
		}

		//Only the pixel under the mouse, stretched to fill the whole single pixel target, with each part drawn as its mesh index plus one
		float pixelX = (mouseX + 0.5f) / (float)screenWidth * 2.0f - 1.0f;
		float pixelY = 1.0f - (mouseY + 0.5f) / (float)screenHeight * 2.0f;
		camera.CameraProjection = glm::scale(glm::vec3((float)screenWidth, (float)screenHeight, 1.0f)) * glm::translate(glm::vec3(-pixelX, -pixelY, 0.0f)) * projection;
		shaders->updateCameraUBO();

		pickingTarget->use();
		for (int meshIdx : editableMeshes)
		{
			glUniform1i(pickingUniform, meshIdx + 1);
			model->renderMesh(shaders, meshIdx);
		}
		if (hatInstance)
		{
			glUniform1i(pickingUniform, hatPart + 1);
			hatModel->render(shaders);
		}
		glUniform1i(pickingUniform, 0);

		//Render targets are made with their read buffer off
		unsigned char pixel[3] = { 0, 0, 0 };
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

		hoveredMesh = (int)pixel[0] - 1;
		if (!(hoveredMesh == hatPart && hatInstance) && std::find(editableMeshes.begin(), editableMeshes.end(), hoveredMesh) == editableMeshes.end())
			hoveredMesh = -1;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenWidth, screenHeight);
	}

	camera.CameraProjection = projection;
	shaders->updateCameraUBO();

	float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() / 1000.0f * 6.2831853f);
	auto highlightPart = [&](int meshIdx)
	{
		if (hoveredMesh != -1 && sameColor(meshIdx, hoveredMesh))
		{
			//A white highlight wouldn't show up on light parts, those get a blue one
			glm::vec3 shown = colors[meshIdx].a > 0 ? glm::vec3(colors[meshIdx]) : glm::vec3(1.0f);
			glm::vec3 highlight = glm::dot(shown, glm::vec3(0.299f, 0.587f, 0.114f)) > 0.7f ? glm::vec3(0.25f, 0.5f, 1.0f) : glm::vec3(1.0f);
			glUniform4f(highlightUniform, highlight.r, highlight.g, highlight.b, 0.15f + 0.15f * pulse);
		}
		else
			glUniform4f(highlightUniform, 0.0f, 0.0f, 0.0f, 0.0f);
	};

	for (int meshIdx : editableMeshes)
	{
		highlightPart(meshIdx);
		model->renderMesh(shaders, meshIdx);
	}
	if (hatInstance)
	{
		highlightPart(hatPart);
		hatModel->render(shaders);
	}
	glUniform4f(highlightUniform, 0.0f, 0.0f, 0.0f, 0.0f);

	environment = previousEnvironment;
	shaders->updateEnvironmentUBO();
	shaders->pointLightUniforms.PointLightCount = pointLightCount;
	shaders->updatePointLightUBO();
}
