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

int UserInterface::getOpenWindowCount() const
{
	int ret = 0;
	for (unsigned int a = 0; a < windows.size(); a++)
		if (windows[a]->opened)
			ret++;
	return ret;
}

//Trigged if you hit escape, ideally, do it again and again until all windows are closed
void UserInterface::closeOneWindow()
{
	for (unsigned int a = 0; a < windows.size(); a++)
	{
		if (windows[a]->opened)
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

	if(e.key.keysym.scancode == input->getKeyBind(OpenChatWindow))
	{
		auto tmp = getWindowByName("Chat Window");
		if (tmp)
			tmp->open();
		return true;
	}

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

	if (!hudLines.empty())
	{
		auto draw = ImGui::GetBackgroundDrawList();
		float y = 10.0f;
		for (const std::string& line : hudLines)
		{
			draw->AddText(ImVec2(10.0f, y), IM_COL32(255, 190, 0, 255), line.c_str());
			y += ImGui::GetFontSize() + 4.0f;
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
