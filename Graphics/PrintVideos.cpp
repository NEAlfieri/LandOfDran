#include "PrintVideos.h"

#include "Texture.h"

//Videos are kept in memory to loop without going back to disk, so there's a limit on how much of them
static constexpr size_t maxTotalVideoBytes = 256 * 1024 * 1024;

bool PrintVideos::add(int print, const PrintType& type, int decalLayer, std::shared_ptr<TextureManager> textures)
{
	scope("PrintVideos::add");

	if (!VideoPlayer::isSupported())
	{
		//Said once, not once per video, see load in LoopClient
		return false;
	}

	Video video;
	video.print = print;
	video.decalLayer = decalLayer;
	video.player = std::make_unique<VideoPlayer>();

	if (!video.player->open(type.filePath, (int)textures->getDecalSize()))
		return false;

	if (loadedBytes + video.player->getBytes() > maxTotalVideoBytes)
	{
		error("Video prints already hold " + std::to_string(loadedBytes / (1024 * 1024)) + "MB, so " + type.name + " was skipped");
		return false;
	}

	const std::vector<unsigned char>& pixels = video.player->getPixels();
	int size = video.player->getSize();

	//finalizeDecals makes the smaller levels of every layer at once, so this first frame doesn't do its own
	if (!textures->setDecalPixels(pixels.data(), size, size, decalLayer, false))
		return false;

	info("Print " + type.name + " is a " + std::to_string(video.player->getVideoWidth()) + "x" + std::to_string(video.player->getVideoHeight()) +
		" video, " + std::to_string(video.player->getFrameCount()) + " frames over " + std::to_string(video.player->getDuration()) + "s");

	loadedBytes += video.player->getBytes();
	videos.push_back(std::move(video));
	return true;
}

void PrintVideos::upload(const Video& video, std::shared_ptr<TextureManager> textures) const
{
	const std::vector<unsigned char>& pixels = video.player->getPixels();
	int size = video.player->getSize();
	textures->setDecalPixels(pixels.data(), size, size, video.decalLayer, true);
}

void PrintVideos::update(float deltaSeconds, std::shared_ptr<TextureManager> textures, const std::function<bool(uint16_t)>& isUsed)
{
	playingCount = 0;
	wantedCount = 0;

	for (Video& video : videos)
	{
		//Print IDs are one more than the index, see Brick::printID
		bool wanted = video.player && video.player->isOpen() && isUsed((uint16_t)(video.print + 1));
		if (wanted)
			wantedCount++;

		//Over the limit it keeps whatever frame it was left on, like a paused screen
		if (!wanted || playingCount >= maxPlaying)
			continue;

		playingCount++;

		if (video.player->advance(deltaSeconds))
			upload(video, textures);
	}
}

std::string PrintVideos::getStatus() const
{
	if (videos.empty())
		return "";

	return std::to_string(playingCount) + "/" + std::to_string(wantedCount) + " video prints playing, " + std::to_string(videos.size()) + " loaded";
}
