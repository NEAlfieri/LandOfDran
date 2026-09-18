#include "BrickTypes.h"

#include "Brick.h"
#include "../Physics/PhysicsWorld.h"

#include <BulletCollision/CollisionShapes/btShapeHull.h>

//Hulls with more points than this are simplified, round bricks list the same corners over and over
static constexpr int maxHullPoints = 64;

SpecialBrickType::~SpecialBrickType()
{
	delete shape;
	for (btCollisionShape* child : childShapes)
		delete child;
}

std::string blocklandTextToUtf8(const std::string& text)
{
	bool validUtf8 = true;
	for (size_t a = 0; a < text.length() && validUtf8; a++)
	{
		unsigned char byte = text[a];
		if (byte < 0x80)
			continue;

		int continuation = (byte & 0xE0) == 0xC0 ? 1 : (byte & 0xF0) == 0xE0 ? 2 : (byte & 0xF8) == 0xF0 ? 3 : -1;
		if (continuation < 0 || a + continuation >= text.length())
		{
			validUtf8 = false;
			break;
		}

		for (int b = 1; b <= continuation; b++)
		{
			if (((unsigned char)text[a + b] & 0xC0) != 0x80)
				validUtf8 = false;
		}
		a += continuation;
	}

	if (validUtf8)
		return text;

	std::string converted;
	for (unsigned char byte : text)
	{
		if (byte < 0x80)
			converted.push_back(byte);
		else
		{
			converted.push_back((char)(0xC0 | (byte >> 6)));
			converted.push_back((char)(0x80 | (byte & 0x3F)));
		}
	}
	return converted;
}

static void stripCarriageReturn(std::string& line)
{
	if (!line.empty() && line.back() == '\r')
		line.pop_back();
}

static std::string trim(const std::string& text)
{
	size_t start = text.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = text.find_last_not_of(" \t\r\n");
	return text.substr(start, end - start + 1);
}

static bool parseBlocklandBasicName(std::string name, int& width, int& height, int& length)
{
	bool plate = !name.empty() && name.back() == 'f';
	if (plate)
		name.pop_back();

	std::vector<int> numbers;
	size_t start = 0;
	while (true)
	{
		size_t separator = name.find('x', start);
		std::string part = name.substr(start, separator == std::string::npos ? std::string::npos : separator - start);

		if (part.empty() || part.length() > 3 || part.find_first_not_of("0123456789") != std::string::npos)
			return false;

		numbers.push_back(std::stoi(part));

		if (separator == std::string::npos)
			break;
		start = separator + 1;
	}

	if (numbers.size() == 2)
	{
		width = numbers[0];
		length = numbers[1];
		height = plate ? 1 : 3;
	}
	else if (numbers.size() == 3 && !plate)
	{
		width = numbers[0];
		length = numbers[1];
		height = numbers[2] * 3;
	}
	else
		return false;

	return width >= 1 && width <= 255 && length >= 1 && length <= 255 && height >= 1 && height <= 255;
}

/*
	Walks the lines of a .blb, skipping blank lines, which Blockland's files scatter everywhere
*/
class BlbReader
{
	std::vector<std::string> lines;
	size_t next = 0;

	public:

	bool done() const { return next >= lines.size(); }

	//The next line that isn't blank, "" once the file runs out
	std::string take()
	{
		while (next < lines.size())
		{
			std::string line = trim(lines[next++]);
			if (!line.empty())
				return line;
		}
		return "";
	}

	std::string peek()
	{
		size_t saved = next;
		std::string line = take();
		next = saved;
		return line;
	}

	/*
		Read like the old game's atof, since a few of Blockland's own files have typos like "1 v" for "1 0 1":
		values that aren't numbers or are missing are 0, only a line that doesn't start with a number fails
	*/
	bool takeFloats(int count, float* out)
	{
		std::string line = take();
		std::istringstream stream(line);
		std::string word;

		for (int a = 0; a < count; a++)
		{
			out[a] = 0;
			if (!(stream >> word))
				continue;

			char* end = nullptr;
			double value = strtod(word.c_str(), &end);
			if (end == word.c_str())
			{
				if (a == 0)
					return false;
				continue;
			}

			out[a] = std::isfinite(value) ? (float)value : 0.0f;
		}
		return true;
	}

