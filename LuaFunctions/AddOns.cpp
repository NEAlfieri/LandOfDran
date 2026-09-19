#include "AddOns.h"
#include "../Utility/ZipFile.h"
#include "../Utility/FileFunctions.h"

#include <fstream>

//Where add-ons live, and the config file in it that says which of them the server runs
static const std::string addOnsFolder = "Add-ons";
static const std::string addOnsList = "Add-ons/list.txt";

//The one file an add-on is loaded by, whatever else it carries
static const std::string entryScript = "server.lua";

//The names of the add-ons this Lua state has run, kept in its registry so two servers in one program
//(single player and the menu demo) don't share a list between them
static const char* loadedAddOnsKey = "LOD_loadedAddOns";

//One line of list.txt
struct AddOnSetting
{
	std::string name = "";
	bool enabled = true;
};

//Add-on names are folder names, so anything that would point at another folder is refused
static bool isAddOnName(const std::string& name)
{
	if (name.length() < 1 || name.length() > 128)
		return false;

	if (name == "." || name == ".." || name.find('/') != std::string::npos ||
		name.find('\\') != std::string::npos || name.find(':') != std::string::npos)
		return false;

	return true;
}

static std::string addOnFolder(const std::string& name)
{
	return addOnsFolder + "/" + name;
}

//Whether this add-on is one the game runs at all, rather than a folder of content like a print pack
static bool hasEntryScript(const std::string& name)
{
	std::error_code trouble;
	return std::filesystem::is_regular_file(addOnFolder(name) + "/" + entryScript, trouble);
}

//The table of add-ons this state has loaded, left on the top of the stack
static void pushLoadedAddOns(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, loadedAddOnsKey);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, loadedAddOnsKey);
	}
}

static bool isAddOnLoaded(lua_State* L, const std::string& name)
{
	pushLoadedAddOns(L);
	lua_getfield(L, -1, name.c_str());
	bool loaded = lua_toboolean(L, -1) != 0;
	lua_pop(L, 2);
	return loaded;
}

static void markAddOnLoaded(lua_State* L, const std::string& name)
{
	pushLoadedAddOns(L);
	lua_pushboolean(L, true);
	lua_setfield(L, -2, name.c_str());
	lua_pop(L, 1);
}

/*
	Runs one add-on's server.lua, if it hasn't been run already

	It's marked as loaded before it runs rather than after, so an add-on that requires something which
	requires it back loads once each rather than for ever. An add-on whose script fails is left marked
	too: whatever it had got through is in, and running it again on top of that only makes more of a mess
*/
static bool loadOneAddOn(lua_State* L, const std::string& name)
{
	scope("loadAddOn");

	if (isAddOnLoaded(L, name))
		return true;

	if (!isAddOnName(name))
	{
		error("\"" + name + "\" isn't an add-on name");
		return false;
	}

	std::error_code trouble;
	if (!std::filesystem::is_directory(addOnFolder(name), trouble))
	{
		error("No add-on called " + name + " in the " + addOnsFolder + " folder");
		return false;
	}

	if (!hasEntryScript(name))
	{
		error("The add-on " + name + " has no " + entryScript + " to run");
		return false;
	}

	markAddOnLoaded(L, name);

	info("Loading add-on " + name);

	std::string path = addOnFolder(name) + "/" + entryScript;
	if (luaL_dofile(L, path.c_str()))
	{
		error("Error loading " + path + ": " + std::string(lua_tostring(L, -1)));
		lua_pop(L, 1);
		return false;
	}

	return true;
}

/*
	requireAddOn("Weapon_Gun")

	Loads that add-on now if it isn't loaded yet, for an add-on that needs another one's types, sounds
	or functions to exist before its own script can run. The same as Torque's ForceRequiredAddOn: it
	loads whether or not list.txt has it enabled, since what asked for it can't work without it

	True if the add-on is loaded by the time this returns, false if there's no such add-on or its
	script failed
*/
static int LUA_requireAddOn(lua_State* L)
{
	scope("LUA_requireAddOn");

	int args = lua_gettop(L);

	if (args != 1 || !lua_isstring(L, 1))
	{
		error("Expected 1 string argument, an add-on name");
		lua_pop(L, args);
		lua_pushboolean(L, false);
		return 1;
	}

	std::string name = std::string(lua_tostring(L, 1));
	lua_pop(L, args);

	lua_pushboolean(L, loadOneAddOn(L, name));
	return 1;
}

