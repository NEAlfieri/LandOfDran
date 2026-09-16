#include "DebugMenu.h"

void DebugMenu::passDetails(const glm::vec3 &camPos,const glm::vec3 &camDir,const NetInfo & netInfo)
{
	cameraPosition = camPos;
	cameraDirection = camDir;
	lastPing = netInfo.ping;
	incomingData = netInfo.incomingData;
	outgoingData = netInfo.outgoingData;
	maxIncoming = std::max(maxIncoming, incomingData);
	maxOutgoing = std::max(maxOutgoing, outgoingData);
	serverLastLongestFrame = netInfo.serverLastLongestFrame;
	serverAverageFrame = netInfo.serverAverageFrame;
}

void DebugMenu::reset()
{
	autheticated = false;
	lastLoggedLines.clear();
	adminLoginComment = "";
	consoleCommandBuffer[0] = 0;
	passwordBuffer[0] = 0;
	maxIncoming = 0;
	maxOutgoing = 0;
	serverLastLongestFrame = 0.0;
	serverAverageFrame = 0.0;
}

void DebugMenu::addLogLine(loggerLine line)
{
	lastLoggedLines.push_back(line);
}

void DebugMenu::render(ImGuiIO* io)
{
	if (!opened)
		return;

	if (!ImGui::Begin("Debug Menu", &opened))
	{
		ImGui::End();
		return;
	}

	ImGui::BeginTabBar("debugTabs");

	if (ImGui::BeginTabItem("Performance"))
	{
		ImGui::Text("FPS: %f", io->Framerate);
		ImGui::Text("Ping: %f", lastPing);

		ImGui::NewLine();

		ImGui::Text("Incoming Data: %f kB/sec", incomingData);
		ImGui::Text("Outgoing Data: %f kB/sec", outgoingData);

		ImGui::NewLine();

		ImGui::Text("Server 1000ms longest frame: %f ms", serverLastLongestFrame);
		ImGui::Text("Server 1000ms average frame: %f ms", serverAverageFrame);

		ImGui::NewLine();

		ImGui::Text("Incoming Data Max: %f kB/sec", maxIncoming);
		ImGui::Text("Outgoing Data Max: %f kB/sec", maxOutgoing);

		ImGui::NewLine();

		ImGui::Text("Run time: %f seconds", getTicksMS()/1000.0f);

		ImGui::NewLine();

		//How long each render pass took, see GpuProfiler. GPU is the time the card spent on the pass,
		//CPU the time this thread spent handing it over, so a pass that's slow on both is draw call bound
		ImGui::Checkbox("Show pass timings", &showPassTimings);
		if (showPassTimings)
		{
			if (passTimings.empty())
				ImGui::Text("Measuring...");
			else if (ImGui::BeginTable("passTimings", 5, ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Pass");
				ImGui::TableSetupColumn("GPU ms");
				ImGui::TableSetupColumn("GPU worst");
				ImGui::TableSetupColumn("CPU ms");
				ImGui::TableSetupColumn("CPU worst");
				ImGui::TableHeadersRow();

				for (const GpuProfiler::Result& pass : passTimings)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					//Passes drawn inside another one are indented under it
					ImGui::Text("%*s%s", pass.depth * 2, "", pass.name.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%.2f", pass.gpuMS);
					ImGui::TableNextColumn();
					//A stutter is a worst frame far above the average, so it's worth making it stand out
					if (pass.worstGpuMS > pass.gpuMS * 2.0f + 1.0f)
						ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "%.2f", pass.worstGpuMS);
					else
						ImGui::Text("%.2f", pass.worstGpuMS);
					ImGui::TableNextColumn();
					ImGui::Text("%.2f", pass.cpuMS);
					ImGui::TableNextColumn();
					if (pass.worstCpuMS > pass.cpuMS * 2.0f + 1.0f)
						ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "%.2f", pass.worstCpuMS);
					else
						ImGui::Text("%.2f", pass.worstCpuMS);
				}

				ImGui::EndTable();
			}
		}

		ImGui::NewLine();

		for(unsigned int a = 0; a<extraLines.size(); a++)
			ImGui::Text(extraLines.at(a).c_str());
		extraLines.clear();
		ImGui::EndTabItem();
	}

	if(ImGui::BeginTabItem("Camera"))
	{
		ImGui::Text("Camera position: %f,%f,%f", cameraPosition.x, cameraPosition.y, cameraPosition.z);
		ImGui::Text("Camera direction: %f,%f,%f", cameraDirection.x, cameraDirection.y, cameraDirection.z);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Console"))
	{
		if (!autheticated)
		{
			ImGui::Text("You need to log into the server as admin to use this feature.");
			bool passSubmit = false;
			if (ImGui::InputText("Password", passwordBuffer, 256, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue))
				passSubmit = true;
			if (ImGui::Button("Authenticate"))
				passSubmit = true;
			if (strlen(passwordBuffer) < 1)
			{
				if (passSubmit)
					adminLoginComment = "Please enter a password";
				passSubmit = false;
			}
			wantsToAuthenticate = passSubmit;
			if(strlen(adminLoginComment.c_str()) > 0)
				ImGui::Text(adminLoginComment.c_str());
			//Login will be started in ClientLoop::run
		}
		else
		{
			ImGui::BeginGroup();
			float windowWidth = ImGui::GetContentRegionAvail().x;

			//Reserve space for what's below the scroll region (checkbox row, repeat/submit row, and the input box)
			//so the log area actually grows/shrinks with the window instead of staying a fixed height
			const float inputBoxHeight = 200.0f;
			float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2 + inputBoxHeight + ImGui::GetStyle().ItemSpacing.y;
			ImGui::BeginChild("debugScroll", ImVec2(windowWidth, -footerHeight));
			ImGui::PushTextWrapPos(0.0f);

			//This line would be for showing client side logs
			//const std::deque<loggerLine> * const lastLoggedLines = Logger::getStorage();

			//Display a list of text lines with flags for their color
			for (int a = 0; a < lastLoggedLines.size(); a++)
			{
				if (lastLoggedLines.at(a).isDebug && !showVerboseLogging)
					continue;

				if (lastLoggedLines.at(a).isError)
					ImGui::TextColored(ImVec4(1, 0, 0, 1), lastLoggedLines.at(a).text.c_str());
				else if (lastLoggedLines.at(a).isDebug)
					ImGui::TextColored(ImVec4(1, 1, 1, 0.5), lastLoggedLines.at(a).text.c_str());
				else
				{
					if (lastLoggedLines.at(a).text.length() > 6)
					{
						if (lastLoggedLines.at(a).text.substr(0, 6) == "INPUT ")
						{
							ImGui::TextColored(ImVec4(0, 1, 0, 1), lastLoggedLines.at(a).text.substr(6).c_str());
							continue;
						}
					}
					ImGui::Text(lastLoggedLines.at(a).text.c_str());
				}
			}

			//Scroll lock button
			if (scrollToBottom)
				ImGui::SetScrollY(ImGui::GetScrollMaxY());

			ImGui::PopTextWrapPos();
			ImGui::EndChild();
			ImGui::EndGroup();

			//Options
			ImGui::Checkbox("Verbose", &showVerboseLogging);
			ImGui::SameLine();
			ImGui::Checkbox("Scroll lock", &scrollToBottom);
			ImGui::SameLine();
			ImGui::Checkbox("Enter to submit", &enterBehavior);

			//Place last command in the text box again
			if(ImGui::Button("Repeat"))
				strcpy(consoleCommandBuffer, lastCommand.c_str());

			bool wantsSubmit = false;
			
			ImGui::SameLine();
			if (ImGui::Button("Submit"))
				wantsSubmit = true;

			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::BeginItemTooltip())
			{
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted("If enter to submit is checked, you can hit CTRL+Enter for a new line. Normal enter will submit the command. Hit the Repeat button to put the last command in the text box.");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			if (ImGui::InputTextMultiline("<- Lua", consoleCommandBuffer, 10000, ImVec2(windowWidth * 0.9, inputBoxHeight), ImGuiInputTextFlags_EnterReturnsTrue | (enterBehavior ? ImGuiInputTextFlags_CtrlEnterForNewLine : 0) | ImGuiInputTextFlags_AllowTabInput))
				wantsSubmit = true; //Hit enter/return while typing

			if(wantsSubmit)
			{
				std::string command = std::string(consoleCommandBuffer);
				if (command.length() > 0)
				{
					luaCommandWaiting = true;
					waitingLuaCommand = command;
					lastCommand = command;
					consoleCommandBuffer[0] = 0;
				}
			}
		}

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	ImGui::End();
}

void DebugMenu::init()
{

}

void DebugMenu::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{

}

DebugMenu::DebugMenu()
{
	name = "Debug Menu";
	consoleCommandBuffer[0] = 0;
	passwordBuffer[0] = 0;	
}

DebugMenu::~DebugMenu()
{

}
