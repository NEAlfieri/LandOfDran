#pragma once

#include "../NetTypes/NetType.h"
#include <memory>

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

/*
	Every object handed to Lua is a table, and scripts expect that table to be the same one each time they get the
	object, so a field they set on it (client.someValue = 72) is still there the next time they look it up
	The tables live in the Lua registry by type and net ID from their first push until the object is gone
	They're held strongly: a weak cache would drop a table, and what a script kept on it, as soon as no script held it
*/

//Pushes the object's table and returns true, or pushes nothing and returns false if it has none yet
bool pushCachedLuaObject(lua_State* L, SimObjectType type, netIDType id);

//Keeps the table on top of the stack as the object's table, leaves it on the stack
void cacheLuaObject(lua_State* L, SimObjectType type, netIDType id);

//For when the object is gone, a script still holding the table keeps it, it just isn't handed out again
void forgetLuaObject(lua_State* L, SimObjectType type, netIDType id);

//For when every object of a type is gone at once
void forgetLuaObjects(lua_State* L, SimObjectType type);

//__eq for tables that only have a type and an id, like bricks
int LUA_objectIdsEqual(lua_State* L);

//__gc for the ptr userdata, Lua frees the space but would never run the weak_ptr's destructor
template <typename T>
int LUA_collectWeakPtr(lua_State* L)
{
	std::weak_ptr<T>* ptr = (std::weak_ptr<T>*)lua_touserdata(L, 1);
	if (ptr)
		ptr->~weak_ptr();
	return 0;
}

//Puts a new weak_ptr userdata on top of the stack, gcMetatableName has to be different for each T
template <typename T>
void pushWeakPtrLua(lua_State* L, const std::weak_ptr<T>& target, const char* gcMetatableName)
{
	std::weak_ptr<T>* userdata = (std::weak_ptr<T>*)lua_newuserdata(L, sizeof(std::weak_ptr<T>));
	new(userdata) std::weak_ptr<T>(target);

	if (luaL_newmetatable(L, gcMetatableName))
	{
		lua_pushcfunction(L, LUA_collectWeakPtr<T>);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
}

//The weak_ptr in a table's ptr field, nullptr if there isn't one, the table at index has to be one made for a T
template <typename T>
std::weak_ptr<T>* getWeakPtrLua(lua_State* L, int index)
{
	if (!lua_istable(L, index))
		return nullptr;

	lua_pushstring(L, "ptr");
	lua_rawget(L, index < 0 ? index - 1 : index);
	std::weak_ptr<T>* ret = lua_isuserdata(L, -1) ? (std::weak_ptr<T>*)lua_touserdata(L, -1) : nullptr;
	lua_pop(L, 1);
	return ret;
}

/*
	__eq for tables with a ptr field, true if both are the same object
	Lua only asks when the two tables aren't the same table, which the cache makes rare: one a script kept from
	before its object was deleted, or one built by hand
*/
template <typename T>
int LUA_weakPtrsEqual(lua_State* L)
{
	bool equal = false;

	if (lua_gettop(L) == 2 && lua_istable(L, 1) && lua_istable(L, 2))
	{
		//Only tables of the same type hold the same kind of weak_ptr
		lua_pushstring(L, "type");
		lua_rawget(L, 1);
		lua_pushstring(L, "type");
		lua_rawget(L, 2);
		bool sameType = lua_isinteger(L, -1) && lua_isinteger(L, -2) && lua_tointeger(L, -1) == lua_tointeger(L, -2);
		lua_pop(L, 2);

		if (sameType)
		{
			std::weak_ptr<T>* a = getWeakPtrLua<T>(L, 1);
			std::weak_ptr<T>* b = getWeakPtrLua<T>(L, 2);
			equal = a && b && !a->owner_before(*b) && !b->owner_before(*a);
		}
	}

	lua_pushboolean(L, equal);
	return 1;
}
