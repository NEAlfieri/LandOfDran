#include "BrickSaves.h"

#include <climits>
#include <sstream>

//The only place the JPEG writer is compiled, see drawBricksFromAbove; the client's preview includes the header without this
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../External/stb_image_write.h"

/*
	The "next version" (16483535) adds owner, name, and flag fields to every brick record
	Our own version after that (16483536) swaps its music track and light color for BrickAttachments' music loop, whole light, and emitter,
	and its material byte is a BrickMaterial (saves from before materials wrote 0 there, which is none)
	The one after (16483537) adds blink speed and strength to the end of the light, see BrickAttachments::lightFloatCount
	The one after that (16483538) puts a brick's print name at the very end of its record, see printFlag
	The one after that (16483539) adds a second flags byte after the first, for what a brick spawns, see BrickAttachments::getExtraFlags
	The one after that (16483540) puts who saved it, when, and a picture of the build between the version and the brick count, see LodSaveInfo
*/
static constexpr unsigned int lodMagic = 16483534;
static constexpr unsigned int lodMagicAttachments = lodMagic + 2;
static constexpr unsigned int lodMagicBlinkingLights = lodMagic + 3;
static constexpr unsigned int lodMagicPrints = lodMagic + 4;
static constexpr unsigned int lodMagicSpawns = lodMagic + 5;
static constexpr unsigned int lodMagicInfo = lodMagic + 6;

//Flags bit saying a brick record ends with a print, the same bit the old game used for its own print mask and name
static constexpr unsigned char printFlag = 8;

/*
	The old game's material byte: 2-9 were pearl, chrome, glow, blink, swirl, rainbow (from Blockland saves, never drawn), slippery, and foil,
	and undulo and bouncy were 1000 and 2000 added on top, cut down to a byte when saved
	Bricks here have one material, so undulo or bouncy wins over whatever it was added to
*/
static unsigned char oldSaveMaterial(unsigned char saved)
{
	if (saved >= 1000 % 256 && saved < 1000 % 256 + 10)
		return BrickMaterial_Undulo;
	if (saved >= 2000 % 256 && saved < 2000 % 256 + 10)
		return BrickMaterial_Bouncy;

	switch (saved)
	{
		case 2: return BrickMaterial_Pearl;
		case 3: return BrickMaterial_Chrome;
		case 4: return BrickMaterial_Glow;
		case 5: return BrickMaterial_Blink;
		case 6: return BrickMaterial_Hologram;
		case 7: return BrickMaterial_Rainbow;
		case 8: return BrickMaterial_Slippery;
		case 9: return BrickMaterial_Foil;
		default: return BrickMaterial_None;
	}
}

//Fixed size parts of old save brick records
static constexpr std::streamoff basicRecordBytes = 4 + 3 * 4 + 4 + 2;
static constexpr std::streamoff specialRecordBytes = 4 + 3 * 4 + 4 + 2;

std::string getSavePath(const std::string& fileName)
{
	if (fileName.empty() || fileName.length() > 128 || fileName[0] == '.')
		return "";

	if (fileName.find_first_of("/\\:") != std::string::npos || fileName.find("..") != std::string::npos)
		return "";

	return "Saves/" + fileName;
}

std::string getVehicleSavePath(const std::string& name)
{
	if (name.empty() || name.length() > 64 || name[0] == '.' || name.back() == ' ')
		return "";

	if (name.find_first_of("/\\:*?\"<>|") != std::string::npos || name.find("..") != std::string::npos)
		return "";

	return "Saves/Vehicles/" + name + ".lod";
}

static void writeValue(std::ostream& file, const auto& value)
{
	file.write((const char*)&value, sizeof(value));
}

static bool readValue(std::istream& file, auto& value)
{
	return (bool)file.read((char*)&value, sizeof(value));
}

/*
	The LodSaveInfo block right after the version number:
	1 byte		-	length of the saver's name, then the name
	8 bytes		-	unix seconds it was saved at
	4 bytes		-	length of the thumbnail, then the thumbnail
*/
static void writeInfo(std::ostream& file, const LodSaveInfo* info)
{
	std::string savedBy = info ? info->savedBy.substr(0, 255) : "";
	int64_t savedAt = info ? info->savedAt : 0;
	std::string thumbnail = info && info->thumbnail.size() <= maxSaveThumbnailBytes ? info->thumbnail : "";

	writeValue(file, (unsigned char)savedBy.length());
	file.write(savedBy.c_str(), savedBy.length());
	writeValue(file, savedAt);
	writeValue(file, (uint32_t)thumbnail.size());
	file.write(thumbnail.data(), thumbnail.size());
}

