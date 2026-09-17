#include "ChatWindow.h"

void ChatWindow::addMessage(const std::string &message)
{
	messages.push_back(message);
}

void ChatWindow::addSuggestion(const std::string& command, const std::string& text)
{
	Suggestion suggestion;
	suggestion.command = command;
	for (char& c : suggestion.command)
		c = (char)tolower((unsigned char)c);
	suggestion.text = text.empty() ? "/" + suggestion.command : text;

	if (suggestion.command.empty())
		return;

	for (Suggestion& existing : suggestions)
	{
		if (existing.command == suggestion.command)
		{
			existing.text = suggestion.text;
			return;
		}
	}

	suggestions.push_back(suggestion);

	//Whatever is being typed may match it
	lastSeenMessage = "";
}

void ChatWindow::clearSuggestions()
{
	suggestions.clear();
	candidates.clear();
	selectedCandidate = -1;
	lastSeenMessage = "";
}

void ChatWindow::updateCandidates(const char* message)
{
	candidates.clear();
	selectedCandidate = -1;

	if (message[0] != '/')
		return;

	//The command is whatever follows the slash up to the first space, the arguments are after it
	const char* start = message + 1;
	const char* space = strchr(start, ' ');
	std::string typed = space ? std::string(start, space - start) : std::string(start);
	for (char& c : typed)
		c = (char)tolower((unsigned char)c);

	for (size_t a = 0; a < suggestions.size(); a++)
	{
		const std::string& command = suggestions[a].command;
		//Once arguments are being typed, only the command they belong to stays listed, as a reminder of what it takes
		bool matches = space ? (command == typed) : (command.compare(0, typed.length(), typed) == 0);
		if (matches)
			candidates.push_back(a);
	}
}

void ChatWindow::pickCandidate(ImGuiInputTextCallbackData* data, int direction)
{
	if (candidates.empty())
		return;

	int count = (int)candidates.size();
	if (selectedCandidate < 0)
		selectedCandidate = direction > 0 ? 0 : count - 1;
	else
		selectedCandidate = (selectedCandidate + direction + count) % count;

	//The command with a space after it, so arguments can be typed straight away
	std::string text = "/" + suggestions[candidates[selectedCandidate]].command + " ";
	data->DeleteChars(0, data->BufTextLen);
	data->InsertChars(0, text.c_str());
	data->CursorPos = data->BufTextLen;
	data->SelectionStart = data->SelectionEnd = data->CursorPos;

	//Not a new message to match against, the candidates stay so the arrows keep cycling them
	lastSeenMessage = text;
	scrollToSelected = true;
}

int ChatWindow::messageCallback(ImGuiInputTextCallbackData* data)
{
	ChatWindow* window = (ChatWindow*)data->UserData;

	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
	{
		if (data->EventKey == ImGuiKey_UpArrow)
			window->pickCandidate(data, -1);
		else if (data->EventKey == ImGuiKey_DownArrow)
			window->pickCandidate(data, 1);
	}

	return 0;
}

void ChatWindow::startTyping()
{
	if (!opened || typing)
		return;

	typing = true;
	focusRequested = true;
	focusFramesLeft = focusFrames;
	releaseFocus = false;
}

void ChatWindow::stopTyping(bool cancel)
{
	if (cancel)
	{
		messageBuffer[0] = 0;
		clearAfterRelease = true;
	}

	if (!typing)
		return;

	typing = false;
	focusRequested = false;
	focusFramesLeft = 0;
	releaseFocus = true;

	candidates.clear();
	selectedCandidate = -1;
	lastSeenMessage = "";
}

