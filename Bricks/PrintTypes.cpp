#include "PrintTypes.h"

#include "../Networking/Server.h"

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

void PrintTypes::load(const std::string& printsFolder)
{
	scope("PrintTypes::load");

	unsigned int startMS = SDL_GetTicks();

	prints.clear();
	byName.clear();

	std::error_code errorCode;
	if (!std::filesystem::is_directory(printsFolder, errorCode))
	{
		info("No prints folder at " + printsFolder);
		return;
	}

	for (std::filesystem::recursive_directory_iterator iterator(printsFolder, errorCode), end; iterator != end; iterator.increment(errorCode))
	{
		const std::filesystem::path& file = iterator->path();
		std::string extension = lowercase(file.extension().string());
		if (!iterator->is_regular_file(errorCode) || (extension != ".png" && extension != ".webm"))
			continue;

		//Each add-on keeps the prints themselves in a prints folder and their icons in an icons folder next to it
		if (lowercase(file.parent_path().filename().string()) != "prints")
			continue;

		prints.push_back({ printName(file), file.generic_string(), extension == ".webm", -1 });
	}

	std::sort(prints.begin(), prints.end(), [](const PrintType& a, const PrintType& b) { return lowercase(a.name) < lowercase(b.name); });

	for (size_t a = 0; a < prints.size(); a++)
		byName[lowercase(prints[a].name)] = a;

	size_t videoCount = 0;
	for (const PrintType& print : prints)
		videoCount += print.video ? 1 : 0;

	info("Loaded " + std::to_string(prints.size()) + " print names (" + std::to_string(videoCount) + " of them videos) from " +
		printsFolder + " in " + std::to_string(SDL_GetTicks() - startMS) + "ms");
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
