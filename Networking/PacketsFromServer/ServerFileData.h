#pragma once

#include "../HeldServerPacket.h"

/*
	Part of one of the add-on files we asked the server for, see Networking/ServerFiles.h
	Written into the download folder once all of it is here, see ContentFiles::takeChunk
*/
struct ServerFileDataPacket : public HeldServerPacket
{
	//Returns true if the packet can be discarded (it was applied or it expired)
	virtual bool applyPacket(const ClientProgramData& pd, Simulation& simulation, const ExecutableArguments& cmdArgs) override;

	ServerFileDataPacket(unsigned int holdTime, ENetPacket* _packet);
	~ServerFileDataPacket();
};
