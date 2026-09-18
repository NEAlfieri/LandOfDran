#include "LuaObjectCache.h"
#include <initializer_list>

//Registry key of the table of per-type tables
static const char* cacheName = "LandOfDran_objectTables";

//Pushes the table of one type's object tables, by net ID, making it if create is set, returns false with nothing pushed if there is none
static bool pushTypeCache(lua_State* L, SimObjectType type, bool create)
{
	if (lua_getfield(L, LUA_REGISTRYINDEX, cacheName) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		if (!create)
			return false;

		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, cacheName);
	}

	if (lua_rawgeti(L, -1, (lua_Integer)type) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		if (!create)
		{
			lua_pop(L, 1);
			return false;
		}

		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_rawseti(L, -3, (lua_Integer)type);
	}

	//Just the type's table
	lua_remove(L, -2);
	return true;
}

bool pushCachedLuaObject(lua_State* L, SimObjectType type, netIDType id)
{
	if (!pushTypeCache(L, type, false))
		return false;

	if (lua_rawgeti(L, -1, (lua_Integer)id) != LUA_TTABLE)
	{
		lua_pop(L, 2);
		return false;
	}

	lua_remove(L, -2);
	return true;
}

void cacheLuaObject(lua_State* L, SimObjectType type, netIDType id)
{
	//Object table, type's table, object table
	pushTypeCache(L, type, true);
	lua_pushvalue(L, -2);
	lua_rawseti(L, -2, (lua_Integer)id);
	lua_pop(L, 1);
}

void forgetLuaObject(lua_State* L, SimObjectType type, netIDType id)
{
	if (!L || !pushTypeCache(L, type, false))
		return;

	lua_pushnil(L);
	lua_rawseti(L, -2, (lua_Integer)id);
	lua_pop(L, 1);
}

void forgetLuaObjects(lua_State* L, SimObjectType type)
{
	if (!L)
		return;

	if (lua_getfield(L, LUA_REGISTRYINDEX, cacheName) == LUA_TTABLE)
	{
		lua_pushnil(L);
		lua_rawseti(L, -2, (lua_Integer)type);
	}
	lua_pop(L, 1);
}

int LUA_objectIdsEqual(lua_State* L)
{
	bool equal = false;

	if (lua_gettop(L) == 2 && lua_istable(L, 1) && lua_istable(L, 2))
	{
		equal = true;
		for (const char* field : { "type", "id" })
		{
			lua_pushstring(L, field);
			lua_rawget(L, 1);
			lua_pushstring(L, field);
			lua_rawget(L, 2);
			if (!lua_isinteger(L, -1) || !lua_isinteger(L, -2) || lua_tointeger(L, -1) != lua_tointeger(L, -2))
				equal = false;
			lua_pop(L, 2);
		}
	}

	lua_pushboolean(L, equal);
	return 1;
}