//Reads what writeInfo wrote, false if the file ran out or the thumbnail is bigger than one can be
static bool readInfo(std::istream& file, LodSaveInfo& info)
{
	unsigned char nameLength;
	uint32_t thumbnailBytes;
	if (!readValue(file, nameLength))
		return false;

	info.savedBy.resize(nameLength);
	if (nameLength > 0 && !file.read(&info.savedBy[0], nameLength))
		return false;

	if (!readValue(file, info.savedAt) || !readValue(file, thumbnailBytes) || thumbnailBytes > maxSaveThumbnailBytes)
		return false;

	info.thumbnail.resize(thumbnailBytes);
	if (thumbnailBytes > 0 && !file.read(&info.thumbnail[0], thumbnailBytes))
		return false;

	return true;
}

bool writeLodBricks(std::ostream& file, const std::vector<const Brick*>& bricks, const BrickTypes* types, const PrintTypes* prints, bool omitOwnership, const LodSaveInfo* info)
{
	std::vector<const Brick*> basic;
	std::vector<const Brick*> special;

	//Special types used in this save, and each one's index among them, which is what its bricks store
	std::vector<std::string> typeNames;
	std::unordered_map<uint16_t, unsigned int> saveTypeIDs;

	for (const Brick* brick : bricks)
	{
		const SpecialBrickType* type = brick->isSpecial() && types ? types->getSpecial(brick->typeID - 1) : nullptr;
		if (!type)
		{
			basic.push_back(brick);
			continue;
		}

		if (!saveTypeIDs.count(brick->typeID))
		{
			saveTypeIDs[brick->typeID] = typeNames.size();
			typeNames.push_back(type->uiName.substr(0, 255));
		}
		special.push_back(brick);
	}

	unsigned int count = bricks.size();
	writeValue(file, lodMagicInfo);
	writeInfo(file, info);
	writeValue(file, count);

	writeValue(file, (unsigned int)typeNames.size());
	for (const std::string& typeName : typeNames)
	{
		writeValue(file, (unsigned char)typeName.length());
		file.write(typeName.c_str(), typeName.length());
	}

	//Color, center, then either a size and print mask or a special type, then the turn and everything after it, the same for both
	auto writeRecord = [&](const Brick* brick)
	{
		for (int channel = 0; channel < 4; channel++)
			writeValue(file, (unsigned char)brick->color[channel]);

		//Brick centers, x and z in studs, y in world units
		writeValue(file, (float)(brick->x + brick->footprintWidth() * 0.5));
		writeValue(file, (float)((brick->y + brick->height * 0.5) * PLATE_SIZE));
		writeValue(file, (float)(brick->z + brick->footprintLength() * 0.5));

		if (saveTypeIDs.count(brick->typeID))
			writeValue(file, saveTypeIDs[brick->typeID]);
		else
		{
			writeValue(file, brick->width);
			writeValue(file, brick->height);
			writeValue(file, brick->length);
			writeValue(file, (unsigned char)0); //Print mask
		}

		writeValue(file, brick->angleID);
		writeValue(file, brick->material);

		writeValue(file, omitOwnership ? -1 : brick->ownerID);

		std::string name = brick->name.substr(0, 255);
		writeValue(file, (unsigned char)name.length());
		file.write(name.c_str(), name.length());

		std::string printName = prints ? prints->getName((int)brick->printID - 1).substr(0, 255) : "";

		unsigned char flags = brick->collides ? 1 : 0;
		if (brick->attachments)
			flags |= brick->attachments->getFlags();
		if (!printName.empty())
			flags |= printFlag;
		writeValue(file, flags);

		//The first byte is full, so what the brick spawns has a byte of its own
		writeValue(file, brick->attachments ? brick->attachments->getExtraFlags() : (unsigned char)0);

		if (brick->attachments)
			brick->attachments->writeParts([&file](const void* data, size_t count) { file.write((const char*)data, count); });

		if (!printName.empty())
		{
			writeValue(file, (unsigned char)printName.length());
			file.write(printName.c_str(), printName.length());
		}
	};

	writeValue(file, (unsigned int)basic.size());
	for (const Brick* brick : basic)
		writeRecord(brick);

	writeValue(file, (unsigned int)0); //Transparent basic bricks, the old game only filled this in when clients saved

	writeValue(file, (unsigned int)special.size());
	for (const Brick* brick : special)
		writeRecord(brick);

	return (bool)file;
}

bool saveLodBuild(const BrickHolder& bricks, const PrintTypes* prints, const std::string& path, bool omitOwnership, const LodSaveInfo* saveInfo)
{
	scope("saveLodBuild");

	std::error_code errorCode;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), errorCode);

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path + " for writing");
		return false;
	}

	std::vector<const Brick*> all;
	all.reserve(bricks.size());
	for (size_t a = 0; a < bricks.size(); a++)
		all.push_back(bricks.get(a));

	if (!writeLodBricks(file, all, bricks.getTypes(), prints, omitOwnership, saveInfo))
	{
		error("Error while writing " + path);
		return false;
	}

	info("Saved " + std::to_string(all.size()) + " bricks to " + path);
	return true;
}

