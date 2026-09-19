#include "ZipFile.h"
#include "FileFunctions.h"

#include <fstream>
#include <zlib.h>

//The four signatures the format is found by, least significant byte first on disk
static constexpr uint32_t endOfDirectorySignature = 0x06054b50;
static constexpr uint32_t directoryEntrySignature = 0x02014b50;
static constexpr uint32_t localHeaderSignature = 0x04034b50;

//A zip comment is a 16 bit length, so the end record is never further back than this from the end of the file
static constexpr size_t furthestEndRecord = 0xFFFF + 22;

//What a member is read and written in at a time
static constexpr size_t zipChunkBytes = 64 * 1024;

//Zip stores everything little endian, whatever the machine reading it does
static uint16_t readU16(const char* from)
{
	return (uint16_t)((unsigned char)from[0] | ((unsigned char)from[1] << 8));
}

static uint32_t readU32(const char* from)
{
	return (uint32_t)((unsigned char)from[0]) | ((uint32_t)((unsigned char)from[1]) << 8) |
		((uint32_t)((unsigned char)from[2]) << 16) | ((uint32_t)((unsigned char)from[3]) << 24);
}

//One file the zip's central directory lists
struct ZipMember
{
	std::string name = "";
	uint16_t method = 0;
	uint32_t compressedSize = 0;
	uint32_t uncompressedSize = 0;
	uint32_t localHeaderOffset = 0;
};

/*
	Zips are read from the end: the last thing in the file says how many members there are and where
	their headers start. Anything could be in front of that (a self extracting zip is an executable with
	an archive glued on) so the record is searched for rather than assumed to be at a fixed place
*/
static bool findCentralDirectory(std::ifstream& file, size_t fileBytes, uint32_t& entries, uint32_t& directoryOffset, std::string& problem)
{
	size_t lookBytes = std::min(fileBytes, furthestEndRecord);
	if (lookBytes < 22)
	{
		problem = "file is too small to be a zip";
		return false;
	}

	std::vector<char> tail(lookBytes);
	file.seekg(fileBytes - lookBytes, std::ios::beg);
	file.read(tail.data(), lookBytes);
	if (!file)
	{
		problem = "could not read the end of the file";
		return false;
	}

	//Backwards, so a comment that happens to contain the signature doesn't win over the real record
	for (size_t at = lookBytes - 22; ; at--)
	{
		if (readU32(tail.data() + at) == endOfDirectorySignature)
		{
			entries = readU16(tail.data() + at + 10);
			directoryOffset = readU32(tail.data() + at + 16);

			//Both of these are what a zip64 archive puts in the 32 bit fields it has outgrown
			if (entries == 0xFFFF || directoryOffset == 0xFFFFFFFF)
			{
				problem = "zip64 archives aren't supported";
				return false;
			}

			if (directoryOffset >= fileBytes)
			{
				problem = "the file list starts past the end of the file";
				return false;
			}

			return true;
		}

		if (at == 0)
			break;
	}

	problem = "not a zip file";
	return false;
}

//Reads the central directory, one entry per file in the archive
static bool readMembers(std::ifstream& file, uint32_t entries, uint32_t directoryOffset, std::vector<ZipMember>& members, std::string& problem)
{
	file.seekg(directoryOffset, std::ios::beg);

	for (uint32_t which = 0; which < entries; which++)
	{
		char header[46];
		file.read(header, sizeof(header));
		if (!file || readU32(header) != directoryEntrySignature)
		{
			problem = "the file list is damaged";
			return false;
		}

		ZipMember member;
		member.method = readU16(header + 10);
		member.compressedSize = readU32(header + 20);
		member.uncompressedSize = readU32(header + 24);
		member.localHeaderOffset = readU32(header + 42);

		uint16_t nameBytes = readU16(header + 28);
		uint16_t extraBytes = readU16(header + 30);
		uint16_t commentBytes = readU16(header + 32);

		if (member.compressedSize == 0xFFFFFFFF || member.uncompressedSize == 0xFFFFFFFF || member.localHeaderOffset == 0xFFFFFFFF)
		{
			problem = "zip64 archives aren't supported";
			return false;
		}

		if (nameBytes > 0)
		{
			std::vector<char> name(nameBytes);
			file.read(name.data(), nameBytes);
			member.name = std::string(name.data(), nameBytes);
		}

		file.seekg(extraBytes + commentBytes, std::ios::cur);
		if (!file)
		{
			problem = "the file list is damaged";
			return false;
		}

		members.push_back(member);
	}

	return true;
}

/*
	A member's path as it goes on disk: forward slashes, and nothing that would climb out of the folder
	it's being unpacked into. Empty for a path that isn't allowed, which is refused rather than skipped
*/
static std::string safeMemberPath(const std::string& name)
{
	std::string path = name;
	for (char& letter : path)
	{
		if (letter == '\\')
			letter = '/';
	}

	//An absolute path, a drive letter, or a leading .. is somebody aiming at a file outside the add-on
	if (path.length() < 1 || path[0] == '/' || path.find(':') != std::string::npos)
		return "";

	size_t at = 0;
	while (at < path.length())
	{
		size_t slash = path.find('/', at);
		std::string part = path.substr(at, slash == std::string::npos ? std::string::npos : slash - at);

		if (part == "..")
			return "";

		if (slash == std::string::npos)
			break;

		at = slash + 1;
	}

	return path;
}

