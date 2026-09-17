#include "WrenchDialog.h"
#include "../Bricks/BrickSaves.h"

#include <cmath>

//A dropdown with None first, for picking music or an emitter by name
static void nameCombo(const char* label, std::string& picked, const std::vector<std::string>& names)
{
	if (!ImGui::BeginCombo(label, picked.empty() ? "None" : picked.c_str()))
		return;

	if (ImGui::Selectable("None", picked.empty()))
		picked = "";

	for (size_t a = 0; a < names.size(); a++)
	{
		ImGui::PushID((int)a);
		if (ImGui::Selectable(names[a].c_str(), names[a] == picked))
			picked = names[a];
		ImGui::PopID();
	}

	ImGui::EndCombo();
}

//A typed file name without spaces around it
static std::string trimmedSaveName(const std::string& name)
{
	size_t start = name.find_first_not_of(" \t");
	if (start == std::string::npos)
		return "";
	return name.substr(start, name.find_last_not_of(" \t") - start + 1);
}

//A collapsing header with a blank line above it, unless it's the first thing in the settings
static bool sectionHeader(const char* label)
{
	if (ImGui::GetCursorPosY() > ImGui::GetCursorStartPos().y)
		ImGui::NewLine();
	return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
}

static void tooltip(const char* text)
{
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", text);
}

//A red button, for something that can't be undone
static bool dangerButton(const char* label)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.12f, 0.12f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.24f, 0.24f, 1.0f));
	bool clicked = ImGui::Button(label);
	ImGui::PopStyleColor(3);
	return clicked;
}

//A button that stays lit while whatever it turns on is on
static bool toggleButton(const char* label, bool& on)
{
	bool wasOn = on;
	if (wasOn)
	{
		const ImVec4& active = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
		ImGui::PushStyleColor(ImGuiCol_Button, active);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active);
	}

	bool clicked = ImGui::Button(label);

	if (wasOn)
		ImGui::PopStyleColor(2);

	if (clicked)
		on = !on;

	return clicked;
}

void WrenchDialog::stashCopy()
{
	if (!wasOpen)
		return;

	wasOpen = false;

	if (!copying)
		return;

	copied = editing;
	copiedFromVehicle = editing.vehicleID != NO_ID;
	hasCopy = true;
}

//A vehicle's dialog is only its music, and a wheel's settings mean nothing on a brick that isn't a wheel
void WrenchDialog::applyCopy()
{
	const BrickAttachments& from = copied.attachments;
	BrickAttachments& to = editing.attachments;

	to.musicName = from.musicName;
	to.musicVolume = from.musicVolume;
	to.musicPitch = from.musicPitch;

	if (editing.vehicleID != NO_ID || copiedFromVehicle)
		return;

	editing.collides = copied.collides;
	editing.name = copied.name;

	to.hasLight = from.hasLight;
	to.lightColor = from.lightColor;
	to.lightBrightness = from.lightBrightness;
	to.lightFlicker = from.lightFlicker;
	to.lightBlinkSpeed = from.lightBlinkSpeed;
	to.lightBlinkStrength = from.lightBlinkStrength;
	to.lightCoronaWidth = from.lightCoronaWidth;
	to.lightConeAngle = from.lightConeAngle;
	to.lightDirection = from.lightDirection;
	to.lightSpin = from.lightSpin;
	to.lightOffset = from.lightOffset;

	to.emitterName = from.emitterName;

	if (editing.part == VehiclePart_Wheel && from.hasWheel)
		to.wheel = from.wheel;

	if (editing.part == VehiclePart_Steering && from.hasSteering)
		to.steering = from.steering;
}

