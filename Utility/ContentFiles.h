#pragma once

#include "../LandOfDran.h"

#include <set>
#include <unordered_map>

/*
	A server can offer the files its add-ons use to clients that don't have them, see Lua's addServerFile.
	Only assets ever move: sounds, models, textures, and the descriptor files a model needs to load at all.
	Nothing that runs (.lua, .cs) is ever offered by a server or written by a client, whatever it claims to be

	The server side of this lives in Networking/ServerFiles.h, the client side in the ContentFiles below
*/
enum ContentFileKind : unsigned char
{
	ContentFileUnknown = 0,		//Not a kind of file that gets sent, so never offered and never accepted
	ContentFileAudio = 1,		//.wav .ogg .mp3
	ContentFileModel = 2,		//.dts .fbx .obj .blend, whatever Assimp reads
	ContentFileTexture = 3,		//.png .jpg .jpeg .bmp
	ContentFileDescriptor = 4,	//.txt model and material descriptors, see Graphics/Mesh.h and Graphics/Material.h
	ContentFileVideo = 5		//.webm, which is what a print that plays rather than sits still is, see Graphics/PrintVideos.h
};

//ContentFileUnknown for anything clients don't accept, which is every extension not listed above
ContentFileKind contentFileKind(const std::string& path);

//What the download window's Type column says
std::string contentFileKindName(ContentFileKind kind);

//"12 Kb", for the download window
std::string contentFileSizeName(uint32_t bytes);

//The most one file may be, big enough for the largest models add-ons ship
constexpr uint32_t maxContentFileBytes = 16 * 1024 * 1024;

//The most files one server may offer
constexpr uint32_t maxContentFiles = 4096;

//A server's copy of a file is kept here rather than next to the player's own, which is never written over
constexpr const char* contentDownloadFolder = "Downloads/";

//One file a server offers as a client joins, as a ServerFileList packet describes it
struct ContentFile
{
	//What a ServerFileData packet and a request for this file name it by
	uint16_t id = 0;
	//Where the server has it, and where the client keeps a copy of its own if it has one
	std::string path = "";
	ContentFileKind kind = ContentFileUnknown;
	uint32_t size = 0;
	//CRC32 of the whole file, the same one getFileChecksum gives
	uint32_t checksum = 0;
};

/*
	What the server we're joining offered us, which of those files we still need, and where each one is
	loaded from once we have it

	There's one for the whole program, see contentFiles(), because the loaders that have to look a path up
	(models, textures, sounds) are nowhere near the packet that decided where it lives
*/
class ContentFiles
{
	//Everything the server listed, in the order it listed them
	std::vector<ContentFile> offered;

	//How many entries the server says its list has: ours is complete once offered has that many
	uint32_t listSize = 0;
	bool listStarted = false;
	//Set once the completed list has been compared against what we have, so it's only done once
	bool listChecked = false;

	//The ones we have no matching copy of, which is what the download window lists
	std::vector<ContentFile> missing;

	//Paths loaded out of the download folder instead of where the server named them, see resolve
	std::unordered_map<std::string, std::string> redirects;

	//Folders with a downloaded file in them, so a shape's textures are looked for beside it, see resolveFolder
	std::set<std::string> redirectedFolders;

	//A file we asked for that hasn't finished arriving
	struct Incoming
	{
		ContentFile file;
		std::string bytes = "";
	};
	std::unordered_map<uint16_t, Incoming> incoming;

	//Bytes we asked for and bytes written so far, for the progress bar
	uint64_t downloadTotal = 0;
	uint64_t downloadDone = 0;

	//Something was written and nothing is still on its way, see takeDownloadsFinished
	bool finishedDownloading = false;

	//The server finished a batch of files and is waiting to hear that we took it, see takeWantsMore
	bool wantsMore = false;

	//Route this path at the download folder from here on
	void redirect(const std::string& path);

	public:

	//Forget everything about the server we were on, on leaving it
	void clear();

	//A ServerFileList packet says the whole list is this many files
	void startList(uint32_t total);

	//One entry of the list, ignored if it isn't a kind of file we accept or the list is already full
	void addOffer(const ContentFile& file);

	//Have all the entries the server said there would be arrived
	bool listComplete() const { return listStarted && offered.size() >= listSize; }

	//Was a list received at all, as opposed to a server that offers nothing
	bool hasList() const { return listStarted; }

	/*
		Compare the finished list against what we have: files we already have an identical copy of are
		routed and taken out, so only the ones that need downloading are left in missing
		Returns true if anything is missing, i.e. the download window has something to show
	*/
	bool checkList();
	bool isChecked() const { return listChecked; }

	const std::vector<ContentFile>& getMissing() const { return missing; }

	//Start waiting for these files, which are the IDs we just asked the server for
	void beginDownload(const std::vector<uint16_t>& ids);

	//Part of a file from a ServerFileData packet, written out and routed once all of it is here
	//Returns false if the packet was nothing we asked for or the file came out wrong
	bool takeChunk(uint16_t id, uint32_t total, uint32_t offset, const unsigned char* data, size_t length);

	//Are we still waiting on files we asked for
	bool downloading() const { return !incoming.empty(); }

	/*
		Files are still on their way, or the last of them landed and what they brought hasn't been taken
		in yet. Anything that would complain about something a server has and we don't waits for this to
		be false, since one of those files may well be the answer, see BrickPrintTypesPacket
	*/
	bool downloadsPending() const { return !incoming.empty() || finishedDownloading; }

	/*
		True once after the last file we asked for was written, and only when something really arrived
		What a server sent can be a print, a face or a shirt, which the client finds by looking through
		folders rather than by a path a packet names, so those have to be gone through again, see
		LoopClient::loadDecalArray
	*/
	bool takeDownloadsFinished();

	/*
		The server sends the files a batch at a time and waits after each one, so a big download doesn't
		queue up in its memory all at once. This says a batch has arrived and it's waiting on us
	*/
	void batchArrived() { wantsMore = true; }

	//True once after a batch arrived, when the server should be asked for the next one
	bool takeWantsMore();

	//0 to 1 of the bytes we asked for that have arrived
	float getProgress() const;
	uint64_t getDownloadTotal() const { return downloadTotal; }
	uint64_t getDownloadDone() const { return downloadDone; }

	//Where to actually load a file the server named, which is the download folder for one we got from it
	std::string resolve(const std::string& path) const;

	//Same for a folder being looked through, see DtsShape's hunt for the texture beside a shape
	std::string resolveFolder(const std::string& folder) const;
};

//The one for the whole program
ContentFiles& contentFiles();

//Short hand for contentFiles().resolve, which is what asset loading calls
std::string contentPath(const std::string& path);
