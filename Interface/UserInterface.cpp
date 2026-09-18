#include "UserInterface.h"
#include "../External/Imgui/imgui_internal.h"

//ImGui windows can restore a saved position from imgui.ini that no longer fits the current
//display (e.g. imgui.ini was written on a larger monitor). Pull any window fully back onto the
//viewport the moment it appears so it never starts up (partially) off-screen.
static void clampAppearingWindowsToViewport()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
	{
		if (!window->Appearing || (window->Flags & ImGuiWindowFlags_ChildWindow))
			continue;

		ImVec2 minPos = viewport->Pos;
		ImVec2 maxPos = ImVec2(
			minPos.x + ImMax(0.0f, viewport->Size.x - window->Size.x),
			minPos.y + ImMax(0.0f, viewport->Size.y - window->Size.y));

		ImVec2 pos = window->Pos;
		pos.x = ImClamp(pos.x, minPos.x, maxPos.x);
		pos.y = ImClamp(pos.y, minPos.y, maxPos.y);

		if (pos.x != window->Pos.x || pos.y != window->Pos.y)
			ImGui::SetWindowPos(window, pos);
	}
}

void UserInterface::updateSettings(std::shared_ptr<SettingManager> settings)
{
	ImGuiStyle& style = ImGui::GetStyle();

	PreferencePair const * pref = settings->getPreference("gui/textcolor");
	if (pref)
		style.Colors[ImGuiCol_Text] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);

	pref = settings->getPreference("gui/windowcolor");
	if (pref)
		style.Colors[ImGuiCol_WindowBg] = ImVec4(pref->color[0],pref->color[1],pref->color[2],pref->color[3]);

	pref = settings->getPreference("gui/framecolor");
	if (pref)
	{
		style.Colors[ImGuiCol_FrameBg] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_Button] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_Tab] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_Header] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
	}

	pref = settings->getPreference("gui/framehovercolor");
	if (pref)
	{
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
	}

	pref = settings->getPreference("gui/highlight");
	if (pref)
	{
		style.Colors[ImGuiCol_CheckMark] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
	}

	pref = settings->getPreference("gui/frameclickcolor");
	if (pref)
	{
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_TabActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);
	}

	pref = settings->getPreference("gui/titlecolor");
	if (pref)
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(pref->color[0], pref->color[1], pref->color[2], pref->color[3]);

	float rounding = settings->getFloat("gui/rounding");
	style.WindowRounding = rounding;
	style.ChildRounding = rounding;
	style.FrameRounding = rounding;
	style.PopupRounding = rounding;
	style.GrabRounding = rounding;

	globalInterfaceTransparency = settings->getFloat("gui/opacity");

	showFps = settings->getBool("gui/showfps");

	//Refer to DefaultPreferences.cpp - Small, Normal, Large, Largest
	int sizeEnum = settings->getInt("gui/scaling");
	switch (sizeEnum)
	{
		case 0: 
			uiScaling = 0.5;
			break;
		case 1:
			uiScaling = 1.0;
			break;
		case 2:
			uiScaling = 1.5;
			break;
		case 3:
			uiScaling = 2.0;
			break;
	}
}

bool UserInterface::wantsSuppression() const
{
	return io->WantCaptureKeyboard;
}

//If mouselock should be forced on
bool UserInterface::shouldUnlockMouse() const
{
	return io->WantCaptureMouse;
}

