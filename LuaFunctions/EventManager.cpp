#include "EventManager.h"

std::vector<LuaEvent> EventManager::events;

/*
    This is a good template for an event function that takes no args and returns nothing
    Needs to be expanded for arguments and return values
*/
void EventManager::callEvent(lua_State *L,std::string eventName,int numValues)
{
    scope("callEvent");

    //Maybe I should just have the event as a global variable...
    LuaEvent* event = 0;
    for (unsigned int a = 0; a < events.size(); a++)
    {
        if (events[a].name == eventName)
        {
            event = &events[a];
            break;
        }
    }

    if (!event)
    {
        error(eventName + " event dissapeared!");
        return;
    }

    if (lua_gettop(L) != numValues)
    {
        error("Lua stack size was not equal to numValues");
        return;
    }

    //For each event listener
    for(unsigned int a = 0; a<event->boundFunctions.size(); a++)
	{
        lua_getglobal(L, event->boundFunctions[a].c_str());
        if (!lua_isfunction(L, -1))
        {
            error("Function " + event->boundFunctions[a] + " was assigned to " + event->name + " but doesn't appear to be a valid function!");
            lua_pop(L, 1);
            continue;
        }

        /*
            Lua needs the function to call first on the stack
            But arguments to the function were pushed before callEvent was called
        */
        lua_insert(L, 1);

        //Actually call the Lua function
        if (lua_pcall(L, numValues, numValues, 0))
        {
            error("Error in lua call to event listener function " + event->boundFunctions[a] + " for event " + event->name);
            if (lua_gettop(L) > 0)
            {
                const char* err = lua_tostring(L, -1);
                if (!err)
                    continue;
                std::string errorstr = err;

                lua_settop(L, 0);

                error(errorstr);
            }
        }

        int rets = lua_gettop(L);
        if (rets != numValues)
        {
            error(event->name + " listener " + event->boundFunctions[a] + " was meant to return " + std::to_string(numValues) + " arguments, returned " + std::to_string(rets) + ". Will ignore return values!");
            lua_settop(L, 0);
            return;
        }
	}

    if (lua_gettop(L) != numValues)
    {
        error("Event listener for event " + eventName + " returned " + std::to_string(lua_gettop(L)) + " values was meant to return " + std::to_string(numValues));
        lua_settop(L, 0);
        return;
    }
}

int EventManager::getNumListeners(lua_State* L)
{
    scope("getNumListeners");

    int args = lua_gettop(L);
    if (args != 1)
    {
        error("requires 1 argument " + std::to_string(args) + " provided!");
        lua_pop(L, args);
        return 0;
    }

    const char* eventName = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (!eventName)
    {
        error("eventName invalid!");
        return 0;
    }

    for (unsigned int a = 0; a < events.size(); a++)
    {
        if (events[a].name == eventName)
        {
            lua_pushnumber(L,events[a].boundFunctions.size());
            return 1;
        }
    }

    error("Could not find event " + std::string(eventName));

    return 0;
}

int EventManager::getListenerIdx(lua_State* L)
{
    scope("getListenerIdx");

    int args = lua_gettop(L);
    if (args != 2)
    {
        error("requires 2 arguments " + std::to_string(args) + " provided!");
        lua_pop(L, args);
        return 0;
    }

    int idx = lua_tonumber(L, -1);
    lua_pop(L, 1);

    if(idx < 0)
	{
		error("Index invalid!");
		return 0;
	}

    const char* eventName = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (!eventName)
    {
        error("eventName invalid!");
        return 0;
    }

    for (unsigned int a = 0; a < events.size(); a++)
    {
        if (events[a].name == eventName)
        {
            if(idx >= events[a].boundFunctions.size())
			{
				error("Index out of bounds!");
				return 0;
			}

			lua_pushstring(L, events[a].boundFunctions[idx].c_str());
			return 1;
        }
    }

    error("Could not find event " + std::string(eventName));

    return 0;
}

int EventManager::registerEventListener(lua_State *L)
{
    scope("registerEventListener");

    int args = lua_gettop(L);
    if (args != 2)
    {
        error("requires 2 arguments " + std::to_string(args) + " provided!");
        lua_pop(L, args);
        return 0;
    }

    const char* funcName = lua_tostring(L, -1);
    lua_pop(L, 1);
    const char* eventName = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (!funcName)
    {
        error("funcName invalid");
        return 0;
    }

    if (!eventName)
    {
        error("eventName invalid!");
        return 0;
    }

    for (unsigned int a = 0; a < events.size(); a++)
    {
        if (events[a].name == eventName)
        {
            events[a].boundFunctions.push_back(funcName);
            return 0;
        }
    }

    error("Could not find event " + std::string(eventName));

    return 0;
}

int EventManager::unregisterEventListener(lua_State* L)
{
    scope("unregisterEventListener");

    int args = lua_gettop(L);
    if (args != 2)
    {
        error("requires 2 arguments " + std::to_string(args) + " provided!");
        lua_pop(L, args);
        return 0;
    }

    const char* funcName = lua_tostring(L, -1);
    lua_pop(L, 1);
    const char* eventName = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (!funcName)
    {
        error("funcName invalid");
        return 0;
    }

    if (!eventName)
    {
        error("eventName invalid!");
        return 0;
    }

    for (unsigned int a = 0; a < events.size(); a++)
    {
        if (events[a].name == eventName)
        {
            for (unsigned int b = 0; b < events[a].boundFunctions.size(); b++)
            {
                if (events[a].boundFunctions[b] == funcName)
                {
                    events[a].boundFunctions.erase(events[a].boundFunctions.begin() + b);
                    return 0;
                }
            }

            error("Function " + std::string(funcName) + " was not registered to " + std::string(eventName));

            return 0;
        }
    }

    error("Could not find event " + std::string(eventName));

    return 0;
}

EventManager::EventManager(lua_State* L)
{
    //events is static (Lua's C API needs plain function pointers, so the callbacks below can't be
    //bound to a particular instance) but only one EventManager is ever alive at a time - each new
    //one belongs to a brand new LoopServer/lua_State, so start with a clean slate rather than piling
    //another copy of these events (and whatever got registered to them) onto whatever came before.
    events.clear();

    events.push_back(LuaEvent("ClientJoin"));
    events.push_back(LuaEvent("ClientChat"));
    events.push_back(LuaEvent("ClientLeave"));
    events.push_back(LuaEvent("ClientClick"));

    lua_register(L, "registerEventListener", registerEventListener);
    lua_register(L, "unregisterEventListener", unregisterEventListener);
    lua_register(L, "getNumListeners", getNumListeners);
    lua_register(L, "getListenerIdx", getListenerIdx);
}

EventManager::~EventManager()
{

}
