#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"

/*
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void parseEvalCommand(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	//Invalid empty packet
	if (packet->dataLength < 4)
		return;

	bool trustedLocal = pd->isTrustedLocalAdmin(source);

	if (!pd->useEvalPassword && !trustedLocal)
		return;

	//Is the client logged in?
	if (!source->isAdmin)
	{
		error("Client " + source->name + " tried to do an eval statement without being logged in somehow.");
		return;
	}

	//What command would they like to run?
	unsigned short commandLength = 0;
	memcpy(&commandLength,packet->data + 1,sizeof(unsigned short));

	if (packet->dataLength < 3 + commandLength)
		return;

	std::string command = std::string((char*)packet->data + 3, commandLength);

	//Get the password

	unsigned int passwordLength = packet->data[3 + commandLength];

	if (packet->dataLength < 4 + commandLength + passwordLength)
		return;

	std::string password = std::string((char*)packet->data + 4 + commandLength, passwordLength);

	if (!trustedLocal && password != pd->evalPassword)
	{
		error("Client " + source->name + " tried to do an eval statement with the wrong password.");
		return;
	}

	//Run the command

	info("INPUT " + command);

	if (luaL_dostring(pd->luaState, command.c_str()) != LUA_OK)
	{
		//Lua syntax error
		if (lua_gettop(pd->luaState) > 0)
		{
			//There's an error string on the top of Lua's stack
			const char* errorString = lua_tostring(pd->luaState, -1);
			if (!errorString)
				return;

			if(lua_gettop(pd->luaState) > 0)
				lua_pop(pd->luaState, lua_gettop(pd->luaState));

			error(std::string(errorString));
		}
	}
}