	explicit BlbReader(std::ifstream& file)
	{
		std::string line;
		while (getline(file, line))
		{
			stripCarriageReturn(line);
			lines.push_back(line);
		}
	}
};

static bool isWholeNumber(const std::string& text)
{
	return !text.empty() && text.length() < 6 && text.find_first_not_of("0123456789") == std::string::npos;
}

//Blockland is z-up with x and y in studs and z in plates, the old game swapped y and z, and so does everything here
static glm::vec3 blbPosition(const float* values)
{
	return glm::vec3(values[0] * STUD_SIZE, values[2] * PLATE_SIZE, values[1] * STUD_SIZE);
}

//Normals in .blb files are already in world proportions, so they only swap axes
static glm::vec3 blbDirection(const float* values)
{
	return glm::vec3(values[0], values[2], values[1]);
}

static BrickFaceTexture textureFromLine(const std::string& line, const std::string& path, bool& warned)
{
	std::string upper = line;
	std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

	if (upper == "TEX:TOP")
		return BrickTextureTop;
	if (upper == "TEX:SIDE")
		return BrickTextureSide;
	if (upper == "TEX:BOTTOMLOOP" || upper == "TEX:BOTTOMEDGE")
		return BrickTextureBottom;
	if (upper == "TEX:RAMP")
		return BrickTextureRamp;
	if (upper.compare(0, 9, "TEX:PRINT") == 0)
		return BrickTexturePrint;

	if (!warned)
		error("Unknown brick face texture " + line + " in " + path + ", drawing it as a side");
	warned = true;
	return BrickTextureSide;
}

struct BlbVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	glm::vec4 color;
};

static void appendTriangle(std::vector<float>& group, BlbVertex corners[3])
{
	glm::vec3 geometricNormal = glm::cross(corners[1].position - corners[0].position, corners[2].position - corners[0].position);
	float area = glm::length(geometricNormal);
	if (area < 1e-6f)
		return;
	geometricNormal /= area;

	//Blockland doesn't keep a consistent winding, and swapping y and z mirrors everything anyway, so face each triangle the way its normals point
	glm::vec3 listedNormal = corners[0].normal + corners[1].normal + corners[2].normal;
	if (glm::dot(listedNormal, listedNormal) > 1e-6f && glm::dot(geometricNormal, listedNormal) < 0)
	{
		std::swap(corners[1], corners[2]);
		geometricNormal = -geometricNormal;
	}

	glm::vec3 edge1 = corners[1].position - corners[0].position;
	glm::vec3 edge2 = corners[2].position - corners[0].position;
	glm::vec2 uvEdge1 = corners[1].uv - corners[0].uv;
	glm::vec2 uvEdge2 = corners[2].uv - corners[0].uv;
	float determinant = uvEdge1.x * uvEdge2.y - uvEdge2.x * uvEdge1.y;

	glm::vec3 tangent, bitangent;
	if (std::abs(determinant) > 1e-8f)
	{
		tangent = (edge1 * uvEdge2.y - edge2 * uvEdge1.y) / determinant;
		bitangent = (edge2 * uvEdge1.x - edge1 * uvEdge2.x) / determinant;
	}

	//No usable texture coordinates, any directions along the face will do
	if (std::abs(determinant) <= 1e-8f || glm::dot(tangent, tangent) < 1e-12f || glm::dot(bitangent, bitangent) < 1e-12f)
	{
		tangent = std::abs(geometricNormal.y) < 0.9f ? glm::cross(glm::vec3(0, 1, 0), geometricNormal) : glm::cross(glm::vec3(1, 0, 0), geometricNormal);
		bitangent = glm::cross(geometricNormal, tangent);
	}

	tangent = glm::normalize(tangent);
	bitangent = glm::normalize(bitangent);

	for (int a = 0; a < 3; a++)
	{
		const BlbVertex& corner = corners[a];
		glm::vec3 normal = glm::dot(corner.normal, corner.normal) > 1e-6f ? glm::normalize(corner.normal) : geometricNormal;

		group.insert(group.end(), {
			corner.position.x, corner.position.y, corner.position.z,
			normal.x, normal.y, normal.z,
			tangent.x, tangent.y, tangent.z,
			bitangent.x, bitangent.y, bitangent.z,
			corner.uv.x, corner.uv.y,
			corner.color.r, corner.color.g, corner.color.b, corner.color.a });
	}
}