void ChatWindow::render(ImGuiIO* io)
{
	if (!opened)
		return;

	if (releaseFocus)
	{
		//Also ends the message bar being active, so nothing keeps the game's keys suppressed
		ImGui::SetWindowFocus(nullptr);
		releaseFocus = false;
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
	if (focusRequested)
		ImGui::SetNextWindowCollapsed(false);

	//No close button, and it never takes the keyboard from being clicked or appearing, only from startTyping
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	if (!ImGui::Begin("Chat Window", nullptr, flags))
	{
		//Collapsed, which ends typing
		if (typing)
		{
			stopTyping(false);
			ImGui::SetWindowFocus(nullptr);
			releaseFocus = false;
		}
		ImGui::End();
		return;
	}

	//Suggestions are only listed while typing a command, each takes a text row
	int suggestionRows = typing ? std::min((int)candidates.size(), maxSuggestionRows) : 0;
	float suggestionHeight = suggestionRows > 0 ? suggestionRows * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y * 2 : 0.0f;

	//Messages fill whatever the suggestions, scroll lock, and message bar rows leave
	ImGui::BeginChild("chatScroll", ImVec2(0, -(ImGui::GetFrameHeightWithSpacing() * 2 + suggestionHeight)));
	ImGui::PushTextWrapPos(0.0f);

	//Unformatted, since a message is whatever someone typed, percent signs included
	for (const std::string& message : messages)
		ImGui::TextUnformatted(message.c_str());

	if(scrollLock)
		ImGui::SetScrollY(ImGui::GetScrollMaxY());

	ImGui::PopTextWrapPos();
	ImGui::EndChild();

	if (suggestionRows > 0)
	{
		//Every command that matches what's typed, the one the arrows picked highlighted
		ImGui::BeginChild("chatSuggestions", ImVec2(0, suggestionHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float lineHeight = ImGui::GetTextLineHeightWithSpacing();
		for (size_t a = 0; a < candidates.size(); a++)
		{
			bool selected = (int)a == selectedCandidate;
			if (selected)
			{
				ImVec2 topLeft = ImGui::GetCursorScreenPos();
				ImVec2 bottomRight = ImVec2(topLeft.x + ImGui::GetContentRegionAvail().x, topLeft.y + lineHeight);
				drawList->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Header));
			}
			ImGui::TextUnformatted(suggestions[candidates[a]].text.c_str());
			if (selected && scrollToSelected)
			{
				ImGui::SetScrollHereY();
				scrollToSelected = false;
			}
		}
		ImGui::EndChild();
	}

	ImGui::Checkbox("Scroll Lock", &scrollLock);

	if (focusRequested)
	{
		//The chat key's own letter is waiting to be typed this frame
		io->InputQueueCharacters.resize(0);
		ImGui::SetKeyboardFocusHere();
		focusRequested = false;
	}

	std::string hint = typing ? "Enter to send, Escape to cancel, / for commands" : "Press " + chatKeyName + " to chat";
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::BeginDisabled(!typing);
	//The arrow keys go to messageCallback to cycle through suggestions
	ImGuiInputTextFlags messageFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
	bool submitted = ImGui::InputTextWithHint("##Message", hint.c_str(), messageBuffer, sizeof(messageBuffer), messageFlags, messageCallback, this);
	bool active = ImGui::IsItemActive();
	ImGui::EndDisabled();

	//Anything typed since last frame may change which commands match, the list itself is drawn next frame
	if (typing && !submitted && lastSeenMessage != messageBuffer)
	{
		lastSeenMessage = messageBuffer;
		updateCandidates(messageBuffer);
	}

	if (clearAfterRelease && !typing)
	{
		messageBuffer[0] = 0;
		clearAfterRelease = false;
	}

	if (submitted)
	{
		//A command picked with the arrows ends in a space
		std::string message = messageBuffer;
		while (!message.empty() && message.back() == ' ')
			message.pop_back();

		if (!message.empty())
		{
			chatMessage = message;
			chatMessageWaiting = true;
		}
		messageBuffer[0] = 0;
		stopTyping(false);
	}
	else if (typing)
	{
		//The keyboard is handed over a frame or two after it's asked for, and once it's taken, losing it means something else was clicked
		if (active)
			focusFramesLeft = 0;
		else if (focusFramesLeft > 0)
			focusFramesLeft--;
		else
			stopTyping(false);
	}

	ImGui::End();
}

ChatWindow::ChatWindow()
{
	name = "Chat Window";
	hudWindow = true;
}

ChatWindow::~ChatWindow()
{

}

void ChatWindow::init()
{

}

//The chat key starts typing, unless something else already has the keyboard
void ChatWindow::handleInput(SDL_Event& e, std::shared_ptr<InputMap> input)
{
	SDL_Scancode chatKey = input->getKeyBind(OpenChatWindow);
	const char* keyName = SDL_GetScancodeName(chatKey);
	chatKeyName = keyName && keyName[0] ? keyName : "the chat key";

	if (e.type != SDL_KEYDOWN || e.key.repeat || e.key.keysym.scancode != chatKey)
		return;

	if (opened && !typing && !ImGui::GetIO().WantCaptureKeyboard)
		startTyping();
}