/*
	Reads the owner, name, and flags at the end of a next version record, then either our attachments or the old game's music and light data, which is skipped
	Either way a print's name comes out in printName, "" for a brick without one
	Returns false if the file ran out. lightFloats is how many floats the save's lights have, see BrickAttachments::readParts
*/
static bool readRecordExtras(std::istream& file, bool hasAttachments, bool hasPrints, bool hasSpawns, size_t lightFloats, bool& collides, std::string& name,
	std::shared_ptr<BrickAttachments>& attachments, std::string& printName)
{
	int ownerID;
	unsigned char nameLength, flags, extraFlags = 0;
	if (!readValue(file, ownerID) || !readValue(file, nameLength))
		return false;

	name.resize(nameLength);
	if (nameLength > 0 && !file.read(&name[0], nameLength))
		return false;

	if (!readValue(file, flags))
		return false;

	if (hasSpawns && !readValue(file, extraFlags))
		return false;

	collides = flags & 1;

	//A name on its own, however many faces the old game's mask put it on, since a print brick here wears it on all of them
	auto readPrintName = [&file, &printName](bool hasMask)
	{
		unsigned char mask = 1, printNameLength;
		if (hasMask && !readValue(file, mask))
			return false;

		if (!readValue(file, printNameLength))
			return false;

		printName.resize(printNameLength);
		if (printNameLength > 0 && !file.read(&printName[0], printNameLength))
			return false;

		if (mask == 0)
			printName = "";
		return true;
	};

	if (hasAttachments)
	{
		if ((flags & (BrickAttachment_Music | BrickAttachment_Light | BrickAttachment_Emitter | BrickAttachment_Wheel | BrickAttachment_Steering | BrickAttachment_Horn)) || extraFlags != 0)
		{
			auto read = std::make_shared<BrickAttachments>();
			if (!read->readParts(flags, extraFlags, [&file](void* data, size_t count) { return (bool)file.read((char*)data, count); }, lightFloats))
				return false;

			read->clampValues();
			if (!read->isEmpty())
				attachments = read;
		}

		if (hasPrints && (flags & printFlag) && !readPrintName(false))
			return false;

		return true;
	}

	//Music: track and pitch
	if (flags & 2)
		file.seekg(sizeof(unsigned int) + sizeof(float), std::ios::cur);

	//Light: rgb
	if (flags & 4)
		file.seekg(3 * sizeof(float), std::ios::cur);

	//The old game's print: mask then name
	if ((flags & printFlag) && !readPrintName(true))
		return false;

	return (bool)file;
}

/*
	Reads the version number, the info block of a version that has one, and the brick count, leaving the stream at the type count
	Returns false if the stream ran out or doesn't start with a version we read. magic comes back with the version
*/
static bool readHeader(std::istream& file, unsigned int& magic, LodSaveInfo& info)
{
	if (!readValue(file, magic) || magic < lodMagic || magic > lodMagicInfo)
		return false;

	if (magic >= lodMagicInfo && !readInfo(file, info))
		return false;

	return (bool)readValue(file, info.brickCount);
}