void UserInterface::setMouseCaptured(bool captured)
{
	if (captured)
		io->ConfigFlags |= ImGuiConfigFlags_NoMouse;
	else
		io->ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

int UserInterface::getOpenWindowCount() const
{
	int ret = 0;
	for (unsigned int a = 0; a < windows.size(); a++)
		if (windows[a]->opened && !windows[a]->hudWindow)
			ret++;
	return ret;
}

//Trigged if you hit escape, ideally, do it again and again until all windows are closed
void UserInterface::closeOneWindow()
{
	for (unsigned int a = 0; a < windows.size(); a++)
	{
		if (windows[a]->opened && !windows[a]->hudWindow)
		{
			windows[a]->opened = false;
			return;
		}
	}
}

void Window::close()
{
	opened = false;
}

void Window::open()
{
	//TODO: Make new windows appear in upper left corner and slighty offset depending on how many are open
	opened = true;
}

void UserInterface::showPopup()
{
	if (popupErrorMessage.length() < 1)
		return;

	ImGui::OpenPopup("MessagePopup");
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("MessagePopup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(popupErrorMessage.c_str());
		if (ImGui::Button("Okay")) //acknowledged
			popupErrorMessage = "";
		ImGui::EndPopup();
	}
}

bool UserInterface::handleInput(SDL_Event& e,std::shared_ptr<InputMap> input)
{
	ImGui_ImplSDL2_ProcessEvent(&e);

	for (unsigned int a = 0; a < windows.size(); a++)
		windows[a]->handleInput(e, input);

	//Don't open other windows while typing in a window already
	if (io->WantCaptureKeyboard)
		return false;

	if (e.type != SDL_KEYDOWN)
		return false;

	if (e.key.keysym.scancode == input->getKeyBind(OpenDebugWindow))
	{
		auto tmp = getWindowByName("Debug Menu");
		if (tmp)
			tmp->open();
		return true;
	}

	if (e.key.keysym.scancode == input->getKeyBind(OpenOptionsMenu))
	{
		auto tmp = getWindowByName("Settings Menu");
		if (tmp)
			tmp->open();
		return true;
	}

	//The chat key is ChatWindow's own, see ChatWindow::handleInput

	return false;
}

std::shared_ptr<Window> UserInterface::getWindowByName(const std::string &name)
{
	for (unsigned int a = 0; a < windows.size(); a++)
		if (windows[a]->name == name)
			return windows[a];
	return nullptr;
}

void UserInterface::addCenterPrint(const std::string& text, unsigned int durationMS, float red, float green, float blue)
{
	if (durationMS == 0)
	{
		centerPrints.clear();
		return;
	}

	CenterPrintMessage msg;
	msg.text = text;
	msg.expireTicksMS = SDL_GetTicks() + durationMS;
	msg.color = IM_COL32(
		(int)(std::clamp(red, 0.0f, 1.0f) * 255.0f),
		(int)(std::clamp(green, 0.0f, 1.0f) * 255.0f),
		(int)(std::clamp(blue, 0.0f, 1.0f) * 255.0f),
		255);

	centerPrints.push_back(msg);
}

void UserInterface::initAll()
{
	for (unsigned int a = 0; a < windows.size(); a++)
		windows[a]->init();
}

/*
	Frames are counted for fpsAverageOverMS at a time. The average is what the frame rate feels like, and the
	slowest single frame in the same stretch is what a stutter shows up as, which an average hides completely
*/
float UserInterface::renderFpsCounter(float deltaSeconds, float top)
{
	if (deltaSeconds > 0.0f)
	{
		fpsFrames++;
		fpsTotalSeconds += deltaSeconds;
		fpsWorstSeconds = std::max(fpsWorstSeconds, deltaSeconds);
	}

	fpsSinceShownMS += deltaSeconds * 1000.0f;
	if (fpsSinceShownMS >= fpsAverageOverMS && fpsFrames > 0)
	{
		shownAverageFps = fpsFrames / fpsTotalSeconds;
		shownWorstFps = 1.0f / fpsWorstSeconds;
		fpsFrames = 0;
		fpsTotalSeconds = 0;
		fpsWorstSeconds = 0;
		fpsSinceShownMS = 0;
	}

	//Nothing to show until the first window is up
	if (shownAverageFps <= 0.0f)
		return top;

	std::string text = "FPS " + std::to_string((int)std::lround(shownAverageFps)) + "  worst " + std::to_string((int)std::lround(shownWorstFps));

	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
	ImVec2 at(10.0f, top);
	ImVec2 padding(5.0f, 2.0f);
	draw->AddRectFilled(ImVec2(at.x - padding.x, at.y - padding.y), ImVec2(at.x + textSize.x + padding.x, at.y + textSize.y + padding.y),
		IM_COL32(20, 20, 25, 150), 3.0f);
	draw->AddText(at, IM_COL32_WHITE, text.c_str());

	return at.y + textSize.y + padding.y + 6.0f;
}

void UserInterface::render(int screenX,int screenY,bool drawCrossHair,const std::vector<std::string>& hudLines)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	showPopup();

	for (unsigned int a = 0; a < windows.size(); a++)
	{
		ImGui::SetNextWindowBgAlpha(globalInterfaceTransparency);
		windows[a]->render(io);
	}
	clampAppearingWindowsToViewport();
	io->FontGlobalScale = uiScaling;

	//Crosshair
	//ImGui::Begin("#CH", nullptr, ImGuiWindowFlags_NoMove | ImGuiInputTextFlags_ReadOnly | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
	if (drawCrossHair)
	{
		auto draw = ImGui::GetBackgroundDrawList();
		draw->AddCircle(ImVec2(screenX / 2, screenY / 2), 6, IM_COL32(255, 0, 0, 255), 100, 0.0f);
	}
	//ImGui::End();

	//The frame rate sits in the same corner as the hud lines, above them
	float y = 10.0f;
	if (showFps)
		y = renderFpsCounter(io->DeltaTime, y);

	if (!hudLines.empty())
	{
		auto draw = ImGui::GetBackgroundDrawList();
		for (const std::string& line : hudLines)
		{
			draw->AddText(ImVec2(10.0f, y), IM_COL32(255, 190, 0, 255), line.c_str());
			y += ImGui::GetFontSize() + 4.0f;
		}
	}

	//Names over players' heads and anything else Lua gave a name tag, farthest first so nearer ones cover them
	if (!nameTags.empty())
	{
		std::sort(nameTags.begin(), nameTags.end(),
			[](const WorldNameTag& a, const WorldNameTag& b) { return a.distance > b.distance; });

		auto draw = ImGui::GetBackgroundDrawList();
		for (const WorldNameTag& tag : nameTags)
		{
			ImVec2 textSize = ImGui::CalcTextSize(tag.text.c_str());
			ImVec2 at(tag.position.x - textSize.x * 0.5f, tag.position.y - textSize.y * 0.5f);
			//The plate behind it fades out along with the text, so a tag is readable over anything
			unsigned int alpha = (tag.color >> IM_COL32_A_SHIFT) & 0xFF;
			ImVec2 padding(5.0f, 2.0f);
			draw->AddRectFilled(ImVec2(at.x - padding.x, at.y - padding.y), ImVec2(at.x + textSize.x + padding.x, at.y + textSize.y + padding.y),
				IM_COL32(20, 20, 20, (int)(alpha * 0.55f)), 3.0f);
			draw->AddText(at, tag.color, tag.text.c_str());
		}
	}

	//Building mode toggles, from the bottom right corner leftwards
	{
		auto draw = ImGui::GetBackgroundDrawList();
		float right = screenX - 10.0f;

		//meter (0-1) fills the box from the left instead of the whole box lighting up when it's on, for the microphone level
		auto drawIndicator = [&](const char* label, int state, float meter = -1.0f, bool meterWarning = false)
		{
			if (state < 0)
				return;

			bool on = state == 1;
			bool showMeter = on && meter >= 0.0f;
			ImVec2 padding(8.0f, 4.0f);
			ImVec2 textSize = ImGui::CalcTextSize(label);
			//A meter needs some room to move in
			float width = textSize.x + padding.x * 2.0f;
			if (showMeter)
				width = std::max(width, ImGui::GetFontSize() * 7.0f);
			ImVec2 max(right, screenY - 10.0f);
			ImVec2 min(max.x - width, max.y - textSize.y - padding.y * 2.0f);

			if (showMeter)
			{
				draw->AddRectFilled(min, max, IM_COL32(30, 30, 30, 160), 4.0f);
				float filled = std::clamp(meter, 0.0f, 1.0f) * width;
				if (filled > 1.0f)
					draw->AddRectFilled(min, ImVec2(min.x + filled, max.y), meterWarning ? IM_COL32(190, 50, 40, 220) : IM_COL32(40, 150, 60, 220), 4.0f);
			}
			else
				draw->AddRectFilled(min, max, on ? IM_COL32(40, 150, 60, 220) : IM_COL32(30, 30, 30, 160), 4.0f);
			draw->AddRect(min, max, on ? IM_COL32(130, 255, 150, 255) : IM_COL32(120, 120, 120, 200), 4.0f);
			draw->AddText(ImVec2(min.x + padding.x, min.y + padding.y), on ? IM_COL32_WHITE : IM_COL32(160, 160, 160, 255), label);

			right = min.x - 6.0f;
		};

		drawIndicator(voiceIndicator == 1 ? "Talking" : "Voice Muted", voiceIndicator, voiceLevel, voiceClipping);
		drawIndicator("Super Shift", superShiftIndicator);
		drawIndicator("Resize", resizeIndicator);

		//Who can be heard talking, stacked upwards from above the indicators
		float lineHeight = ImGui::GetFontSize() + 4.0f;
		float y = screenY - 10.0f - (ImGui::GetFontSize() + 8.0f) - 6.0f - lineHeight;
		for (const std::string& speaker : voiceSpeakers)
		{
			ImVec2 textSize = ImGui::CalcTextSize(speaker.c_str());
			float x = screenX - 10.0f - textSize.x;
			draw->AddCircleFilled(ImVec2(x - 10.0f, y + textSize.y * 0.5f), 4.0f, IM_COL32(90, 230, 110, 255));
			draw->AddText(ImVec2(x, y), IM_COL32_WHITE, speaker.c_str());
			y -= lineHeight;
		}
	}

	if (!centerPrints.empty())
	{
		unsigned int now = SDL_GetTicks();
		centerPrints.erase(std::remove_if(centerPrints.begin(), centerPrints.end(),
			[now](const CenterPrintMessage& msg) { return now > msg.expireTicksMS; }), centerPrints.end());

		auto draw = ImGui::GetBackgroundDrawList();
		float lineHeight = ImGui::GetFontSize() + 4.0f;
		float y = screenY / 2.0f - (centerPrints.size() * lineHeight) / 2.0f;
		for (const CenterPrintMessage& msg : centerPrints)
		{
			ImVec2 textSize = ImGui::CalcTextSize(msg.text.c_str());
			draw->AddText(ImVec2(screenX / 2.0f - textSize.x / 2.0f, y), msg.color, msg.text.c_str());
			y += lineHeight;
		}
	}

	ImGui::Render();
	ImDrawData* data = ImGui::GetDrawData();
	ImGui_ImplOpenGL3_RenderDrawData(data);
}

UserInterface::UserInterface()
{
	io = &ImGui::GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	//It's so weird that ImGui handles it this way lol
	const char* glsl_version = "#version 330";
	ImGui_ImplOpenGL3_Init(glsl_version);
}

UserInterface::~UserInterface()
{
	for (unsigned int a = 0; a < windows.size(); a++)
		windows[a].reset();
	windows.clear();
}
