#include "BrickSaves.h"

/*
	The "next version" (16483535) adds owner, name, and flag fields to every brick record
	Our own version after that (16483536) swaps its music track and light color for BrickAttachments' music loop, whole light, and emitter,
	and its material byte is a BrickMaterial (saves from before materials wrote 0 there, which is none)
	The one after (16483537) adds blink speed and strength to the end of the light, see BrickAttachments::lightFloatCount
	The one after that (16483538) puts a brick's print name at the very end of its record, see printFlag
*/
static constexpr unsigned int lodMagic = 16483534;
static constexpr unsigned int lodMagicAttachments = lodMagic + 2;
static constexpr unsigned int lodMagicBlinkingLights = lodMagic + 3;
static constexpr unsigned int lodMagicPrints = lodMagic + 4;

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

bool writeLodBricks(std::ostream& file, const std::vector<const Brick*>& bricks, const BrickTypes* types, const PrintTypes* prints, bool omitOwnership)
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
	writeValue(file, lodMagicPrints);
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

bool saveLodBuild(const BrickHolder& bricks, const PrintTypes* prints, const std::string& path, bool omitOwnership)
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

	if (!writeLodBricks(file, all, bricks.getTypes(), prints, omitOwnership))
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
static bool readRecordExtras(std::istream& file, bool hasAttachments, bool hasPrints, size_t lightFloats, bool& collides, std::string& name,
	std::shared_ptr<BrickAttachments>& attachments, std::string& printName)
{
	int ownerID;
	unsigned char nameLength, flags;
	if (!readValue(file, ownerID) || !readValue(file, nameLength))
		return false;

	name.resize(nameLength);
	if (nameLength > 0 && !file.read(&name[0], nameLength))
		return false;

	if (!readValue(file, flags))
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
		if (flags & (BrickAttachment_Music | BrickAttachment_Light | BrickAttachment_Emitter | BrickAttachment_Wheel | BrickAttachment_Steering | BrickAttachment_Horn))
		{
			auto read = std::make_shared<BrickAttachments>();
			if (!read->readParts(flags, [&file](void* data, size_t count) { return (bool)file.read((char*)data, count); }, lightFloats))
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

LodReadResult readLodBricks(std::istream& file, const BrickTypes* types, const PrintTypes* prints, const std::function<void(Brick&)>& found)
{
	LodReadResult result;

	unsigned int magic, brickCount, typeCount;
	if (!readValue(file, magic) || magic < lodMagic || magic > lodMagicPrints)
		return result;

	result.valid = true;
	bool hasExtras = magic != lodMagic;
	bool hasAttachments = magic >= lodMagicAttachments;
	bool hasPrints = magic >= lodMagicPrints;
	size_t lightFloats = magic >= lodMagicBlinkingLights ? BrickAttachments::lightFloatCount : BrickAttachments::lightFloatCount - 1;

	if (!readValue(file, brickCount) || !readValue(file, typeCount))
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
				ok = readRecordExtras(file, hasAttachments, hasPrints, lightFloats, collides, name, attachments, printName);

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

	unsigned int startMS = SDL_GetTicks();

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path);
		return -1;
	}

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

int loadBlocklandBuild(BrickHolder& bricks, const BrickTypes& types, const PrintTypes* prints, const std::string& path, const BlocklandAttachmentLookup& lookup)
{
	scope("loadBlocklandBuild");

	unsigned int startMS = SDL_GetTicks();

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path);
		return -1;
	}

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
		{
			error(path + " ended before its color palette did");
			return -1;
		}
		sscanf(line.c_str(), "%f %f %f %f", &r, &g, &b, &alpha);
		palette[a] = glm::u8vec4(glm::clamp(glm::vec4(r, g, b, alpha), 0.0f, 1.0f) * 255.0f + 0.5f);
	}

	//"Linecount N"
	readLine(line);

	int loaded = 0;
	int rejected = 0;
	int lights = 0;
	int emitters = 0;
	int music = 0;
	int turnedEmitters = 0;
	int droppedEffects = 0;
	std::map<std::string, int> skippedNames;
	std::map<std::string, int> missingLights;
	std::map<std::string, int> missingEmitters;
	std::map<std::string, int> missingMusic;
	std::map<std::string, int> missingPrints;

	//Each brick is added once the lines after it, which name it and put things on it, have been read, so its attachments spawn with it
	Brick pending;
	bool hasPending = false;

	auto addPending = [&]()
	{
		if (!hasPending)
			return;
		hasPending = false;

		if (!bricks.add(pending))
		{
			rejected++;
			return;
		}

		loaded++;
		if (const BrickAttachments* attachments = pending.attachments.get())
		{
			lights += attachments->hasLight ? 1 : 0;
			emitters += attachments->emitterName.empty() ? 0 : 1;
			music += attachments->musicName.empty() ? 0 : 1;
		}
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
					missingLights[uiName]++;
			}
			else if (readAttachmentLine(line, "EMITTER", uiName, value))
			{
				std::string typeName = lookup.findEmitterType ? lookup.findEmitterType(uiName) : "";
				if (typeName.empty())
					missingEmitters[uiName]++;
				else
				{
					pendingAttachments().emitterName = typeName;
					//Followed by which way it points, 0 for up
					if (atoi(value.c_str()) != 0)
						turnedEmitters++;
				}
			}
			else if (readAttachmentLine(line, "AUDIOEMITTER", uiName, value))
			{
				std::string soundName = lookup.findMusic ? lookup.findMusic(uiName) : "";
				if (soundName.empty())
					missingMusic[uiName]++;
				else
					pendingAttachments().musicName = soundName;
			}
			continue;
		}

		size_t quote = line.find('"');
		if (quote == std::string::npos || quote + 2 > line.length())
			continue;

		addPending();

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
				skippedNames[name]++;
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
				missingPrints[printName]++;
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
			droppedEffects++;

		//Blockland is z-up with half-stud and fifth-of-a-world-unit units, the old game swapped y and z and doubled
		double centerX = atof(fields[0].c_str()) * 2.0;
		double centerZ = atof(fields[1].c_str()) * 2.0;
		double centerY = atof(fields[2].c_str()) * 2.0;

		pending.x = (int)lround(centerX - pending.footprintWidth() * 0.5);
		pending.y = (int)lround(centerY / PLATE_SIZE - height * 0.5);
		pending.z = (int)lround(centerZ - pending.footprintLength() * 0.5);

		hasPending = true;
	}

	addPending();

	info("Loaded " + std::to_string(loaded) + " bricks from " + path + " in " + std::to_string(SDL_GetTicks() - startMS) + "ms");
	if (rejected > 0)
		info(std::to_string(rejected) + " bricks overlapped existing bricks or were out of bounds");
	if (lights + emitters + music > 0)
		info("Its bricks have " + std::to_string(lights) + " lights, " + std::to_string(emitters) + " emitters, and " + std::to_string(music) + " music loops");

	int count = 0;
	if (!skippedNames.empty())
	{
		std::string list = listCounts(skippedNames, count);
		info(std::to_string(count) + " bricks of unknown types skipped: " + list);
	}
	if (!missingLights.empty())
	{
		std::string list = listCounts(missingLights, count);
		info(std::to_string(count) + " lights skipped, addBlocklandLight wasn't given their names: " + list);
	}
	if (!missingEmitters.empty())
	{
		std::string list = listCounts(missingEmitters, count);
		info(std::to_string(count) + " emitters skipped, no emitter type has their uiName: " + list);
	}
	if (!missingMusic.empty())
	{
		std::string list = listCounts(missingMusic, count);
		info(std::to_string(count) + " music loops skipped, no music sound type has their name: " + list);
	}
	if (!missingPrints.empty())
	{
		std::string list = listCounts(missingPrints, count);
		info(std::to_string(count) + " bricks wear prints we don't have, they loaded plain: " + list);
	}
	if (turnedEmitters > 0)
		info(std::to_string(turnedEmitters) + " emitters pointed sideways or down in Blockland, emitters on bricks here always point up");
	if (droppedEffects > 0)
		info(std::to_string(droppedEffects) + " bricks lost a color or shape effect: water has no material here, and undulo replaces a color effect on the same brick");

	return loaded;
}