LodReadResult readLodBricks(std::istream& file, const BrickTypes* types, const PrintTypes* prints, const std::function<void(Brick&)>& found, LodSaveInfo* info)
{
	LodReadResult result;

	unsigned int magic, typeCount;
	LodSaveInfo ownInfo;
	if (!info)
		info = &ownInfo;

	//A file that starts with a version we know is valid even if its header ends early
	if (!readValue(file, magic) || magic < lodMagic || magic > lodMagicInfo)
		return result;
	file.seekg(0);

	result.valid = true;
	bool hasExtras = magic != lodMagic;
	bool hasAttachments = magic >= lodMagicAttachments;
	bool hasPrints = magic >= lodMagicPrints;
	bool hasSpawns = magic >= lodMagicSpawns;
	size_t lightFloats = magic >= lodMagicBlinkingLights ? BrickAttachments::lightFloatCount : BrickAttachments::lightFloatCount - 1;

	if (!readHeader(file, magic, *info) || !readValue(file, typeCount))
		return result;

	//Special type names, bricks in the save refer to them by index, which becomes our own type ID or 0 if we don't have it
	std::vector<uint16_t> typeIDs;
	for (unsigned int a = 0; a < typeCount; a++)
	{
		unsigned char nameLength;
		std::string typeName;
		if (!readValue(file, nameLength))
			return result;

		typeName.resize(nameLength);
		if (nameLength > 0 && !file.read(&typeName[0], nameLength))
			return result;

		typeName = blocklandTextToUtf8(typeName);
		int special = types ? types->findSpecial(typeName) : -1;
		typeIDs.push_back(special < 0 ? 0 : (uint16_t)(special + 1));
		if (special < 0)
			result.missingTypes[typeName] = 0;
	}

	//Opaque basic, transparent basic, then special bricks
	for (int section = 0; section < 3; section++)
	{
		unsigned int sectionCount;
		if (!readValue(file, sectionCount))
			return result;

		for (unsigned int a = 0; a < sectionCount; a++)
		{
			unsigned char color[4];
			float centerX, centerY, centerZ;
			unsigned char width = 0, height = 0, length = 0, printMask, angleID, material;
			unsigned int typeID;

			bool ok = file.read((char*)color, 4) && readValue(file, centerX) && readValue(file, centerY) && readValue(file, centerZ);
			if (section == 2)
				ok = ok && readValue(file, typeID);
			else
				ok = ok && readValue(file, width) && readValue(file, height) && readValue(file, length) && readValue(file, printMask);
			ok = ok && readValue(file, angleID) && readValue(file, material);

			bool collides = true;
			std::string name = "";
			std::string printName = "";
			std::shared_ptr<BrickAttachments> attachments = nullptr;
			if (ok && hasExtras)
				ok = readRecordExtras(file, hasAttachments, hasPrints, hasSpawns, lightFloats, collides, name, attachments, printName);

			if (!ok)
				return result;

			uint16_t specialType = 0;
			if (section == 2)
			{
				specialType = typeID < typeIDs.size() ? typeIDs[typeID] : 0;
				const SpecialBrickType* type = types ? types->getSpecial(specialType - 1) : nullptr;
				if (!type)
				{
					result.skippedSpecial++;
					continue;
				}

				width = type->width;
				height = type->height;
				length = type->length;
			}

			if (width == 0 || height == 0 || length == 0 || angleID > 3 || !std::isfinite(centerX) || !std::isfinite(centerY) || !std::isfinite(centerZ) ||
				std::abs(centerX) > 40000 || std::abs(centerY) > 40000 || std::abs(centerZ) > 40000)
			{
				result.invalid++;
				continue;
			}

			Brick desc;
			desc.typeID = specialType;
			desc.width = width;
			desc.height = height;
			desc.length = length;
			desc.angleID = angleID;
			desc.color = glm::u8vec4(color[0], color[1], color[2], color[3]);
			if (hasAttachments)
				desc.material = material < BrickMaterialCount ? material : BrickMaterial_None;
			else
				desc.material = oldSaveMaterial(material);
			desc.collides = collides;
			desc.name = name;
			desc.attachments = attachments;

			//A print we don't have leaves the brick plain, like a special type we don't have
			if (!printName.empty())
			{
				printName = blocklandTextToUtf8(printName);
				desc.printID = (uint16_t)((prints ? prints->find(printName) : -1) + 1);
				if (desc.printID == 0)
					result.missingPrints[printName]++;
			}
			desc.x = (int)lround(centerX - desc.footprintWidth() * 0.5);
			desc.y = (int)lround(centerY / PLATE_SIZE - height * 0.5);
			desc.z = (int)lround(centerZ - desc.footprintLength() * 0.5);

			found(desc);
		}
	}

	result.complete = true;
	return result;
}

int loadLodBuild(BrickHolder& bricks, const PrintTypes* prints, const std::string& path, int offsetX, int offsetY, int offsetZ)
{
	scope("loadLodBuild");

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path);
		return -1;
	}

	return loadLodBricks(bricks, prints, file, path, offsetX, offsetY, offsetZ);
}

int loadLodBricks(BrickHolder& bricks, const PrintTypes* prints, std::istream& file, const std::string& path, int offsetX, int offsetY, int offsetZ)
{
	unsigned int startMS = SDL_GetTicks();

	int loaded = 0;
	int rejected = 0;

	LodReadResult result = readLodBricks(file, bricks.getTypes(), prints, [&](Brick& desc)
	{
		desc.x += offsetX;
		desc.y += offsetY;
		desc.z += offsetZ;

		if (bricks.add(desc))
			loaded++;
		else
			rejected++;
	});

	if (!result.valid)
	{
		error(path + " is not a Land of Dran binary save");
		return -1;
	}

	if (!result.complete)
		error(path + " ended early, loaded " + std::to_string(loaded) + " bricks");

	info("Loaded " + std::to_string(loaded) + " bricks from " + path + " in " + std::to_string(SDL_GetTicks() - startMS) + "ms");
	if (rejected > 0)
		info(std::to_string(rejected) + " bricks overlapped existing bricks or were out of bounds");
	if (result.skippedSpecial > 0)
	{
		std::string list = "";
		for (const auto& entry : result.missingTypes)
			list += (list.empty() ? "" : ", ") + entry.first;
		info(std::to_string(result.skippedSpecial) + " special bricks of types we don't have were skipped: " + list);
	}
	if (!result.missingPrints.empty())
	{
		int count = 0;
		std::string list = "";
		for (const auto& entry : result.missingPrints)
		{
			count += entry.second;
			list += (list.empty() ? "" : ", ") + entry.first;
		}
		info(std::to_string(count) + " bricks wear prints we don't have, they loaded plain: " + list);
	}
	if (result.invalid > 0)
		error(std::to_string(result.invalid) + " bricks had invalid sizes or rotations");

	return loaded;
}

