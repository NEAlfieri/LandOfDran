#include "ContentFiles.h"

/*
	Paths reach here written all sorts of ways: a descriptor file inside an add-on points at its material
	with a path relative to its own folder, so what gets opened is something like
	"Add-ons/Weapon_Gun/../../Assets/cube/cube.fbx" while the server listed the plain path. Everything
	that goes into the redirect table, and everything looked up in it, is flattened the same way first
*/
static std::string normalizeContentPath(const std::string& path)
{
	if (path.empty())
		return path;

	std::error_code problem;
	std::filesystem::path parts = std::filesystem::path(path).lexically_normal();
	std::string ret = parts.generic_string();

	//"./something" and "something" are the same file
	if (ret.rfind("./", 0) == 0)
		ret = ret.substr(2);

	return ret;
}

ContentFileKind contentFileKind(const std::string& path)
{
	std::string extension = lowercase(std::filesystem::path(path).extension().string());

	if (extension == ".wav" || extension == ".ogg" || extension == ".mp3")
		return ContentFileAudio;
	if (extension == ".dts" || extension == ".fbx" || extension == ".obj" || extension == ".dae" || extension == ".blend")
		return ContentFileModel;
	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp")
		return ContentFileTexture;
	if (extension == ".txt")
		return ContentFileDescriptor;
	if (extension == ".webm")
		return ContentFileVideo;

	return ContentFileUnknown;
}

std::string contentFileKindName(ContentFileKind kind)
{
	switch (kind)
	{
		case ContentFileAudio:		return "Audio";
		case ContentFileModel:		return "Model";
		case ContentFileTexture:	return "Texture";
		case ContentFileDescriptor:	return "Descriptor";
		case ContentFileVideo:		return "Video";
		default:					return "Unknown";
	}
}

std::string contentFileSizeName(uint32_t bytes)
{
	if (bytes >= 1024 * 1024)
		return std::to_string(bytes / (1024 * 1024)) + " Mb";
	if (bytes >= 1024)
		return std::to_string(bytes / 1024) + " Kb";
	return std::to_string(bytes) + " Bytes";
}

ContentFiles& contentFiles()
{
	static ContentFiles files;
	return files;
}

std::string contentPath(const std::string& path)
{
	return contentFiles().resolve(path);
}

void ContentFiles::clear()
{
	offered.clear();
	listSize = 0;
	listStarted = false;
	listChecked = false;
	missing.clear();
	redirects.clear();
	redirectedFolders.clear();
	incoming.clear();
	downloadTotal = 0;
	downloadDone = 0;
	finishedDownloading = false;
	wantsMore = false;
}

bool ContentFiles::takeWantsMore()
{
	if (!wantsMore)
		return false;

	wantsMore = false;
	return true;
}

bool ContentFiles::takeDownloadsFinished()
{
	if (!finishedDownloading)
		return false;

	finishedDownloading = false;
	return true;
}

void ContentFiles::startList(uint32_t total)
{
	if (listStarted)
		return;

	listStarted = true;
	listSize = std::min(total, maxContentFiles);
}

void ContentFiles::addOffer(const ContentFile& file)
{
	if (offered.size() >= listSize)
		return;

	//A server that names a file we'd never load, or one that could reach outside the game's folder, gets ignored rather than kicking us
	if (file.kind == ContentFileUnknown || file.kind != contentFileKind(file.path))
	{
		error("Server offered a kind of file clients don't take: " + file.path);
		//Still counted, or the list would never look complete
		offered.push_back(ContentFile());
		return;
	}

	if (!isPathInsideGameFolder(file.path) || file.size < 1 || file.size > maxContentFileBytes)
	{
		error("Server offered a file with a bad path or size: " + file.path);
		offered.push_back(ContentFile());
		return;
	}

	offered.push_back(file);
}

void ContentFiles::redirect(const std::string& path)
{
	std::string flat = normalizeContentPath(path);
	redirects[flat] = contentDownloadFolder + flat;

	//getFolderFromPath turns a file with no folder at all into "/", which is nobody's folder
	if (flat.find('/') != std::string::npos)
		redirectedFolders.insert(getFolderFromPath(flat));
}

