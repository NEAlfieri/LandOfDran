#pragma once

#include "../LandOfDran.h"
#include "UserInterface.h"

/*
	Pinned to the top right corner while in game, where it can be resized and collapsed but not moved or closed
	The message bar only takes the keyboard after the chat key (OpenChatWindow, T by default), until Enter sends, Escape cancels, or something else is clicked
*/
class ChatWindow : public Window
{
	std::vector<std::string> messages;

	bool scrollLock = true;

	char messageBuffer[256] = { 0 };
	std::string chatMessage = "";
	bool chatMessageWaiting = false;

	//Typing a message in the message bar
	bool typing = false;

	//The message bar is given the keyboard on the next render, and gets this many frames to take it before typing gives up
	bool focusRequested = false;
	static constexpr int focusFrames = 5;
	int focusFramesLeft = 0;

	//Set by stopTyping, the keyboard is taken back from the message bar on the next render
	bool releaseFocus = false;

	//Set by stopTyping for a cancel, the message bar is emptied after it's drawn, since ImGui writes a message bar's text back into the buffer the frame it loses the keyboard
	bool clearAfterRelease = false;

	//What the chat key is bound to, for the hint in the message bar
	std::string chatKeyName = "T";

	//A slash command the server told us about, see addSuggestion
	struct Suggestion
	{
		//Lowercase, without the slash
		std::string command;
		//What the list shows for it, the command's full name and its arguments
		std::string text;
	};
	std::vector<Suggestion> suggestions;

	//Indices into suggestions that match the command being typed, see updateCandidates
	std::vector<size_t> candidates;

	//Which candidate the arrow keys have picked, -1 for none
	int selectedCandidate = -1;

	//The list scrolls to the picked candidate on the next render
	bool scrollToSelected = false;

	//What the message bar held the last time candidates were worked out, so they're only redone when it changes
	std::string lastSeenMessage = "";

	//At most this many suggestions are listed at once, the rest scroll
	static constexpr int maxSuggestionRows = 6;

	//Works out candidates from a message: nothing unless it starts with a slash, every command that starts with what's typed, or just the one named once a space follows it
	void updateCandidates(const char* message);

	//Up or down arrow in the message bar: picks the next or previous candidate and writes it into the message bar, ready for arguments
	void pickCandidate(ImGuiInputTextCallbackData* data, int direction);

	static int messageCallback(ImGuiInputTextCallbackData* data);

public:
	//A command from the server's registerChatSuggestion, registering the same command again replaces its text
	void addSuggestion(const std::string& command, const std::string& text);

	//Forgets every suggestion, for leaving a server
	void clearSuggestions();

	void addMessage(const std::string &message);

	ChatWindow();
	~ChatWindow();

	bool hasChatMessage() const { return chatMessageWaiting; }

	//Resets hasChatMessage
	std::string getChatMessage() { chatMessageWaiting = false; return chatMessage; }

	//Puts the keyboard in the message bar, expanding the window if it's collapsed
	void startTyping();

	//Gives the keyboard back, throwing away what was typed if cancel
	void stopTyping(bool cancel);

	bool isTyping() const { return typing; }

	virtual void render(ImGuiIO* io) override;
	virtual void init() override;
	virtual void handleInput(SDL_Event& e, std::shared_ptr<InputMap> input) override;
};