bool readLodSaveInfo(std::istream& file, LodSaveInfo& info)
{
	unsigned int magic;
	info = LodSaveInfo();
	return readHeader(file, magic, info);
}

bool readLodSaveInfo(const std::string& path, LodSaveInfo& info)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
		return false;

	return readLodSaveInfo(file, info);
}

bool setLodSaveThumbnail(std::string& file, const std::string& thumbnail)
{
	if (thumbnail.size() > maxSaveThumbnailBytes)
		return false;

	std::istringstream stream(file);
	unsigned int magic;
	LodSaveInfo info;
	if (!readValue(stream, magic) || magic != lodMagicInfo || !readInfo(stream, info))
		return false;

	//Everything from the brick count on is kept as it was
	std::string rest = file.substr((size_t)stream.tellg());
	info.thumbnail = thumbnail;

	std::ostringstream out(std::ios::binary);
	writeValue(out, magic);
	writeInfo(out, &info);
	out.write(rest.data(), rest.size());
	file = out.str();
	return true;
}

bool readBlocklandSaveInfo(const std::string& path, LodSaveInfo& info)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
		return false;

	info = LodSaveInfo();

	//Warning, description line count and description, 64 palette lines, then "Linecount N", so it's within the first hundred or so lines
	std::string line;
	for (int a = 0; a < 200 && getline(file, line); a++)
	{
		if (line.compare(0, 10, "Linecount ") == 0)
		{
			info.brickCount = (unsigned int)std::max(0, atoi(line.c_str() + 10));
			return true;
		}
	}
	return false;
}

std::vector<BrickSaveListing> listBrickSaves(bool withThumbnails)
{
	std::vector<BrickSaveListing> saves;

	std::error_code errorCode;
	if (!std::filesystem::is_directory("Saves", errorCode))
		return saves;

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator("Saves", errorCode))
	{
		if (!entry.is_regular_file())
			continue;

		std::string extension = lowercase(entry.path().extension().string());
		if (extension != ".lod" && extension != ".bls")
			continue;

		BrickSaveListing save;
		save.fileName = entry.path().filename().string();
		save.blockland = extension == ".bls";
		//A file with a name a request couldn't name is left out
		if (getSavePath(save.fileName).empty())
			continue;

		bool read = save.blockland ? readBlocklandSaveInfo(entry.path().string(), save.info) : readLodSaveInfo(entry.path().string(), save.info);
		if (!read)
			continue;

		if (!withThumbnails)
			save.info.thumbnail.clear();

		if (save.info.savedAt == 0)
		{
			//Written files' times are in the file clock, which only the system clock is any good for turning into a date
			std::filesystem::file_time_type written = std::filesystem::last_write_time(entry.path(), errorCode);
			if (!errorCode)
			{
				auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(written - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
				save.info.savedAt = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
			}
		}

		saves.push_back(save);
	}

	std::sort(saves.begin(), saves.end(), [](const BrickSaveListing& a, const BrickSaveListing& b) { return lowercase(a.fileName) < lowercase(b.fileName); });
	return saves;
}

//How wide and tall drawBricksFromAbove's pictures are, matching the client's rendered ones
static constexpr int thumbnailSize = 256;

//stb's JPEG writer hands out pieces of the file, appended to the string it's given
static void appendJpegBytes(void* context, void* data, int size)
{
	((std::string*)context)->append((const char*)data, size);
}

