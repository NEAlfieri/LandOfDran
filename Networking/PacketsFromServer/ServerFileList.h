#pragma once

#include "../HeldServerPacket.h"

/*
	The add-on files the server we're joining can send us, see Networking/ServerFiles.h
	The client answers the finished list with a ServerFileRequest, see LoopClient::run
*/
struct ServerFileListPacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	ServerFileListPacket(unsigned int holdTime, ENetPacket* _packet);
	~ServerFileListPacket();
};
