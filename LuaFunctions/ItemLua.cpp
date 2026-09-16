#include "ItemLua.h"
#include "ClientLua.h"
#include "../Utility/FileFunctions.h"

#include <cmath>

//Pops an item off the top of the stack, logging an error with usage for anything that isn't one
static std::shared_ptr<Item> popItem(lua_State* L, const std::string& usage)
{
	std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->popLua(L);
	if (!dynamic)
	{
		error("Invalid item passed to " + usage + ", was it deleted already?");
		return nullptr;
	}

	if (dynamic->getKind() != DynamicKind_Item)
	{
		error(usage + " needs an item, not a plain dynamic");
		return nullptr;
	}

	return std::static_pointer_cast<Item>(dynamic);
}

//Pops a client off the top of the stack, logging an error with usage for anything that isn't one
static std::shared_ptr<ClientData> popClient(lua_State* L, const std::string& usage)
{
	std::shared_ptr<JoinedClient> joined = popClientLua(L);
	std::shared_ptr<ClientData> client = joined ? LUA_pd->getClient(joined) : nullptr;
	if (!client)
		error("Invalid client passed to " + usage);
	return client;
}

//A slot argument, 0 to inventorySize - 1, logging an error with usage and returning -1 for anything else
static int readSlot(lua_State* L, int index, const std::string& usage)
{
	if (!lua_isnumber(L, index))
	{
		error(usage + " needs a slot number");
		return -1;
	}

	lua_Number value = lua_tonumber(L, index);
	if (value != std::floor(value) || value < 0 || value >= inventorySize)
	{
		error(usage + " needs a slot from 0 to " + std::to_string(inventorySize - 1));
		return -1;
	}

	return (int)value;
}

//The ID of an item's animation by name, "swing" for the swing every item has, false if its type has none by that name
static bool findItemAnimation(const Item& item, const std::string& name, int& id)
{
	if (lowercase(name) == "swing")
	{
		id = itemSwingAnimation;
		return true;
	}

	id = item.getType()->getModel()->getAnimationID(name);
	return id != -1;
}

static int LUA_newItemType(lua_State* L)
{
	scope("(LUA) newItemType");

	if (lua_gettop(L) != 7)
	{
		error("Expected 7 arguments newItemType(scriptName,modelFilePath,scaleX,scaleY,scaleZ,uiName,iconPath)");
		return 0;
	}

	const char* scriptName = lua_tostring(L, 1);
	const char* modelFilePath = lua_tostring(L, 2);
	glm::vec3 scale(lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5));
	const char* uiName = lua_tostring(L, 6);
	const char* iconPath = lua_tostring(L, 7);

	if (!scriptName || !modelFilePath || !uiName || !iconPath)
	{
		error("newItemType needs strings for its script name, model file path, name, and icon path");
		return 0;
	}

	//Games load the icon from their own copy of the game folder
	std::string icon = iconPath;
	if (!icon.empty() && (icon.length() > 255 || !isPathInsideGameFolder(icon) || !std::filesystem::is_regular_file(icon)))
	{
		error("Item icon " + icon + " isn't a file in the game folder, " + std::string(scriptName) + " won't have one");
		icon = "";
	}

	auto type = std::make_shared<DynamicType>();
	type->serverSideLoad(modelFilePath, LUA_pd->dynamicTypes.size(), scale);
	type->scriptName = scriptName;
	type->isItemType = true;
	type->itemName = std::string(uiName).substr(0, 255);
	type->itemIconPath = icon;
	LUA_pd->dynamicTypes.push_back(type);
	LUA_pd->allNetTypes.push_back(type);

	info("Added new item type: " + std::string(scriptName));

	lua_pushinteger(L, type->getID());
	return 1;
}

static int LUA_setItemHand(lua_State* L)
{
	scope("(LUA) setItemHand");

	if (lua_gettop(L) != 7)
	{
		error("Expected 7 arguments setItemHand(typeID,gripX,gripY,gripZ,pitch,yaw,roll)");
		return 0;
	}

	lua_Integer typeID = lua_tointeger(L, 1);
	if (!lua_isnumber(L, 1) || typeID < 0 || typeID >= (lua_Integer)LUA_pd->dynamicTypes.size() || !LUA_pd->dynamicTypes[typeID]->isItemType)
	{
		error("setItemHand needs a type ID from newItemType");
		return 0;
	}

	float values[6];
	for (int a = 0; a < 6; a++)
	{
		values[a] = (float)lua_tonumber(L, a + 2);
		if (!lua_isnumber(L, a + 2) || !std::isfinite(values[a]))
		{
			error("setItemHand needs numbers for the grip and angles");
			return 0;
		}
	}

	std::shared_ptr<DynamicType> type = LUA_pd->dynamicTypes[typeID];
	type->handOffset = glm::vec3(values[0], values[1], values[2]);
	type->handRotation = glm::quat(glm::radians(glm::vec3(values[3], values[4], values[5])));

	return 0;
}