std::string drawBricksFromAbove(const std::vector<Brick>& bricks)
{
	if (bricks.empty())
		return "";

	//In studs and plates, the top of a brick being what's seen from above
	int minX = INT_MAX, minZ = INT_MAX, maxX = INT_MIN, maxZ = INT_MIN, minTop = INT_MAX, maxTop = INT_MIN;
	for (const Brick& brick : bricks)
	{
		minX = std::min(minX, brick.x);
		minZ = std::min(minZ, brick.z);
		maxX = std::max(maxX, brick.x + brick.footprintWidth());
		maxZ = std::max(maxZ, brick.z + brick.footprintLength());
		minTop = std::min(minTop, brick.y + (int)brick.height);
		maxTop = std::max(maxTop, brick.y + (int)brick.height);
	}

	//Studs per pixel, the same both ways so nothing is stretched, with a little grass around the edge
	float extent = std::max(maxX - minX, maxZ - minZ) * 1.1f;
	float scale = thumbnailSize / std::max(extent, 1.0f);
	float originX = (minX + maxX) * 0.5f - thumbnailSize * 0.5f / scale;
	float originZ = (minZ + maxZ) * 0.5f - thumbnailSize * 0.5f / scale;

	std::vector<unsigned char> pixels(thumbnailSize * thumbnailSize * 3);
	std::vector<int> tops(thumbnailSize * thumbnailSize, INT_MIN);
	for (size_t a = 0; a < pixels.size(); a += 3)
	{
		pixels[a] = 96;
		pixels[a + 1] = 140;
		pixels[a + 2] = 64;
	}

	float topRange = (float)std::max(maxTop - minTop, 1);
	for (const Brick& brick : bricks)
	{
		//Nearly invisible bricks would hide what's under them
		if (brick.color.a < 64)
			continue;

		int top = brick.y + brick.height;
		//Higher bricks come out brighter, so a roof reads over the floor around it
		float shade = 0.55f + 0.45f * (top - minTop) / topRange;
		unsigned char r = (unsigned char)std::min(255.0f, brick.color.r * shade);
		unsigned char g = (unsigned char)std::min(255.0f, brick.color.g * shade);
		unsigned char b = (unsigned char)std::min(255.0f, brick.color.b * shade);

		int left = (int)std::floor((brick.x - originX) * scale);
		int right = (int)std::ceil((brick.x + brick.footprintWidth() - originX) * scale);
		int front = (int)std::floor((brick.z - originZ) * scale);
		int back = (int)std::ceil((brick.z + brick.footprintLength() - originZ) * scale);
		left = std::clamp(left, 0, thumbnailSize);
		right = std::clamp(right, 0, thumbnailSize);
		front = std::clamp(front, 0, thumbnailSize);
		back = std::clamp(back, 0, thumbnailSize);

		//A brick smaller than a pixel still gets one
		if (right == left && left < thumbnailSize)
			right = left + 1;
		if (back == front && front < thumbnailSize)
			back = front + 1;

		for (int row = front; row < back; row++)
		{
			for (int column = left; column < right; column++)
			{
				int index = row * thumbnailSize + column;
				if (tops[index] > top)
					continue;
				tops[index] = top;
				pixels[index * 3] = r;
				pixels[index * 3 + 1] = g;
				pixels[index * 3 + 2] = b;
			}
		}
	}

	std::string jpeg;
	if (!stbi_write_jpg_to_func(appendJpegBytes, &jpeg, thumbnailSize, thumbnailSize, 3, pixels.data(), 85))
		return "";
	return jpeg;
}

std::string drawBricksFromAbove(const BrickHolder& bricks)
{
	std::vector<Brick> copies;
	copies.reserve(bricks.size());
	for (size_t a = 0; a < bricks.size(); a++)
		copies.push_back(*bricks.get(a));
	return drawBricksFromAbove(copies);
}

//Splits on single spaces, keeping empty fields, since an empty print name in a .bls line leaves two spaces in a row
static std::vector<std::string> splitFields(const std::string& line)
{
	std::vector<std::string> fields;
	size_t start = 0;
	while (true)
	{
		size_t space = line.find(' ', start);
		fields.push_back(line.substr(start, space == std::string::npos ? std::string::npos : space - start));
		if (space == std::string::npos)
			return fields;
		start = space + 1;
	}
}

/*
	Reads a line like +-LIGHT Red Light" 1, which follows the brick it's on
	Returns false if the line isn't of that kind, otherwise the name between the kind and the quote and whatever's after the quote and a space
*/
static bool readAttachmentLine(const std::string& line, const std::string& kind, std::string& uiName, std::string& value)
{
	std::string prefix = "+-" + kind + " ";
	if (line.compare(0, prefix.length(), prefix) != 0)
		return false;

	size_t quote = line.rfind('"');
	if (quote == std::string::npos || quote < prefix.length())
		return false;

	uiName = blocklandTextToUtf8(line.substr(prefix.length(), quote - prefix.length()));
	value = quote + 2 <= line.length() ? line.substr(quote + 2) : "";
	return true;
}

//"name (count), name (count)" for a log line, adding up the counts in total
static std::string listCounts(const std::map<std::string, int>& counts, int& total)
{
	total = 0;
	std::string list = "";
	for (const auto& entry : counts)
	{
		total += entry.second;
		list += (list.empty() ? "" : ", ") + entry.first + " (" + std::to_string(entry.second) + ")";
	}
	return list;
}

