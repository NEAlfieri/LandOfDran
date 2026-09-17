#pragma once

#include "../../LandOfDran.h"
#include "../HeldServerPacket.h"

//A slash command the chat window can suggest while typing one, see ChatWindow::addSuggestion
class ChatSuggestionPacket : public HeldServerPacket
{
public:

	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	ChatSuggestionPacket(unsigned int holdTime, ENetPacket* _packet);
	~ChatSuggestionPacket();
};