void WrenchDialog::openFor(const WrenchSubmission& settings, const std::string& label, const std::vector<std::string>& music, const std::vector<std::string>& emitters)
{
	//Opening one dialog right on top of another still counts as closing the first
	stashCopy();

	editing = settings;

	if (copying && hasCopy)
		applyCopy();

	brickLabel = label;
	musicNames = music;
	emitterNames = emitters;

	//Applying a wheel or steering wheel's dialog keeps its settings, even the defaults it opened with
	if (editing.part == VehiclePart_Wheel)
		editing.attachments.hasWheel = true;
	if (editing.part == VehiclePart_Steering)
		editing.attachments.hasSteering = true;

	//Applying without touching them keeps whatever Lua put on the brick
	const std::string& musicName = editing.attachments.musicName;
	if (!musicName.empty() && std::find(musicNames.begin(), musicNames.end(), musicName) == musicNames.end())
		musicNames.push_back(musicName);

	const std::string& emitterName = editing.attachments.emitterName;
	if (!emitterName.empty() && std::find(emitterNames.begin(), emitterNames.end(), emitterName) == emitterNames.end())
		emitterNames.push_back(emitterName);

	const glm::vec3& direction = editing.attachments.lightDirection;
	lightPitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
	lightYaw = glm::degrees(std::atan2(direction.x, direction.z));

	if (editing.attachments.lightConeAngle > 0.0f)
		lastConeAngle = editing.attachments.lightConeAngle;

	submitted = false;
	justOpened = true;
	framesToCenter = 3;
	open();
}

bool WrenchDialog::takeSubmission(WrenchSubmission& submission)
{
	if (!submitted)
		return false;

	submitted = false;
	submission = editing;
	submission.name = submission.name.substr(0, 255);
	submission.attachments.clampValues();
	return true;
}

bool WrenchDialog::takeRemoveRequest(netIDType& vehicleID)
{
	if (!removeRequested)
		return false;

	removeRequested = false;
	vehicleID = editing.vehicleID;
	return vehicleID != NO_ID;
}

bool WrenchDialog::takeSaveRequest(netIDType& vehicleID, std::string& path)
{
	if (!saveRequested)
		return false;

	saveRequested = false;
	path = getVehicleSavePath(trimmedSaveName(saveName));
	vehicleID = editing.vehicleID;
	return vehicleID != NO_ID && !path.empty();
}

