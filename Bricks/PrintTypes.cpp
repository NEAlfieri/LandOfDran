#include "PrintTypes.h"

#include "../Networking/Server.h"
#include "../Utility/ContentFiles.h"

#include <filesystem>

//Same as the brick record packets in BrickHolder.cpp: packet type and a u16 count, in one MTU
static constexpr unsigned int packetHeaderBytes = 3;
static constexpr unsigned int maxPacketBytes = ENET_HOST_DEFAULT_MTU - 20;

/*
	"Assets/brick/prints/Print_2x2f_Default/prints/arrow.png" becomes "2x2f/arrow", the same name the old
	game's printLoader made and the one Blockland saves use, so a .bls with "Letters/X" finds our letter X
*/
static std::string printName(const std::filesystem::path& file)
{
	std::string group = file.parent_path().parent_path().filename().string();

	if (lowercase(group).compare(0, 6, "print_") == 0)
		group = group.substr(6);

	//An add-on's own prints are named after it, its default ones just after the brick they go on
	if (group.length() > 8 && lowercase(group).compare(group.length() - 8, 8, "_default") == 0)
		group = group.substr(0, group.length() - 8);

	return group + "/" + file.stem().string();
}

/*
	The print menu's button for a print, from the icons folder each add-on keeps next to its prints one:
	"Assets/brick/prints/Print_2x2f_Default/prints/arrow.png" becomes ".../icons/arrow.png"
	"" if the add-on didn't ship one, like for the videos
*/
static std::string printIconPath(const std::filesystem::path& file)
{
	std::filesystem::path icon = file.parent_path().parent_path() / "icons" / (file.stem().string() + ".png");

	std::error_code errorCode;
	return std::filesystem::is_regular_file(icon, errorCode) ? icon.generic_string() : "";
}

void PrintTypes::load(const std::string& printsFolder, const std::string& addOnsFolder)
{
	scope("PrintTypes::load");

	unsigned int startMS = SDL_GetTicks();

	prints.clear();
	byName.clear();

	std::error_code errorCode;

	//Names we already have, so a print of our own isn't listed twice by a server's copy of it
	std::set<std::string> found;

	auto lookThrough = [&](const std::string& folder)
	{
		if (!std::filesystem::is_directory(folder, errorCode))
			return;

		for (std::filesystem::recursive_directory_iterator iterator(folder, errorCode), end; iterator != end; iterator.increment(errorCode))
		{
			const std::filesystem::path& file = iterator->path();
			std::string extension = lowercase(file.extension().string());
			if (!iterator->is_regular_file(errorCode) || (extension != ".png" && extension != ".webm"))
				continue;

			//Each add-on keeps the prints themselves in a prints folder and their icons in an icons folder next to it
			if (lowercase(file.parent_path().filename().string()) != "prints")
				continue;

			std::string name = printName(file);
			if (!found.insert(lowercase(name)).second)
				continue;

			prints.push_back({ name, file.generic_string(), printIconPath(file), extension == ".webm", -1 });
		}
	};

	/*
		The game's own prints, then any an add-on brought - a print pack folder dropped in Add-ons is found
		here the same way, since what makes a print is a prints folder with images in it, whatever sits above

		Then the same two out of the download folder, for prints a server sent us: those are looked for
		rather than loaded by a path a packet names, so that folder has to be gone through the same way
	*/
	lookThrough(printsFolder);
	lookThrough(addOnsFolder);
	lookThrough(contentDownloadFolder + printsFolder);
	lookThrough(contentDownloadFolder + addOnsFolder);

	if (prints.empty() && !std::filesystem::is_directory(printsFolder, errorCode))
	{
		info("No prints folder at " + printsFolder);
		return;
	}

	std::sort(prints.begin(), prints.end(), [](const PrintType& a, const PrintType& b) { return lowercase(a.name) < lowercase(b.name); });

	for (size_t a = 0; a < prints.size(); a++)
		byName[lowercase(prints[a].name)] = a;

	size_t videoCount = 0;
	for (const PrintType& print : prints)
		videoCount += print.video ? 1 : 0;

	info("Loaded " + std::to_string(prints.size()) + " print names (" + std::to_string(videoCount) + " of them videos) from " +
		printsFolder + " and " + addOnsFolder + " in " + std::to_string(SDL_GetTicks() - startMS) + "ms");
}

int PrintTypes::find(const std::string& name) const
{
	auto found = byName.find(lowercase(name));
	return found == byName.end() ? -1 : (int)found->second;
}

std::vector<std::string> PrintTypes::getNames() const
{
	std::vector<std::string> names;
	names.reserve(prints.size());
	for (const PrintType& print : prints)
		names.push_back(print.name);
	return names;
}

void PrintTypes::sendTypes(JoinedClient const* client) const
{
	//Packet type, u16 count, then a u16 print ID, a name length byte, and the name for each, see BrickHolder::sendSpecialTypes
	std::vector<unsigned char> bytes;
	uint16_t count = 0;

	auto flush = [&]()
	{
		if (count == 0)
			return;

		bytes[0] = BrickPrintTypes;
		memcpy(bytes.data() + 1, &count, sizeof(uint16_t));
		client->send(enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation)), JoinNegotiation);
		count = 0;
	};

	size_t printCount = std::min<size_t>(prints.size(), 65534);
	for (size_t a = 0; a < printCount; a++)
	{
		std::string name = prints[a].name.substr(0, 255);
		if (count > 0 && bytes.size() + 3 + name.length() > maxPacketBytes)
			flush();

		if (count == 0)
			bytes.assign(packetHeaderBytes, 0);

		uint16_t printID = (uint16_t)(a + 1);
		bytes.resize(bytes.size() + sizeof(uint16_t));
		memcpy(bytes.data() + bytes.size() - sizeof(uint16_t), &printID, sizeof(uint16_t));
		bytes.push_back((unsigned char)name.length());
		bytes.insert(bytes.end(), name.begin(), name.end());
		count++;
	}

	flush();
}