static btCollisionShape* makeCollisionShape(SpecialBrickType& type, const std::vector<std::pair<glm::vec3, glm::vec3>>& boxes)
{
	if (!boxes.empty())
	{
		//A single box filling the whole brick is most of them, fences and windows
		if (boxes.size() == 1 && glm::length(boxes[0].first) < 0.001f)
			return new btBoxShape(g2b3(boxes[0].second * 0.5f));

		btCompoundShape* compound = new btCompoundShape(false, boxes.size());
		for (const auto& [center, size] : boxes)
		{
			btBoxShape* child = new btBoxShape(g2b3(size * 0.5f));
			type.childShapes.push_back(child);

			btTransform transform = btTransform::getIdentity();
			transform.setOrigin(g2b3(center));
			compound->addChildShape(transform, child);
		}
		return compound;
	}

	btConvexHullShape points;
	for (int a = 0; a < type.vertexCount(); a++)
	{
		const float* vertex = type.vertices.data() + a * specialVertexFloats;
		points.addPoint(btVector3(vertex[0], vertex[1], vertex[2]), false);
	}
	points.recalcLocalAabb();

	btConvexHullShape* hull = nullptr;
	if (points.getNumPoints() > maxHullPoints)
	{
		btShapeHull simplified(&points);
		simplified.buildHull(points.getMargin());
		hull = new btConvexHullShape((const btScalar*)simplified.getVertexPointer(), simplified.numVertices());
	}
	else
		hull = new btConvexHullShape((const btScalar*)points.getUnscaledPoints(), points.getNumPoints());

	//The default margin sticks out past the shape and would leave a little lip where a ramp meets a flat brick
	hull->setMargin(0.01f);
	hull->recalcLocalAabb();
	return hull;
}