BlocklandReadResult readBlocklandBricks(std::istream& file, const BrickTypes& types, const PrintTypes* prints, const BlocklandAttachmentLookup& lookup, const std::function<void(Brick&)>& found)
{
	BlocklandReadResult result;

	auto readLine = [&file](std::string& line) -> bool
	{
		if (!getline(file, line))
			return false;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		return true;
	};

	std::string line;

	//Warning line, then a count of description lines followed by the description itself
	readLine(line);
	readLine(line);
	int descriptionLines = atoi(line.c_str());
	for (int a = 0; a < descriptionLines; a++)
		readLine(line);

	glm::u8vec4 palette[64];
	for (int a = 0; a < 64; a++)
	{
		float r = 1, g = 1, b = 1, alpha = 1;
		if (!readLine(line))
			return result;
		sscanf(line.c_str(), "%f %f %f %f", &r, &g, &b, &alpha);
		palette[a] = glm::u8vec4(glm::clamp(glm::vec4(r, g, b, alpha), 0.0f, 1.0f) * 255.0f + 0.5f);
	}

	result.valid = true;

	//"Linecount N"
	readLine(line);

	//Each brick is handed over once the lines after it, which name it and put things on it, have been read, so its attachments come with it
	Brick pending;
	bool hasPending = false;

	auto finishPending = [&]()
	{
		if (!hasPending)
			return;
		hasPending = false;

		if (const BrickAttachments* attachments = pending.attachments.get())
		{
			result.lights += attachments->hasLight ? 1 : 0;
			result.emitters += attachments->emitterName.empty() ? 0 : 1;
			result.music += attachments->musicName.empty() ? 0 : 1;
		}

		found(pending);
	};

	auto pendingAttachments = [&]() -> BrickAttachments&
	{
		if (!pending.attachments)
			pending.attachments = std::make_shared<BrickAttachments>();
		return *pending.attachments;
	};

	while (readLine(line))
	{
		if (line.compare(0, 2, "+-") == 0)
		{
			//Lines after a brick we skipped
			if (!hasPending)
				continue;

			std::string uiName, value;
			if (line.compare(0, 15, "+-NTOBJECTNAME ") == 0)
				pending.name = line.substr(15);
			else if (readAttachmentLine(line, "LIGHT", uiName, value))
			{
				//Followed by 1, or nothing in older saves, for a light that's on
				if (value != "0" && !(lookup.setLight && lookup.setLight(uiName, pendingAttachments())))
					result.missingLights[uiName]++;
			}
			else if (readAttachmentLine(line, "EMITTER", uiName, value))
			{
				std::string typeName = lookup.findEmitterType ? lookup.findEmitterType(uiName) : "";
				if (typeName.empty())
					result.missingEmitters[uiName]++;
				else
				{
					pendingAttachments().emitterName = typeName;
					//Followed by which way it points, 0 for up
					if (atoi(value.c_str()) != 0)
						result.turnedEmitters++;
				}
			}
			else if (readAttachmentLine(line, "AUDIOEMITTER", uiName, value))
			{
				std::string soundName = lookup.findMusic ? lookup.findMusic(uiName) : "";
				if (soundName.empty())
					result.missingMusic[uiName]++;
				else
					pendingAttachments().musicName = soundName;
			}
			continue;
		}

		size_t quote = line.find('"');
		if (quote == std::string::npos || quote + 2 > line.length())
			continue;

		finishPending();

		std::string name = blocklandTextToUtf8(line.substr(0, quote));
		std::vector<std::string> fields = splitFields(line.substr(quote + 2));

		//x y z angle isBaseplate colorIndex print colorFx shapeFx raycasting collision rendering
		if (fields.size() < 11)
			continue;

		int width, height, length;
		uint16_t typeID = 0;
		if (!types.getBasicSize(name, width, height, length))
		{
			int special = types.findSpecial(name);
			const SpecialBrickType* type = types.getSpecial(special);
			if (!type)
			{
				result.skippedNames[name]++;
				continue;
			}

			typeID = (uint16_t)(special + 1);
			width = type->width;
			height = type->height;
			length = type->length;
		}

		pending = Brick();
		pending.typeID = typeID;
		pending.width = width;
		pending.height = height;
		pending.length = length;

		//Used as is, like the old game: in the Golden Gate save 45° ramps turned this way have the bricks they lead up to past their high edge
		pending.angleID = atoi(fields[3].c_str()) % 4;

		//Blockland names its prints the same way we do, e.g. "Letters/X", and a brick without one leaves the field empty
		if (!fields[6].empty() && fields[6] != "0")
		{
			std::string printName = blocklandTextToUtf8(fields[6]);
			pending.printID = (uint16_t)((prints ? prints->find(printName) : -1) + 1);
			if (pending.printID == 0)
				result.missingPrints[printName]++;
		}
		pending.color = palette[std::clamp(atoi(fields[5].c_str()), 0, 63)];
		pending.collides = fields[10] != "0";

		//colorFx 1-6 are pearl, chrome, glow, blink, swirl, and rainbow, and shapeFx 1 is undulo, which wins since bricks here have one material
		//Water (shapeFx 2) has no material here
		static constexpr unsigned char colorFxMaterials[7] = { BrickMaterial_None, BrickMaterial_Pearl, BrickMaterial_Chrome, BrickMaterial_Glow, BrickMaterial_Blink, BrickMaterial_Hologram, BrickMaterial_Rainbow };
		int colorFx = atoi(fields[7].c_str());
		int shapeFx = atoi(fields[8].c_str());
		if (shapeFx == 1)
			pending.material = BrickMaterial_Undulo;
		else if (colorFx >= 0 && colorFx < 7)
			pending.material = colorFxMaterials[colorFx];

		bool colorKept = colorFx == 0 || (colorFx > 0 && colorFx < 7 && pending.material == colorFxMaterials[colorFx]);
		if (!colorKept || (shapeFx != 0 && shapeFx != 1))
			result.droppedEffects++;

		//Blockland is z-up with half-stud and fifth-of-a-world-unit units, the old game swapped y and z and doubled
		double centerX = atof(fields[0].c_str()) * 2.0;
		double centerZ = atof(fields[1].c_str()) * 2.0;
		double centerY = atof(fields[2].c_str()) * 2.0;

		pending.x = (int)lround(centerX - pending.footprintWidth() * 0.5);
		pending.y = (int)lround(centerY / PLATE_SIZE - height * 0.5);
		pending.z = (int)lround(centerZ - pending.footprintLength() * 0.5);

		hasPending = true;
	}

	finishPending();
	return result;
}