//The one folder everything in the archive sits in, or empty if there isn't one
static std::string sharedRootFolder(const std::vector<ZipMember>& members)
{
	std::string root = "";

	for (const ZipMember& member : members)
	{
		//A directory entry of the root folder itself doesn't decide anything on its own
		size_t slash = member.name.find_first_of("/\\");
		if (slash == std::string::npos)
			return "";

		std::string first = member.name.substr(0, slash);
		if (root.length() < 1)
			root = first;
		else if (root != first)
			return "";
	}

	return root;
}

//Copies a stored member straight across, or inflates a deflated one, into an already open file
static bool writeMember(std::ifstream& file, const ZipMember& member, std::ofstream& out, std::string& problem)
{
	if (member.method == 0)
	{
		std::vector<char> chunk(zipChunkBytes);
		uint32_t left = member.compressedSize;

		while (left > 0)
		{
			size_t take = std::min((size_t)left, zipChunkBytes);
			file.read(chunk.data(), take);
			if (!file)
			{
				problem = "the file ends in the middle of " + member.name;
				return false;
			}

			out.write(chunk.data(), take);
			left -= (uint32_t)take;
		}

		return out.good();
	}

	if (member.method != Z_DEFLATED)
	{
		problem = member.name + " is packed a way this can't read (method " + std::to_string(member.method) + ")";
		return false;
	}

	z_stream stream = {};
	//A negative window means the deflate data on its own, without the zlib header a .gz would have
	if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
	{
		problem = "could not start unpacking";
		return false;
	}

	std::vector<char> packed(zipChunkBytes);
	std::vector<char> unpacked(zipChunkBytes);
	uint32_t left = member.compressedSize;
	bool done = false;

	while (!done)
	{
		size_t take = std::min((size_t)left, zipChunkBytes);
		if (take > 0)
		{
			file.read(packed.data(), take);
			if (!file)
			{
				inflateEnd(&stream);
				problem = "the file ends in the middle of " + member.name;
				return false;
			}

			left -= (uint32_t)take;
		}

		stream.next_in = (Bytef*)packed.data();
		stream.avail_in = (uInt)take;

		//Everything this batch of packed bytes turned into, which can be a good deal more than went in
		do
		{
			stream.next_out = (Bytef*)unpacked.data();
			stream.avail_out = (uInt)unpacked.size();

			int result = inflate(&stream, Z_NO_FLUSH);
			if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			{
				inflateEnd(&stream);
				problem = member.name + " is damaged";
				return false;
			}

			out.write(unpacked.data(), unpacked.size() - stream.avail_out);
			if (!out)
			{
				inflateEnd(&stream);
				problem = "could not write " + member.name;
				return false;
			}

			if (result == Z_STREAM_END)
			{
				done = true;
				break;
			}
		} while (stream.avail_out == 0);

		//Ran out of packed bytes without the stream ending, so the member is cut short
		if (!done && left < 1 && take < 1)
		{
			inflateEnd(&stream);
			problem = member.name + " is cut short";
			return false;
		}
	}

	inflateEnd(&stream);
	return true;
}

bool extractZipFile(const std::string& zipPath, const std::string& destination, std::string& problem)
{
	scope("extractZipFile");

	std::error_code trouble;
	size_t fileBytes = (size_t)std::filesystem::file_size(zipPath, trouble);
	if (trouble)
	{
		problem = "no such file";
		return false;
	}

	std::ifstream file(zipPath, std::ios::binary);
	if (!file.is_open())
	{
		problem = "could not open the file";
		return false;
	}

	uint32_t entries = 0;
	uint32_t directoryOffset = 0;
	if (!findCentralDirectory(file, fileBytes, entries, directoryOffset, problem))
		return false;

	std::vector<ZipMember> members;
	if (!readMembers(file, entries, directoryOffset, members, problem))
		return false;

	//An add-on zipped with its own folder inside it unpacks as if it had been zipped without one
	std::string root = sharedRootFolder(members);
	size_t skip = root.length() > 0 ? root.length() + 1 : 0;

	if (!std::filesystem::create_directories(destination, trouble) && trouble)
	{
		problem = "could not make the folder " + destination;
		return false;
	}

	for (const ZipMember& member : members)
	{
		std::string path = safeMemberPath(member.name);
		if (path.length() < 1)
		{
			problem = member.name + " isn't a path that can go in a folder";
			return false;
		}

		path = path.substr(std::min(skip, path.length()));
		if (path.length() < 1)
			continue;

		std::filesystem::path full = std::filesystem::path(destination) / path;

		//Folders are listed with a trailing slash, and are made anyway by the files that go in them
		if (path.back() == '/')
		{
			std::filesystem::create_directories(full, trouble);
			continue;
		}

		std::filesystem::create_directories(full.parent_path(), trouble);

		//Past the local header, which repeats the name and may carry extra fields the directory doesn't
		file.seekg(member.localHeaderOffset, std::ios::beg);
		char header[30];
		file.read(header, sizeof(header));
		if (!file || readU32(header) != localHeaderSignature)
		{
			problem = "the header for " + member.name + " is damaged";
			return false;
		}

		file.seekg(readU16(header + 26) + readU16(header + 28), std::ios::cur);

		std::ofstream out(full, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			problem = "could not write " + full.generic_string();
			return false;
		}

		if (!writeMember(file, member, out, problem))
			return false;
	}

	return true;
}