//Reads a special .blb's quads and collision boxes, false if it doesn't hold any usable faces
static bool loadSpecialBlb(SpecialBrickType& type, const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		error("Could not open " + path.string());
		return false;
	}

	BlbReader reader(file);
	std::string pathText = path.string();

	//Size and kind were already checked by the caller
	reader.take();
	reader.take();

	//Occupancy grid rows, then how many collision boxes, each a center then a size in studs, studs, plates from the brick's middle
	std::vector<std::pair<glm::vec3, glm::vec3>> boxes;
	while (!reader.done())
	{
		std::string lower = lowercase(reader.peek());
		if (lower.find("coverage") != std::string::npos || lower.find("quads") != std::string::npos)
			break;

		std::string line = reader.take();
		if (!isWholeNumber(line))
			continue;

		int boxCount = std::stoi(line);
		for (int a = 0; a < boxCount; a++)
		{
			float center[3], size[3];
			if (!reader.takeFloats(3, center) || !reader.takeFloats(3, size))
			{
				error("Invalid collision box in " + pathText);
				boxes.clear();
				break;
			}
			boxes.push_back({ blbPosition(center), glm::abs(blbPosition(size)) });
		}
		break;
	}

	std::vector<float> groups[BrickTextureCount];
	bool warnedTexture = false;
	bool malformed = false;

	while (!reader.done() && !malformed)
	{
		std::string header = lowercase(reader.take());
		if (header.find("quads") == std::string::npos)
			continue;

		std::string countLine = reader.take();
		if (!isWholeNumber(countLine))
			continue;

		int quadCount = std::stoi(countLine);
		if (quadCount > 10000)
		{
			error("Too many quads on one side of " + pathText);
			return false;
		}

		for (int q = 0; q < quadCount && !reader.done(); q++)
		{
			std::string line = reader.take();
			while (!reader.done() && line.compare(0, 4, "TEX:") != 0)
				line = reader.take();

			BrickFaceTexture texture = textureFromLine(line, pathText, warnedTexture);

			float values[4];
			BlbVertex corners[4];
			bool ok = reader.take() == "POSITION:";
			for (int c = 0; c < 4 && ok; c++)
			{
				ok = reader.takeFloats(3, values);
				corners[c].position = blbPosition(values);
				corners[c].color = glm::vec4(0);
			}

			ok = ok && reader.take() == "UV COORDS:";
			for (int c = 0; c < 4 && ok; c++)
			{
				ok = reader.takeFloats(2, values);
				corners[c].uv = glm::vec2(values[0], values[1]);

				//Swapping y and z mirrors the whole shape, which only shows on a print, where it would draw every letter backwards
				if (texture == BrickTexturePrint)
					corners[c].uv.x = 1.0f - corners[c].uv.x;
			}

			std::string next = ok ? reader.take() : "";
			if (next == "COLORS:")
			{
				for (int c = 0; c < 4 && ok; c++)
				{
					ok = reader.takeFloats(4, values);
					corners[c].color = glm::clamp(glm::vec4(values[0], values[1], values[2], values[3]), 0.0f, 1.0f);
					if (corners[c].color.a > 0.01f && corners[c].color.a < 0.99f)
						type.hasTransparency = true;
				}
				next = ok ? reader.take() : "";
			}

			ok = ok && next == "NORMALS:";
			for (int c = 0; c < 4 && ok; c++)
			{
				ok = reader.takeFloats(3, values);
				corners[c].normal = blbDirection(values);
			}

			if (!ok)
			{
				error("Malformed quad in " + pathText + ", loaded what came before it");
				malformed = true;
				break;
			}

			BlbVertex first[3] = { corners[0], corners[1], corners[2] };
			BlbVertex second[3] = { corners[0], corners[2], corners[3] };
			appendTriangle(groups[texture], first);
			appendTriangle(groups[texture], second);
		}
	}

	int vertexCount = 0;
	for (int g = 0; g < BrickTextureCount; g++)
	{
		type.groupFirst[g] = vertexCount;
		type.groupCount[g] = (int)(groups[g].size() / specialVertexFloats);
		vertexCount += type.groupCount[g];
		type.vertices.insert(type.vertices.end(), groups[g].begin(), groups[g].end());
	}

	if (vertexCount == 0)
	{
		error(pathText + " has no faces");
		return false;
	}

	type.shape = makeCollisionShape(type, boxes);
	return true;
}

//fxDTSBrickData datablocks found in one .cs file, with fields lowercased by name
struct BrickDatablock
{
	std::string name;
	std::unordered_map<std::string, std::string> fields;
};

//Also adds each datablock to known as it's read, so a later one can name it as its parent, even in the same file
static std::vector<BrickDatablock> readDatablocks(const std::filesystem::path& csPath, std::unordered_map<std::string, BrickDatablock>& known)
{
	std::vector<BrickDatablock> found;

	std::ifstream file(csPath, std::ios::binary);
	std::string text, line;
	while (getline(file, line))
	{
		stripCarriageReturn(line);
		size_t comment = line.find("//");
		if (comment != std::string::npos)
			line.erase(comment);
		text += line + "\n";
	}

	std::string lower = lowercase(text);
	size_t at = 0;
	while ((at = lower.find("datablock", at)) != std::string::npos)
	{
		size_t open = lower.find('(', at);
		size_t close = lower.find(')', at);
		size_t body = lower.find('{', at);
		size_t end = lower.find('}', at);
		at += 9;

		if (open == std::string::npos || close == std::string::npos || body == std::string::npos || end == std::string::npos || !(open < close && close < body && body < end))
			continue;
		if (trim(lower.substr(at, open - at)) != "fxdtsbrickdata")
			continue;

		BrickDatablock datablock;

		//datablock fxDTSBrickData(name : parent) starts with a copy of the parent's fields
		std::string names = text.substr(open + 1, close - open - 1);
		size_t colon = names.find(':');
		datablock.name = lowercase(trim(names.substr(0, colon)));
		if (colon != std::string::npos)
		{
			auto parent = known.find(lowercase(trim(names.substr(colon + 1))));
			if (parent != known.end())
				datablock.fields = parent->second.fields;
		}

		std::istringstream fields(text.substr(body + 1, end - body - 1));
		while (getline(fields, line))
		{
			size_t equals = line.find('=');
			size_t firstQuote = line.find('"');
			size_t lastQuote = line.rfind('"');
			if (equals == std::string::npos || firstQuote == std::string::npos || lastQuote <= firstQuote || firstQuote < equals)
				continue;

			datablock.fields[lowercase(trim(line.substr(0, equals)))] = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
		}

		known[datablock.name] = datablock;
		found.push_back(datablock);
		at = end;
	}

	return found;
}