static int LUA_createItem(lua_State* L)
{
	scope("(LUA) createItem");

	if (lua_gettop(L) != 4)
	{
		error("Expected 4 arguments createItem(typeID,x,y,z)");
		return 0;
	}

	lua_Integer typeID = lua_tointeger(L, 1);
	if (!lua_isnumber(L, 1) || typeID < 0 || typeID >= (lua_Integer)LUA_pd->dynamicTypes.size() || !LUA_pd->dynamicTypes[typeID]->isItemType)
	{
		error("createItem needs a type ID from newItemType");
		return 0;
	}

	btVector3 position((btScalar)lua_tonumber(L, 2), (btScalar)lua_tonumber(L, 3), (btScalar)lua_tonumber(L, 4));
	std::shared_ptr<Item> item = LUA_pd->dynamics->createDerived<Item>(LUA_pd->dynamicTypes[typeID], position, btQuaternion::getIdentity());

	LUA_pd->dynamics->pushLua(L, item);
	return 1;
}

static int LUA_getNumItems(lua_State* L)
{
	int count = 0;
	for (unsigned int a = 0; a < LUA_pd->dynamics->size(); a++)
	{
		if (LUA_pd->dynamics->get(a)->getKind() == DynamicKind_Item)
			count++;
	}

	lua_pushinteger(L, count);
	return 1;
}

static int LUA_getItemIdx(lua_State* L)
{
	scope("(LUA) getItemIdx");

	if (lua_gettop(L) != 1 || !lua_isnumber(L, 1))
	{
		error("Expected 1 argument getItemIdx(index)");
		return 0;
	}

	lua_Integer index = lua_tointeger(L, 1);
	for (unsigned int a = 0; a < LUA_pd->dynamics->size() && index >= 0; a++)
	{
		std::shared_ptr<Dynamic> dynamic = LUA_pd->dynamics->get(a);
		if (dynamic->getKind() != DynamicKind_Item)
			continue;

		if (index == 0)
		{
			LUA_pd->dynamics->pushLua(L, dynamic);
			return 1;
		}
		index--;
	}

	error("Item index out of range");
	return 0;
}

static int LUA_itemIsHeld(lua_State* L)
{
	scope("(LUA) item:isHeld");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:isHeld()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:isHeld()");
	if (!item)
		return 0;

	lua_pushboolean(L, item->isHeld());
	return 1;
}