int loadBlocklandBuild(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, const std::string& path, const BlocklandAttachmentLookup& lookup, int offsetX, int offsetY, int offsetZ)
{
	scope("loadBlocklandBuild");

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path);
		return -1;
	}

	return loadBlocklandBricks(bricks, types, prints, file, path, lookup, offsetX, offsetY, offsetZ);
}

int loadBlocklandBricks(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, std::istream& file, const std::string& path, const BlocklandAttachmentLookup& lookup, int offsetX, int offsetY, int offsetZ)
{
	unsigned int startMS = SDL_GetTicks();

	int loaded = 0;
	int rejected = 0;
	int lights = 0;
	int emitters = 0;
	int music = 0;

	BlocklandReadResult result = readBlocklandBricks(file, types, prints, lookup, [&](Brick& brick)
	{
		brick.x += offsetX;
		brick.y += offsetY;
		brick.z += offsetZ;

		if (!bricks.add(brick))
		{
			rejected++;
			return;
		}

		loaded++;
		if (const BrickAttachments* attachments = brick.attachments.get())
		{
			lights += attachments->hasLight ? 1 : 0;
			emitters += attachments->emitterName.empty() ? 0 : 1;
			music += attachments->musicName.empty() ? 0 : 1;
		}
	});

	if (!result.valid)
	{
		error(path + " ended before its color palette did");
		return -1;
	}

	info("Loaded " + std::to_string(loaded) + " bricks from " + path + " in " + std::to_string(SDL_GetTicks() - startMS) + "ms");
	if (rejected > 0)
		info(std::to_string(rejected) + " bricks overlapped existing bricks or were out of bounds");
	if (lights + emitters + music > 0)
		info("Its bricks have " + std::to_string(lights) + " lights, " + std::to_string(emitters) + " emitters, and " + std::to_string(music) + " music loops");

	int count = 0;
	if (!result.skippedNames.empty())
	{
		std::string list = listCounts(result.skippedNames, count);
		info(std::to_string(count) + " bricks of unknown types skipped: " + list);
	}
	if (!result.missingLights.empty())
	{
		std::string list = listCounts(result.missingLights, count);
		info(std::to_string(count) + " lights skipped, addBlocklandLight wasn't given their names: " + list);
	}
	if (!result.missingEmitters.empty())
	{
		std::string list = listCounts(result.missingEmitters, count);
		info(std::to_string(count) + " emitters skipped, no emitter type has their uiName: " + list);
	}
	if (!result.missingMusic.empty())
	{
		std::string list = listCounts(result.missingMusic, count);
		info(std::to_string(count) + " music loops skipped, no music sound type has their name: " + list);
	}
	if (!result.missingPrints.empty())
	{
		std::string list = listCounts(result.missingPrints, count);
		info(std::to_string(count) + " bricks wear prints we don't have, they loaded plain: " + list);
	}
	if (result.turnedEmitters > 0)
		info(std::to_string(result.turnedEmitters) + " emitters pointed sideways or down in Blockland, emitters on bricks here always point up");
	if (result.droppedEffects > 0)
		info(std::to_string(result.droppedEffects) + " bricks lost a color or shape effect: water has no material here, and undulo replaces a color effect on the same brick");

	return loaded;
}
