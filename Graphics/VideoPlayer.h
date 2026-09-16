#pragma once

#include "../LandOfDran.h"

/*
	Plays the video track of a .webm file, looping, as RGBA pixels to put in a texture

	The file is read once into memory, walked to index every frame of its video track, and decoded
	frame by frame with libvpx as advance is called. Only the video matters: any audio, subtitle,
	or alpha track in the file is left alone, see Graphics/PrintVideos.h for what plays these

	Built without libvpx (see CMakeLists.txt) every open fails and says so, and nothing else changes
*/
class VideoPlayer
{
	//Where one video frame sits in the file, and when it's shown
	struct Frame
	{
		size_t offset = 0;
		uint32_t size = 0;
		double time = 0;
	};

	//The whole file, since looping means walking it again and a print video is small
	std::vector<unsigned char> bytes;

	std::vector<Frame> frames;

	//How long one time around is, in seconds, which is the last frame's time plus how long it's up
	double duration = 0;

	//Pixels the video is decoded at, and the square it's scaled into, see open
	int videoWidth = 0;
	int videoHeight = 0;
	int outputSize = 0;

	//Whether the video track is VP9 rather than VP8, which picks the decoder
	bool codecIsVp9 = false;

	//Current RGBA pixels, outputSize by outputSize, empty until the first frame is decoded
	std::vector<unsigned char> pixels;

	//Seconds into the loop, and the frame getPixels is showing, -1 before the first one
	double playTime = 0;
	int shownFrame = -1;

	//The libvpx decoder, so this header doesn't need libvpx's, nullptr if it couldn't start
	struct Decoder;
	Decoder* decoder = nullptr;

	std::string path = "";

	//Reads the file into bytes and fills in the track, its size, and every frame. False with an error logged
	bool readFile(const std::string& filePath);

	//Decodes one frame, turning it into pixels only if convert is set, false if libvpx didn't like it
	bool decodeFrame(const Frame& frame, bool convert);

	public:

	/*
		Opens a .webm and decodes its first frame
		size is the square of RGBA pixels each frame is scaled into, since that's what a decal wants
		False, with an error logged, if the file is missing, isn't a webm with a VP8 or VP9 video track, or libvpx isn't here
	*/
	bool open(const std::string& filePath, int size);

	/*
		Moves playback on by seconds, decoding whatever frames that passes and looping at the end
		True if getPixels is showing a different frame than it was, so it only goes to the texture when it changes
		Decoding is capped per call, so a long hitch makes the video run slow for a moment rather than
		spending the whole frame catching up, and frames are never skipped: they each need the one before
	*/
	bool advance(double seconds);

	//The current frame, size by size RGBA, empty before the first frame is decoded
	const std::vector<unsigned char>& getPixels() const { return pixels; }

	int getSize() const { return outputSize; }

	//What the file itself is, for the debug menu and log lines
	int getVideoWidth() const { return videoWidth; }
	int getVideoHeight() const { return videoHeight; }
	size_t getFrameCount() const { return frames.size(); }

	//How much of the file is being held in memory, since looping means walking it again
	size_t getBytes() const { return bytes.size(); }
	double getDuration() const { return duration; }

	bool isOpen() const { return decoder != nullptr && !frames.empty(); }

	//Whether this build can play videos at all, see CMakeLists.txt
	static bool isSupported();

	VideoPlayer() = default;
	VideoPlayer(const VideoPlayer&) = delete;
	VideoPlayer& operator=(const VideoPlayer&) = delete;
	~VideoPlayer();
};