void BrickTypes::load(const std::string& typesFolder)
{
	scope("BrickTypes::load");

	unsigned int startMS = SDL_GetTicks();

	basicTypes.clear();
	basicByName.clear();
	specialTypes.clear();
	specialByName.clear();

	std::error_code errorCode;
	if (!std::filesystem::is_directory(typesFolder, errorCode))
	{
		error("Brick types folder " + typesFolder + " not found");
		return;
	}

	const std::filesystem::path root(typesFolder);

	//Lowercase path relative to the types folder to the real path, so add-ons written on Windows find files whatever their case
	std::unordered_map<std::string, std::filesystem::path> filesByPath;
	std::vector<std::filesystem::path> csFiles;
	std::vector<std::filesystem::path> blbFilesInOrder;

	//Lowercase .blb file name to its path, for test.cs, outside of add-ons so their files can't shadow the basic ones
	std::unordered_map<std::string, std::filesystem::path> blbFiles;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(root, errorCode))
	{
		if (!entry.is_regular_file())
			continue;

		std::string relative = lowercase(entry.path().lexically_relative(root).generic_string());
		filesByPath[relative] = entry.path();

		std::string extension = lowercase(entry.path().extension().string());
		if (lowercase(entry.path().filename().string()) == "bricks.txt")
			csFiles.push_back(entry.path());
		else if (extension == ".blb")
		{
			blbFilesInOrder.push_back(entry.path());
			if (relative.compare(0, 7, "addons/") != 0)
				blbFiles[lowercase(entry.path().filename().string())] = entry.path();
		}
	}

	//Directory order isn't the same everywhere
	std::sort(csFiles.begin(), csFiles.end());
	std::sort(blbFilesInOrder.begin(), blbFilesInOrder.end());

	//A datablock path relative to the .cs file's folder, the add-ons folder, or Blockland's base bricks, "" if it doesn't exist
	auto resolve = [&](const std::filesystem::path& csPath, std::string value, const std::string& extension) -> std::string
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		if (value.empty())
			return "";

		std::string lowerValue = lowercase(value);
		std::filesystem::path relative;
		if (lowerValue.compare(0, 8, "add-ons/") == 0)
			relative = std::filesystem::path("addons") / value.substr(8);
		else if (lowerValue.compare(0, 17, "base/data/bricks/") == 0)
			relative = value.substr(17);
		else
			relative = csPath.parent_path().lexically_relative(root) / value;

		std::string key = lowercase(relative.lexically_normal().generic_string());
		if (lowercase(std::filesystem::path(key).extension().string()) != extension)
			key += extension;

		auto found = filesByPath.find(key);
		return found == filesByPath.end() ? "" : found->second.string();
	};

	struct NamedFile
	{
		std::string name;
		std::filesystem::path blb;
		std::string icon = "";
		std::string category = "";
		std::string subCategory = "";
		bool listed = true;
		VehiclePart vehiclePart = VehiclePart_None;
	};
	std::vector<NamedFile> namedFiles;
	std::unordered_set<std::string> claimedFiles;

	//test.cs first, so the basic sizes it lists come before any add-on's bricks in the brick selector
	std::ifstream aliases(typesFolder + "/test.cs", std::ios::binary);
	if (!aliases.is_open())
		error("Could not open " + typesFolder + "/test.cs");

	std::string line;
	while (getline(aliases, line))
	{
		stripCarriageReturn(line);

		size_t bar = line.find('|');
		if (bar == std::string::npos)
			continue;

		auto blb = blbFiles.find(lowercase(line.substr(bar + 1)));

		//Most names in the list have no .blb in the starter pack, getBasicSize falls back to parsing those names
		if (blb == blbFiles.end())
			continue;

		namedFiles.push_back({ blocklandTextToUtf8(line.substr(0, bar)), blb->second });
		claimedFiles.insert(blb->second.string());
	}

	std::unordered_map<std::string, BrickDatablock> datablocksByName;
	for (const std::filesystem::path& csPath : csFiles)
	{
		for (const BrickDatablock& datablock : readDatablocks(csPath, datablocksByName))
		{
			auto field = [&datablock](const std::string& key) -> std::string
			{
				auto value = datablock.fields.find(key);
				return value == datablock.fields.end() ? "" : value->second;
			};

			//Doors' open states and other bricks that can't be picked have no ui name, and saves store bricks by ui name
			std::string uiName = blocklandTextToUtf8(field("uiname"));
			if (uiName.empty())
				continue;

			std::string blb = resolve(csPath, field("brickfile"), ".blb");
			if (blb.empty())
			{
				error("Brick " + uiName + " in " + csPath.string() + " names a missing file " + field("brickfile"));
				continue;
			}

			//Our own field, Blockland's vehicle bricks were just shapes
			std::string part = lowercase(field("vehiclepart"));
			VehiclePart vehiclePart = VehiclePart_None;
			if (part == "wheel")
				vehiclePart = VehiclePart_Wheel;
			else if (part == "steering")
				vehiclePart = VehiclePart_Steering;
			else if (part == "seat")
				vehiclePart = VehiclePart_Seat;

			if (!claimedFiles.insert(blb).second)
			{
				//test.cs names most of the same files, and its names stay for old saves, but it has no icons: Blockland's are named after the ui name ("25 Ramp 1x.png" beside 1x3ramp.blb) so only the datablock's iconName finds them
				for (NamedFile& claimed : namedFiles)
				{
					if (claimed.blb.string() != blb)
						continue;

					if (claimed.icon.empty())
						claimed.icon = resolve(csPath, field("iconname"), ".png");

					//The wheels and such still become vehicle parts in the selector's vehicle section
					if (vehiclePart != VehiclePart_None)
					{
						claimed.vehiclePart = vehiclePart;
						claimed.category = blocklandTextToUtf8(field("category"));
						claimed.subCategory = blocklandTextToUtf8(field("subcategory"));
						claimed.listed = !field("category").empty();
					}
				}
				continue;
			}

			namedFiles.push_back({ uiName, blb, resolve(csPath, field("iconname"), ".png"), blocklandTextToUtf8(field("category")), blocklandTextToUtf8(field("subcategory")), !field("category").empty(), vehiclePart });
		}
	}

	//Some .blb files are named after their type instead of being listed anywhere, e.g. "16x32 Base.blb"
	for (const std::filesystem::path& blb : blbFilesInOrder)
	{
		if (!claimedFiles.count(blb.string()))
			namedFiles.push_back({ blocklandTextToUtf8(blb.stem().string()), blb });
	}

	int invalidSpecials = 0;

	for (NamedFile& named : namedFiles)
	{
		std::string lowerName = lowercase(named.name);
		if (basicByName.count(lowerName) || specialByName.count(lowerName))
		{
			debug("Skipping a second brick named " + named.name + " from " + named.blb.string());
			continue;
		}

		std::ifstream blbFile(named.blb, std::ios::binary);
		std::string sizeLine, kindLine;
		getline(blbFile, sizeLine);
		getline(blbFile, kindLine);
		blbFile.close();
		stripCarriageReturn(kindLine);
		kindLine = trim(kindLine);

		int width = 0, length = 0, height = 0;
		bool validSize = sscanf(sizeLine.c_str(), "%d %d %d", &width, &length, &height) == 3 &&
			width >= 1 && width <= 255 && length >= 1 && length <= 255 && height >= 1 && height <= 255;

		//Whatever the case, 1x1f.blb ships beside 1x1F.png
		std::string icon = named.icon;
		if (icon.empty())
		{
			std::filesystem::path besideBlb = named.blb.lexically_relative(root);
			besideBlb.replace_extension(".png");
			auto found = filesByPath.find(lowercase(besideBlb.generic_string()));
			if (found != filesByPath.end())
				icon = found->second.string();
		}

		if (validSize && (kindLine == "SPECIAL" || kindLine == "SPECIALBRICK"))
		{
			std::unique_ptr<SpecialBrickType> type = std::make_unique<SpecialBrickType>();
			type->uiName = named.name;
			type->category = named.category.empty() ? "Other" : named.category;
			type->subCategory = named.subCategory;
			type->listed = named.listed;
			type->vehiclePart = named.vehiclePart;
			type->blbPath = named.blb.string();
			type->iconPath = icon;
			type->width = width;
			type->height = height;
			type->length = length;

			if (!loadSpecialBlb(*type, named.blb))
			{
				invalidSpecials++;
				continue;
			}

			specialByName[lowerName] = 0;
			specialTypes.push_back(std::move(type));
			continue;
		}

		if (kindLine != "BRICK" || !validSize)
		{
			error("Invalid brick type file " + named.blb.string());
			continue;
		}

		BasicBrickType type;
		type.uiName = named.name;
		type.width = width;
		type.height = height;
		type.length = length;
		type.iconPath = icon;

		basicByName[lowerName] = basicTypes.size();
		basicTypes.push_back(type);
	}

	std::sort(specialTypes.begin(), specialTypes.end(), [](const std::unique_ptr<SpecialBrickType>& a, const std::unique_ptr<SpecialBrickType>& b)
	{
		if (a->category != b->category)
			return a->category < b->category;
		if (a->subCategory != b->subCategory)
			return a->subCategory < b->subCategory;
		return a->uiName < b->uiName;
	});

	specialByName.clear();
	for (size_t a = 0; a < specialTypes.size(); a++)
	{
		std::filesystem::path blb(specialTypes[a]->blbPath);
		specialByName.insert({ lowercase(specialTypes[a]->uiName), a });
		specialByName.insert({ lowercase(blb.filename().string()), a });
		specialByName.insert({ lowercase(blb.stem().string()), a });
	}

	info("Loaded " + std::to_string(basicTypes.size()) + " basic and " + std::to_string(specialTypes.size()) + " special brick types in " +
		std::to_string(SDL_GetTicks() - startMS) + "ms");
	if (invalidSpecials > 0)
		error(std::to_string(invalidSpecials) + " special brick files couldn't be loaded");
}

bool BrickTypes::getBasicSize(const std::string& name, int& width, int& height, int& length) const
{
	std::string lowerName = lowercase(name);

	auto found = basicByName.find(lowerName);
	if (found != basicByName.end())
	{
		const BasicBrickType& type = basicTypes[found->second];
		width = type.width;
		height = type.height;
		length = type.length;
		return true;
	}

	if (specialByName.count(lowerName))
		return false;

	return parseBlocklandBasicName(lowerName, width, height, length);
}

int BrickTypes::findSpecial(const std::string& name) const
{
	auto found = specialByName.find(lowercase(name));
	return found == specialByName.end() ? -1 : (int)found->second;
}

const SpecialBrickType* BrickTypes::getSpecial(int index) const
{
	if (index < 0 || index >= (int)specialTypes.size())
		return nullptr;
	return specialTypes[index].get();
}