//Unpacks every Add-ons/<name>.zip into Add-ons/<name>, and deletes the zip once it's out
static void installZippedAddOns()
{
	scope("installZippedAddOns");

	std::error_code trouble;
	std::vector<std::filesystem::path> zips;

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(addOnsFolder, trouble))
	{
		if (!entry.is_regular_file())
			continue;

		std::string extension = entry.path().extension().generic_string();
		for (char& letter : extension)
			letter = tolower(letter);

		if (extension == ".zip")
			zips.push_back(entry.path());
	}

	//Listed first and unpacked after: unpacking writes into the folder being walked
	for (const std::filesystem::path& zip : zips)
	{
		std::string name = zip.stem().generic_string();
		if (!isAddOnName(name))
		{
			error("Can't unpack " + zip.generic_string() + ": \"" + name + "\" isn't an add-on name");
			continue;
		}

		std::string folder = addOnFolder(name);
		if (std::filesystem::exists(folder, trouble))
		{
			error("Not unpacking " + zip.generic_string() + ": " + folder + " is already there. Delete one or the other");
			continue;
		}

		std::string problem = "";
		if (!extractZipFile(zip.generic_string(), folder, problem))
		{
			error("Could not unpack " + zip.generic_string() + ": " + problem);
			continue;
		}

		info("Unpacked the add-on " + name + " out of " + zip.filename().generic_string());
		std::filesystem::remove(zip, trouble);
	}
}

//Reads list.txt as it is now. Lines are "<name> <true or false>", blank ones and # comments are passed over
static std::vector<AddOnSetting> readAddOnList()
{
	scope("readAddOnList");

	std::vector<AddOnSetting> settings;

	std::ifstream file(addOnsList);
	if (!file.is_open())
		return settings;

	std::string line = "";
	while (std::getline(file, line))
	{
		//Written on Windows, read on Linux
		while (line.length() > 0 && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.pop_back();

		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos)
			continue;

		line = line.substr(start);
		if (line[0] == '#' || line[0] == ';')
			continue;

		//An add-on name can have spaces in it, so the setting is the last word and the name is the rest
		size_t space = line.find_last_of(" \t");
		if (space == std::string::npos)
		{
			error("Skipping \"" + line + "\" in " + addOnsList + ": expected a name then true or false");
			continue;
		}

		AddOnSetting setting;
		setting.name = line.substr(0, space);
		while (setting.name.length() > 0 && (setting.name.back() == ' ' || setting.name.back() == '\t'))
			setting.name.pop_back();

		std::string value = line.substr(space + 1);
		for (char& letter : value)
			letter = tolower(letter);

		setting.enabled = value == "true" || value == "1" || value == "yes" || value == "on";

		if (!isAddOnName(setting.name))
		{
			error("Skipping \"" + setting.name + "\" in " + addOnsList + ": that isn't an add-on name");
			continue;
		}

		settings.push_back(setting);
	}

	return settings;
}

static void writeAddOnList(const std::vector<AddOnSetting>& settings)
{
	scope("writeAddOnList");

	std::string contents =
		"#Every add-on in this folder and whether this server loads it, see LuaAPI.md\n"
		"#They load in the order they're listed, and one dropped in the folder adds itself as true\n";

	for (const AddOnSetting& setting : settings)
		contents += setting.name + " " + (setting.enabled ? "true" : "false") + "\n";

	//Only written when it would say something different, so a server starting doesn't touch the file every time
	std::ifstream existing(addOnsList, std::ios::binary);
	if (existing.is_open())
	{
		std::string was((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
		if (was == contents)
			return;
	}
	existing.close();

	std::ofstream file(addOnsList, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		error("Could not write " + addOnsList);
		return;
	}

	file << contents;
}

void loadAddOns(lua_State* L)
{
	scope("loadAddOns");

	std::error_code trouble;
	if (!std::filesystem::is_directory(addOnsFolder, trouble))
	{
		info("No " + addOnsFolder + " folder, so no add-ons to load");
		return;
	}

	installZippedAddOns();

	//Every add-on folder that has something to run, in the order the file system lists them
	std::vector<std::string> present;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(addOnsFolder, trouble))
	{
		if (!entry.is_directory())
			continue;

		std::string name = entry.path().filename().generic_string();
		if (isAddOnName(name) && hasEntryScript(name))
			present.push_back(name);
	}
	std::sort(present.begin(), present.end());

	//What the file already says, minus anything whose folder has gone
	std::vector<AddOnSetting> settings;
	for (const AddOnSetting& setting : readAddOnList())
	{
		if (std::find(present.begin(), present.end(), setting.name) == present.end())
			continue;

		//A name listed twice is taken as it was first set
		bool already = false;
		for (const AddOnSetting& kept : settings)
			already = already || kept.name == setting.name;

		if (!already)
			settings.push_back(setting);
	}

	//Anything new in the folder is on unless somebody says otherwise
	for (const std::string& name : present)
	{
		bool listed = false;
		for (const AddOnSetting& setting : settings)
			listed = listed || setting.name == name;

		if (listed)
			continue;

		AddOnSetting setting;
		setting.name = name;
		setting.enabled = true;
		settings.push_back(setting);

		info("New add-on " + name + ", enabled in " + addOnsList);
	}

	writeAddOnList(settings);

	for (const AddOnSetting& setting : settings)
	{
		if (setting.enabled)
			loadOneAddOn(L, setting.name);
	}
}

void registerAddOnFunctions(lua_State* L)
{
	lua_register(L, "requireAddOn", LUA_requireAddOn);
}