static int LUA_itemGetHolder(lua_State* L)
{
	scope("(LUA) item:getHolder");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:getHolder()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:getHolder()");
	if (!item)
		return 0;

	std::shared_ptr<ClientData> carrier = item->owner.lock();
	if (carrier && carrier->client)
		pushClientLua(L, carrier->client);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_itemGetSlot(lua_State* L)
{
	scope("(LUA) item:getSlot");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:getSlot()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:getSlot()");
	if (!item)
		return 0;

	if (item->isHeld())
		lua_pushinteger(L, item->slot);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_itemIsEquipped(lua_State* L)
{
	scope("(LUA) item:isEquipped");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:isEquipped()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:isEquipped()");
	if (!item)
		return 0;

	lua_pushboolean(L, item->isEquipped());
	return 1;
}

static int LUA_itemPlayAnimation(lua_State* L)
{
	scope("(LUA) item:playAnimation");

	int args = lua_gettop(L);
	if (args != 2 && args != 3)
	{
		error("Expected item:playAnimation(name[,loop])");
		return 0;
	}

	const char* name = lua_tostring(L, 2);
	bool loop = args == 3 && lua_toboolean(L, 3);
	lua_settop(L, 1);

	std::shared_ptr<Item> item = popItem(L, "item:playAnimation(name[,loop])");
	if (!item)
		return 0;

	int id;
	if (!name || !findItemAnimation(*item, name, id))
	{
		error("Item type " + item->getType()->scriptName + " has no animation named " + std::string(name ? name : "nil") + ", just swing and any addAnimation gave it");
		return 0;
	}

	item->playAnimation(id, loop);
	LUA_pd->markItemChanged(item);
	return 0;
}

static int LUA_itemStopAnimation(lua_State* L)
{
	scope("(LUA) item:stopAnimation");

	int args = lua_gettop(L);
	if (args != 1 && args != 2)
	{
		error("Expected item:stopAnimation([name])");
		return 0;
	}

	const char* name = args == 2 ? lua_tostring(L, 2) : nullptr;
	lua_settop(L, 1);

	std::shared_ptr<Item> item = popItem(L, "item:stopAnimation([name])");
	if (!item)
		return 0;

	int id = itemNoAnimation;
	if (name && !findItemAnimation(*item, name, id))
	{
		error("Item type " + item->getType()->scriptName + " has no animation named " + std::string(name) + ", just swing and any addAnimation gave it");
		return 0;
	}

	int looping = item->loopAnimation;
	item->stopAnimation(id);
	if (item->loopAnimation != looping)
		LUA_pd->markItemChanged(item);
	return 0;
}

static int LUA_itemGetItemName(lua_State* L)
{
	scope("(LUA) item:getItemName");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:getItemName()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:getItemName()");
	if (!item)
		return 0;

	lua_pushstring(L, item->getType()->itemName.c_str());
	return 1;
}

static int LUA_itemGetTypeName(lua_State* L)
{
	scope("(LUA) item:getTypeName");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument item:getTypeName()");
		return 0;
	}

	std::shared_ptr<Item> item = popItem(L, "item:getTypeName()");
	if (!item)
		return 0;

	lua_pushstring(L, item->getType()->scriptName.c_str());
	return 1;
}

static int LUA_clientAddItem(lua_State* L)
{
	scope("(LUA) client:addItem");

	int args = lua_gettop(L);
	if (args != 2 && args != 3)
	{
		error("Expected client:addItem(item[,slot])");
		return 0;
	}

	int slot = -1;
	if (args == 3)
	{
		slot = readSlot(L, 3, "client:addItem(item[,slot])");
		if (slot == -1)
			return 0;
	}
	lua_settop(L, 2);

	std::shared_ptr<Item> item = popItem(L, "client:addItem(item[,slot])");
	if (!item)
		return 0;

	std::shared_ptr<ClientData> client = popClient(L, "client:addItem(item[,slot])");
	if (!client)
		return 0;

	if (item->isHeld())
	{
		error("client:addItem was given an item that's already in someone's inventory, take it out with client:removeItem first");
		lua_pushnil(L);
		return 1;
	}

	int added = client->addItem(LUA_pd, item, slot);
	if (added == -1)
		lua_pushnil(L);
	else
		lua_pushinteger(L, added);
	return 1;
}

static int LUA_clientRemoveItem(lua_State* L)
{
	scope("(LUA) client:removeItem");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:removeItem(slot)");
		return 0;
	}

	int slot = readSlot(L, 2, "client:removeItem(slot)");
	if (slot == -1)
		return 0;
	lua_settop(L, 1);

	std::shared_ptr<ClientData> client = popClient(L, "client:removeItem(slot)");
	if (!client)
		return 0;

	std::shared_ptr<Item> item = client->removeItem(LUA_pd, slot);
	if (item)
		LUA_pd->dynamics->pushLua(L, item);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_clientGetItem(lua_State* L)
{
	scope("(LUA) client:getItem");

	if (lua_gettop(L) != 2)
	{
		error("Expected 2 arguments client:getItem(slot)");
		return 0;
	}

	int slot = readSlot(L, 2, "client:getItem(slot)");
	if (slot == -1)
		return 0;
	lua_settop(L, 1);

	std::shared_ptr<ClientData> client = popClient(L, "client:getItem(slot)");
	if (!client)
		return 0;

	std::shared_ptr<Item> item = client->inventory[slot].lock();
	if (item)
		LUA_pd->dynamics->pushLua(L, item);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_clientSetHandItem(lua_State* L)
{
	scope("(LUA) client:setHandItem");

	int args = lua_gettop(L);
	if (args != 1 && args != 2)
	{
		error("Expected client:setHandItem(item or nil)");
		lua_settop(L, 0);
		return 0;
	}

	std::shared_ptr<Item> item = nullptr;
	if (args == 2 && !lua_isnil(L, 2))
	{
		item = popItem(L, "client:setHandItem(item or nil)");
		if (!item)
			return 0;
	}
	lua_settop(L, 1);

	std::shared_ptr<ClientData> client = popClient(L, "client:setHandItem(item or nil)");
	if (!client)
		return 0;

	if (item && item->isHeld())
	{
		error("client:setHandItem was given an item that's already in someone's inventory, take it out with client:removeItem first");
		lua_pushnil(L);
		return 1;
	}

	std::shared_ptr<Item> previous = client->setHandItem(LUA_pd, item);
	if (previous && previous != item)
		LUA_pd->dynamics->pushLua(L, previous);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_clientGetHandItem(lua_State* L)
{
	scope("(LUA) client:getHandItem");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getHandItem()");
		return 0;
	}

	std::shared_ptr<ClientData> client = popClient(L, "client:getHandItem()");
	if (!client)
		return 0;

	std::shared_ptr<Item> item = client->handItem.lock();
	if (item)
		LUA_pd->dynamics->pushLua(L, item);
	else
		lua_pushnil(L);
	return 1;
}

static int LUA_clientGetSelectedSlot(lua_State* L)
{
	scope("(LUA) client:getSelectedSlot");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getSelectedSlot()");
		return 0;
	}

	std::shared_ptr<ClientData> client = popClient(L, "client:getSelectedSlot()");
	if (!client)
		return 0;

	lua_pushinteger(L, client->selectedSlot);
	lua_pushboolean(L, client->inventoryOpen);
	return 2;
}

static int LUA_clientGetHeldItem(lua_State* L)
{
	scope("(LUA) client:getHeldItem");

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument client:getHeldItem()");
		return 0;
	}

	std::shared_ptr<ClientData> client = popClient(L, "client:getHeldItem()");
	if (!client)
		return 0;

	std::shared_ptr<Item> item = client->getHeldItem();

	if (item)
		LUA_pd->dynamics->pushLua(L, item);
	else
		lua_pushnil(L);
	return 1;
}