void WrenchDialog::render(ImGuiIO* io)
{
	if (!opened)
	{
		stashCopy();
		return;
	}

	wasOpen = true;

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

	if (!ImGui::Begin("Wrench", &opened, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();

	//Opening sections grows it downward from where it opened, which can push it past the bottom of the screen
	ImVec2 position = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();
	float x = std::clamp(position.x, viewport->WorkPos.x, std::max(viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - size.x));
	float y = std::clamp(position.y, viewport->WorkPos.y, std::max(viewport->WorkPos.y, viewport->WorkPos.y + viewport->WorkSize.y - size.y));
	if (x != position.x || y != position.y)
		ImGui::SetWindowPos(ImVec2(x, y));

	BrickAttachments& settings = editing.attachments;
	bool forVehicle = editing.vehicleID != NO_ID;

	//Turning it off drops what it was holding, so turning it back on starts from whatever window that is
	if (toggleButton("Copy", copying) && !copying)
		hasCopy = false;
	tooltip("Keeps these settings when the window closes, and fills in the next brick you wrench with the ones that fit it");

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(brickLabel.c_str());
	ImGui::Separator();

	//The settings scroll once they'd be taller than most of the screen, which a light and large UI scaling easily make them, so Apply stays in view
	float itemWidth = ImGui::GetFontSize() * 16.0f;
	float bodyWidth = itemWidth + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Compression damping").x + style.ScrollbarSize + ImGui::GetFontSize();
	float chrome = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f + ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y * 2.0f;
	float maxBodyHeight = std::max(ImGui::GetFontSize() * 6.0f, viewport->WorkSize.y * 0.95f - chrome);
	ImGui::SetNextWindowSizeConstraints(ImVec2(bodyWidth, 0.0f), ImVec2(bodyWidth, maxBodyHeight));
	ImGui::BeginChild("Settings", ImVec2(bodyWidth, 0.0f), ImGuiChildFlags_AutoResizeY);
	ImGui::PushItemWidth(itemWidth);

	if (!forVehicle)
	{
		ImGui::Checkbox("Colliding", &editing.collides);
		tooltip("Whether players and objects bump into it. Clicks and raycasts hit it either way");

		ImGui::InputText("Name", &editing.name);
		tooltip("For scripts to find it by");
	}

	if (editing.part == VehiclePart_Wheel && sectionHeader("Wheel"))
	{
		WheelSettings& wheel = settings.wheel;
		ImGui::TextDisabled("Used once its bricks are sliced into a vehicle");

		ImGui::SliderFloat("Engine force", &wheel.engineForce, -2000.0f, 2000.0f, "%.0f");
		tooltip("How hard it drives the vehicle forward. Negative drives it backward, 0 just rolls along");

		ImGui::SliderFloat("Brake force", &wheel.brakeForce, 0.0f, 2000.0f, "%.0f");
		tooltip("How hard it stops while the driver holds jump");

		float steerDegrees = glm::degrees(wheel.steerAngle);
		if (ImGui::SliderFloat("Steering", &steerDegrees, -90.0f, 90.0f, "%.0f degrees"))
			wheel.steerAngle = glm::radians(steerDegrees);
		tooltip("How far it turns while steering. 0 doesn't steer, negative turns the other way, like rear wheel steering");

		ImGui::SliderFloat("Suspension length", &wheel.suspensionLength, 0.1f, 5.0f, "%.2f studs");
		tooltip("How far it hangs down from where it's attached when resting");

		ImGui::SliderFloat("Stiffness", &wheel.suspensionStiffness, 1.0f, 1000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
		tooltip("How hard the suspension pushes back. Low is bouncy, high is rigid");

		ImGui::SliderFloat("Compression damping", &wheel.dampingCompression, 1.0f, 100.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
		tooltip("How much the suspension resists being pushed in");

		ImGui::SliderFloat("Relaxation damping", &wheel.dampingRelaxation, 1.0f, 100.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
		tooltip("How much the suspension resists springing back out");

		ImGui::SliderFloat("Grip", &wheel.frictionSlip, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		tooltip("How hard it holds the ground before sliding");

		ImGui::SliderFloat("Roll influence", &wheel.rollInfluence, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		tooltip("How much cornering tips the vehicle over. Lower is steadier");
	}

	if (editing.part == VehiclePart_Steering && sectionHeader("Vehicle"))
	{
		SteeringSettings& steering = settings.steering;
		ImGui::TextDisabled("Used once its bricks are sliced into a vehicle");

		ImGui::SliderFloat("Mass", &steering.mass, 1.5f, 30.0f, "%.1f");
		tooltip("How heavy each brick is to turn and tip over");

		ImGui::SliderFloat("Spin damping", &steering.angularDamping, 0.0f, 1.0f, "%.2f");
		tooltip("How quickly it stops spinning");

		ImGui::Checkbox("Realistic center of mass", &steering.realisticCenterOfMass);
		tooltip("Off, it turns around a point down near its wheels, which keeps it from flipping. On, around the middle of its bricks");
	}

	if (sectionHeader("Music"))
	{
		if (musicNames.empty())
			ImGui::TextDisabled("The server has no music");
		else
		{
			nameCombo("Loop", settings.musicName, musicNames);

			if (!settings.musicName.empty())
			{
				ImGui::SliderFloat("Volume", &settings.musicVolume, 0.0f, 1.0f, "%.2f");
				ImGui::SliderFloat("Pitch##Music", &settings.musicPitch, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
				tooltip("Playback speed. Ctrl+click to type anything from 0.05 to 10");
			}
		}
	}

	if (!forVehicle && sectionHeader("Light"))
	{
		ImGui::Checkbox("Has light", &settings.hasLight);

		if (settings.hasLight)
		{
			ImGui::ColorEdit3("Color", &settings.lightColor[0]);

			ImGui::SliderFloat("Brightness", &settings.lightBrightness, 0.0f, 100000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			tooltip("A lamp is around 20 to 100, a floodlight a few thousand. How far it reaches follows from this");

			ImGui::SliderFloat("Flicker", &settings.lightFlicker, 0.0f, 2.0f, "%.2f studs", ImGuiSliderFlags_Logarithmic);
			tooltip("How far the light wanders around, like a flame");

			ImGui::SliderFloat("Blink speed", &settings.lightBlinkSpeed, 0.0f, 10.0f, "%.2f s", ImGuiSliderFlags_Logarithmic);
			tooltip("Seconds for one full cycle of dimming and brightening again, 0 for no blinking. Ctrl+click to type up to 60");

			if (settings.lightBlinkSpeed > 0.0f)
			{
				ImGui::SliderFloat("Blink strength", &settings.lightBlinkStrength, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				tooltip("How much it dims at the low point of each cycle, 1 goes fully dark");
			}

			ImGui::SliderFloat("Corona", &settings.lightCoronaWidth, 0.0f, 30.0f, "%.2f studs", ImGuiSliderFlags_Logarithmic);
			tooltip("Width of the glow drawn at the light, 0 for none");

			ImGui::DragFloat3("Offset", &settings.lightOffset[0], 0.05f, -BrickAttachments::maxLightOffset, BrickAttachments::maxLightOffset, "%.2f");
			tooltip("Where the light is from the middle of the brick, in studs. Drag or double click to type");

			bool spotlight = settings.lightConeAngle > 0.0f;
			if (ImGui::Checkbox("Spotlight", &spotlight))
				settings.lightConeAngle = spotlight ? lastConeAngle : 0.0f;

			if (spotlight)
			{
				if (ImGui::SliderFloat("Cone angle", &settings.lightConeAngle, 1.0f, 179.0f, "%.0f degrees", ImGuiSliderFlags_AlwaysClamp))
					lastConeAngle = settings.lightConeAngle;
				tooltip("Full width of the beam");

				bool turned = ImGui::SliderFloat("Yaw", &lightYaw, -180.0f, 180.0f, "%.0f degrees");
				turned |= ImGui::SliderFloat("Pitch##Light", &lightPitch, -80.0f, 80.0f, "%.0f degrees");
				tooltip("-90 points straight down, 90 straight up");
				if (turned)
				{
					float yaw = glm::radians(lightYaw);
					float pitch = glm::radians(lightPitch);
					settings.lightDirection = glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
				}

				ImGui::SliderFloat("Spin", &settings.lightSpin, -720.0f, 720.0f, "%.0f degrees/s");
				tooltip("Turns the beam around the vertical like a lighthouse. Ctrl+click to type up to 3600");
			}
		}
	}

	if (!forVehicle && sectionHeader("Emitter"))
	{
		if (emitterNames.empty())
			ImGui::TextDisabled("The server has no emitters");
		else
			nameCombo("Type##Emitter", settings.emitterName, emitterNames);
	}

	if (forVehicle && sectionHeader("Save"))
	{
		ImGui::InputText("File name", &saveName);
		tooltip("Saved to Saves/Vehicles on your computer, load it again from the Vehicles window");

		bool usable = !getVehicleSavePath(trimmedSaveName(saveName)).empty();
		ImGui::BeginDisabled(!usable);
		if (ImGui::Button("Save to my computer"))
			saveRequested = true;
		ImGui::EndDisabled();

		if (!usable && !saveName.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Names can't have / \\ : * ? \" < > |");
		}
	}

	ImGui::PopItemWidth();
	ImGui::EndChild();
	ImGui::Separator();

	if (ImGui::Button("Apply"))
	{
		submitted = true;
		close();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel"))
		close();

	if (forVehicle)
	{
		ImGui::SameLine();
		if (dangerButton("Remove vehicle"))
			ImGui::OpenPopup("Remove vehicle?");

		if (ImGui::BeginPopupModal("Remove vehicle?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::TextUnformatted("Its bricks won't come back. Save it first to keep a copy.");

			if (dangerButton("Remove"))
			{
				removeRequested = true;
				ImGui::CloseCurrentPopup();
				close();
			}

			ImGui::SameLine();

			if (ImGui::Button("Keep it"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	ImGui::End();
}

void WrenchDialog::init()
{

}

void WrenchDialog::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

WrenchDialog::WrenchDialog()
{
	name = "Wrench Dialog";
}

WrenchDialog::~WrenchDialog()
{

}
