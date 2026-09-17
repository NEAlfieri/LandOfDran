#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"

/*	
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void chatMessageSent(JoinedClient * source, Server const * const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	//Invalid empty packet
	if (packet->dataLength < 2)
		return;

	//What message would they like to send?
	unsigned int messageLength = packet->data[1];

	if (packet->dataLength < 2 + messageLength)
		return;

	std::string message = std::string((char*)packet->data + 2, messageLength);

	//Slash commands are case-insensitive: the command word is lowercased before Lua compares it, its arguments are left as typed
	if (message.length() > 1 && message[0] == '/')
	{
		size_t commandEnd = message.find(' ');
		if (commandEnd == std::string::npos)
			commandEnd = message.length();
		for (size_t a = 1; a < commandEnd; a++)
			message[a] = (char)tolower((unsigned char)message[a]);
	}

	message = source->name + ": " + message;
	info(message.c_str());

	//Push arguments to event
	pushClientLua(pd->luaState, source->me);
	lua_pushstring(pd->luaState, message.c_str());
	//Trigger lua functions
	pd->eventManager->callEvent(pd->luaState, "ClientChat", 2);
	//Either return values will be correct, or they will be zero, do nothing special if lua functions messed up the event
	if (lua_gettop(pd->luaState) != 0)
	{
		if (!lua_isstring(pd->luaState, -1))
			error("First returned value for ClientChat event was not a string!");
		else
		{
			//Lua can alter client's chat messages
			const char* newMsg = lua_tostring(pd->luaState, -1);
			if(newMsg)
				message = std::string(newMsg);
			else
				error("First returned value for ClientChat event was invalid string");
		}

		lua_settop(pd->luaState, 0);
	}

	messageLength = message.length();

	//Lua may have deleted the chat message
	if (messageLength < 1)
		return;

	//Or made it too long
	if (messageLength > 255)
	{
		messageLength = 255;
		message = message.substr(0, 255);
	}

	server->broadcastChat(message);
}