bool ContentFiles::checkList()
{
	if (listChecked)
		return !missing.empty();

	listChecked = true;
	missing.clear();

	for (const ContentFile& file : offered)
	{
		if (file.kind == ContentFileUnknown)
			continue;

		//Our own copy, which is always the one that gets loaded when it matches
		if (std::filesystem::exists(file.path) &&
			GetFileSize(file.path) == (long)file.size &&
			getFileChecksum(file.path.c_str()) == file.checksum)
			continue;

		//One we downloaded from this server, or another that used the same file, on an earlier visit
		std::string downloaded = contentDownloadFolder + normalizeContentPath(file.path);
		if (std::filesystem::exists(downloaded) &&
			GetFileSize(downloaded) == (long)file.size &&
			getFileChecksum(downloaded.c_str()) == file.checksum)
		{
			redirect(file.path);
			continue;
		}

		missing.push_back(file);
	}

	return !missing.empty();
}

void ContentFiles::beginDownload(const std::vector<uint16_t>& ids)
{
	incoming.clear();
	downloadTotal = 0;
	downloadDone = 0;

	for (uint16_t id : ids)
	{
		for (const ContentFile& file : missing)
		{
			if (file.id != id)
				continue;

			Incoming& waiting = incoming[id];
			waiting.file = file;
			waiting.bytes.clear();
			//Room is made for a file as its first part turns up, not for all of them now: a hundred
			//megabyte download would otherwise reserve a hundred megabytes before a byte of it arrived
			downloadTotal += file.size;
			break;
		}
	}
}

bool ContentFiles::takeChunk(uint16_t id, uint32_t total, uint32_t offset, const unsigned char* data, size_t length)
{
	auto found = incoming.find(id);
	if (found == incoming.end())
		return false;

	Incoming& waiting = found->second;

	//Not the file we asked for, or bytes that don't carry on from the ones before them
	if (total != waiting.file.size || offset != waiting.bytes.size() || waiting.bytes.size() + length > total)
	{
		error("Bad part of " + waiting.file.path + " from the server, giving up on it");
		downloadTotal -= waiting.file.size;
		incoming.erase(found);
		return false;
	}

	if (offset == 0)
		waiting.bytes.reserve(total);

	waiting.bytes.append((const char*)data, length);
	downloadDone += length;

	if (waiting.bytes.size() < total)
		return true;

	//All of it is here: it only gets written if it really is the file the list described
	ContentFile file = waiting.file;
	std::string bytes;
	bytes.swap(waiting.bytes);
	incoming.erase(found);

	if (getBufferChecksum(bytes.data(), bytes.size()) != file.checksum)
	{
		error("Server's " + file.path + " didn't match the checksum it listed, not keeping it");
		return false;
	}

	std::string writeTo = contentDownloadFolder + normalizeContentPath(file.path);

	std::error_code problem;
	std::string folder = getFolderFromPath(writeTo);
	if (!folder.empty())
		std::filesystem::create_directories(folder, problem);

	std::ofstream out(writeTo, std::ios::binary);
	if (!out.is_open() || !out.write(bytes.data(), bytes.size()))
	{
		error("Couldn't write " + writeTo);
		return false;
	}
	out.close();

	info("Downloaded " + file.path);
	redirect(file.path);

	//That was the last one we were waiting for
	if (incoming.empty())
		finishedDownloading = true;

	return true;
}

float ContentFiles::getProgress() const
{
	if (downloadTotal < 1)
		return 1.0f;
	return std::min(1.0f, (float)((double)downloadDone / (double)downloadTotal));
}

std::string ContentFiles::resolve(const std::string& path) const
{
	if (redirects.empty() || path.empty())
		return path;

	auto found = redirects.find(normalizeContentPath(path));
	if (found == redirects.end())
		return path;

	return found->second;
}

std::string ContentFiles::resolveFolder(const std::string& folder) const
{
	if (redirectedFolders.empty() || folder.empty())
		return folder;

	std::string flat = normalizeContentPath(folder);
	//getFolderFromPath keeps the trailing slash, the set doesn't care either way as long as both sides match
	if (redirectedFolders.find(flat) == redirectedFolders.end())
		return folder;

	return contentDownloadFolder + flat;
}
