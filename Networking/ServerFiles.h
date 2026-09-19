#pragma once

#include "../LandOfDran.h"
#include "../Utility/ContentFiles.h"
#include "JoinedClient.h"

struct ServerProgramData;
struct ClientData;

/*
	The files this server offers to clients that don't have them, see Lua's addServerFile

	A file is read once, when it's registered, for its size and checksum: a server that changes one
	of its own add-on files while it runs has to register it again for clients to hear about the change
*/

//Adds one file to what joining clients are offered, false with the reason in problem if it can't be offered
bool addServerFile(ServerProgramData* pd, const std::string& path, std::string& problem);

//Everything on offer, sent to a client as it joins. Sent even when nothing is on offer, since clients
//answer the list before the server sends them anything that could need a file
void sendServerFileList(const ServerProgramData* pd, JoinedClient* source);

/*
	The files a client asked for go out on the join channel, so every byte of them is there before the
	types that use them, which are sent the moment the last batch has gone

	They go a batch at a time rather than all at once: a server can offer a hundred megabytes, and queueing
	all of that for each joining client would hold it all in memory as ENet packets. The client asks for the
	next batch once it has taken the last one, which paces the whole thing to whatever it can actually take,
	see ClientData::fileSend and Networking/PacketsFromClient/ServerFileRequest.cpp
*/

//How much of a client's files goes out before waiting to hear that they took it
constexpr size_t serverFileBatchBytes = 512 * 1024;

//Begins the transfer and sends the first batch. True if there was nothing to send, so the join can carry on
bool startServerFiles(const ServerProgramData* pd, JoinedClient* source, ClientData* client, const std::vector<uint16_t>& ids);

//Sends the next batch. True once the last of it has gone out
bool sendNextServerFileBatch(const ServerProgramData* pd, JoinedClient* source, ClientData* client);