//Shared by getCameraPosition and getCameraDirection, which go by what the client last sent with their movement
static int pushCameraVector(lua_State* L, bool direction)
{
	std::string usage = direction ? "client:getCameraDirection()" : "client:getCameraPosition()";

	if (lua_gettop(L) != 1)
	{
		error("Expected 1 argument " + usage);
		return 0;
	}

	std::shared_ptr<ClientData> client = popClient(L, usage);
	if (!client)
		return 0;

	if (client->controllers.empty())
	{
		error(usage + " needs client:setDefaultController to have been called first");
		return 0;
	}

	glm::vec3 vector = direction ? client->controllers[0].lastCameraDirection : client->controllers[0].lastCameraPosition;
	lua_pushnumber(L, vector.x);
	lua_pushnumber(L, vector.y);
	lua_pushnumber(L, vector.z);
	return 3;
}

static int LUA_clientGetCameraPosition(lua_State* L)
{
	scope("(LUA) client:getCameraPosition");
	return pushCameraVector(L, false);
}

static int LUA_clientGetCameraDirection(lua_State* L)
{
	scope("(LUA) client:getCameraDirection");
	return pushCameraVector(L, true);
}

void registerItemFunctions(lua_State* L)
{
	lua_register(L, "newItemType", LUA_newItemType);
	lua_register(L, "setItemHand", LUA_setItemHand);
	lua_register(L, "createItem", LUA_createItem);
	lua_register(L, "getNumItems", LUA_getNumItems);
	lua_register(L, "getItemIdx", LUA_getItemIdx);

	//Items can do anything a dynamic can, so their metatable starts as a copy of the dynamic one
	luaL_newmetatable(L, "metatable_item");
	lua_getglobal(L, "metatable_dynamic");
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		//The item metatable has its own name and index
		if (lua_type(L, -2) == LUA_TSTRING)
		{
			std::string key = lua_tostring(L, -2);
			if (key == "__index" || key == "__name")
			{
				lua_pop(L, 1);
				continue;
			}
		}

		//Metatable, dynamic metatable, key, key, value
		lua_pushvalue(L, -2);
		lua_insert(L, -2);
		lua_settable(L, -5);
	}
	lua_pop(L, 1);

	luaL_Reg itemRegs[] = {
		{ "isHeld", LUA_itemIsHeld },
		{ "getHolder", LUA_itemGetHolder },
		{ "getSlot", LUA_itemGetSlot },
		{ "isEquipped", LUA_itemIsEquipped },
		{ "playAnimation", LUA_itemPlayAnimation },
		{ "stopAnimation", LUA_itemStopAnimation },
		{ "getItemName", LUA_itemGetItemName },
		{ "getTypeName", LUA_itemGetTypeName },
		{ NULL, NULL }
	};

	luaL_setfuncs(L, itemRegs, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "metatable_item");

	luaL_Reg clientRegs[] = {
		{ "addItem", LUA_clientAddItem },
		{ "removeItem", LUA_clientRemoveItem },
		{ "getItem", LUA_clientGetItem },
		{ "getSelectedSlot", LUA_clientGetSelectedSlot },
		{ "getHeldItem", LUA_clientGetHeldItem },
		{ "setHandItem", LUA_clientSetHandItem },
		{ "getHandItem", LUA_clientGetHandItem },
		{ "getCameraPosition", LUA_clientGetCameraPosition },
		{ "getCameraDirection", LUA_clientGetCameraDirection },
		{ NULL, NULL }
	};

	lua_getglobal(L, "metatable_client");
	luaL_setfuncs(L, clientRegs, 0);
	lua_pop(L, 1);
}
