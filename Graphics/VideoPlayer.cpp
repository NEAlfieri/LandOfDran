#include "VideoPlayer.h"

#ifdef HAVE_LIBVPX
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#endif

/*
	Matroska (which webm is a subset of) is a tree of elements, each an ID then a size then its body,
	both written as variable length integers. Only the handful of elements below matter here, and
	everything else is skipped by its size, so this reads any webm without knowing the rest of the format
	See https://www.matroska.org/technical/elements.html
*/
static constexpr uint32_t idSegment = 0x18538067;
static constexpr uint32_t idInfo = 0x1549A966;
static constexpr uint32_t idTimecodeScale = 0x2AD7B1;
static constexpr uint32_t idDuration = 0x4489;
static constexpr uint32_t idTracks = 0x1654AE6B;
static constexpr uint32_t idTrackEntry = 0xAE;
static constexpr uint32_t idTrackNumber = 0xD7;
static constexpr uint32_t idTrackType = 0x83;
static constexpr uint32_t idCodecID = 0x86;
static constexpr uint32_t idDefaultDuration = 0x23E383;
static constexpr uint32_t idVideo = 0xE0;
static constexpr uint32_t idPixelWidth = 0xB0;
static constexpr uint32_t idPixelHeight = 0xBA;
static constexpr uint32_t idCluster = 0x1F43B675;
static constexpr uint32_t idTimecode = 0xE7;
static constexpr uint32_t idSimpleBlock = 0xA3;
static constexpr uint32_t idBlockGroup = 0xA0;
static constexpr uint32_t idBlock = 0xA1;

static constexpr uint64_t trackTypeVideo = 1;

//Biggest .webm that will be read into memory, well over what a print's worth of video needs
static constexpr size_t maxVideoBytes = 32 * 1024 * 1024;

//Frames one advance call will decode before letting the video fall behind instead, see VideoPlayer::advance
static constexpr int maxDecodesPerAdvance = 6;

//An element's ID keeps the marker bits of its first byte, so the numbers above are the ones the spec lists
static bool readElementId(const unsigned char* data, size_t end, size_t& at, uint32_t& id)
{
	if (at >= end)
		return false;

	unsigned char first = data[at];
	int length = (first & 0x80) ? 1 : (first & 0x40) ? 2 : (first & 0x20) ? 3 : (first & 0x10) ? 4 : 0;
	if (length == 0 || at + length > end)
		return false;

	id = 0;
	for (int a = 0; a < length; a++)
		id = (id << 8) | data[at + a];

	at += length;
	return true;
}

//A size drops its marker bit, and one that's all ones means the element runs until something that can't be inside it
static bool readElementSize(const unsigned char* data, size_t end, size_t& at, uint64_t& size, bool& unknown)
{
	if (at >= end)
		return false;

	unsigned char first = data[at];
	int length = 0;
	for (int bit = 0; bit < 8; bit++)
	{
		if (first & (0x80 >> bit))
		{
			length = bit + 1;
			break;
		}
	}

	if (length == 0 || at + length > end)
		return false;

	uint64_t value = first & (0xFF >> length);
	uint64_t ones = (0xFF >> length);
	for (int a = 1; a < length; a++)
	{
		value = (value << 8) | data[at + a];
		ones = (ones << 8) | 0xFF;
	}

	at += length;
	size = value;
	unknown = value == ones;
	return true;
}

//The track number at the start of a block is written like a size
static bool readVarInt(const unsigned char* data, size_t end, size_t& at, uint64_t& value)
{
	bool unknown;
	return readElementSize(data, end, at, value, unknown);
}

//Element bodies: an unsigned integer is big endian in as few bytes as it needs, a float is 4 or 8 bytes
static uint64_t readUnsignedBody(const unsigned char* data, size_t at, uint64_t size)
{
	uint64_t value = 0;
	for (uint64_t a = 0; a < size && a < 8; a++)
		value = (value << 8) | data[at + a];
	return value;
}

static double readFloatBody(const unsigned char* data, size_t at, uint64_t size)
{
	if (size == 4)
	{
		uint32_t bits = (uint32_t)readUnsignedBody(data, at, 4);
		float value;
		memcpy(&value, &bits, sizeof(float));
		return value;
	}

	if (size == 8)
	{
		uint64_t bits = readUnsignedBody(data, at, 8);
		double value;
		memcpy(&value, &bits, sizeof(double));
		return value;
	}

	return 0;
}

//Kept out of the header so nothing else has to have libvpx's
struct VideoPlayer::Decoder
{
#ifdef HAVE_LIBVPX
	vpx_codec_ctx_t codec = {};
#endif
	bool started = false;
};

bool VideoPlayer::isSupported()
{
#ifdef HAVE_LIBVPX
	return true;
#else
	return false;
#endif
}

bool VideoPlayer::readFile(const std::string& filePath)
{
	scope("VideoPlayer::readFile");

	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		error("Could not open " + filePath);
		return false;
	}

	std::streamoff length = file.tellg();
	if (length <= 0 || (size_t)length > maxVideoBytes)
	{
		error(filePath + " is empty or bigger than " + std::to_string(maxVideoBytes / (1024 * 1024)) + "MB");
		return false;
	}

	bytes.resize((size_t)length);
	file.seekg(0);
	if (!file.read((char*)bytes.data(), length))
	{
		error("Could not read all of " + filePath);
		return false;
	}

	const unsigned char* data = bytes.data();
	size_t end = bytes.size();

	//The segment holds everything but the little EBML header that comes before it
	size_t segment = 0;
	size_t segmentEnd = 0;
	for (size_t at = 0; at < end; )
	{
		uint32_t id;
		uint64_t size;
		bool unknown;
		if (!readElementId(data, end, at, id) || !readElementSize(data, end, at, size, unknown))
			break;

		if (id == idSegment)
		{
			segment = at;
			segmentEnd = unknown || size > end - at ? end : at + (size_t)size;
			break;
		}

		if (unknown || size > end - at)
			break;
		at += (size_t)size;
	}

	if (segmentEnd == 0)
	{
		error(filePath + " has no webm segment in it, is it really a .webm?");
		return false;
	}

	//First pass: how long a timecode unit is, how long the whole thing is, and which track is the video
	uint64_t timecodeScale = 1000000;
	double scaledDuration = 0;
	uint64_t videoTrack = 0;
	double frameSeconds = 0;
	std::string codec = "";

	for (size_t at = segment; at < segmentEnd; )
	{
		uint32_t id;
		uint64_t size;
		bool unknown;
		if (!readElementId(data, segmentEnd, at, id) || !readElementSize(data, segmentEnd, at, size, unknown))
			break;

		size_t body = at;
		size_t bodyEnd = unknown || size > segmentEnd - at ? segmentEnd : at + (size_t)size;

		if (id == idInfo)
		{
			for (size_t inner = body; inner < bodyEnd; )
			{
				uint32_t innerId;
				uint64_t innerSize;
				if (!readElementId(data, bodyEnd, inner, innerId) || !readElementSize(data, bodyEnd, inner, innerSize, unknown) ||
					unknown || innerSize > bodyEnd - inner)
					break;

				if (innerId == idTimecodeScale)
					timecodeScale = readUnsignedBody(data, inner, innerSize);
				else if (innerId == idDuration)
					scaledDuration = readFloatBody(data, inner, innerSize);

				inner += (size_t)innerSize;
			}
		}
		else if (id == idTracks)
		{
			for (size_t entry = body; entry < bodyEnd; )
			{
				uint32_t entryId;
				uint64_t entrySize;
				if (!readElementId(data, bodyEnd, entry, entryId) || !readElementSize(data, bodyEnd, entry, entrySize, unknown) ||
					unknown || entrySize > bodyEnd - entry)
					break;

				size_t entryBody = entry;
				size_t entryEnd = entry + (size_t)entrySize;
				entry = entryEnd;

				if (entryId != idTrackEntry)
					continue;

				//One track's settings, taken only if it turns out to be the video track and we don't have one yet
				uint64_t number = 0, type = 0, width = 0, height = 0, defaultDuration = 0;
				std::string trackCodec = "";

				for (size_t field = entryBody; field < entryEnd; )
				{
					uint32_t fieldId;
					uint64_t fieldSize;
					if (!readElementId(data, entryEnd, field, fieldId) || !readElementSize(data, entryEnd, field, fieldSize, unknown) ||
						unknown || fieldSize > entryEnd - field)
						break;

					if (fieldId == idTrackNumber)
						number = readUnsignedBody(data, field, fieldSize);
					else if (fieldId == idTrackType)
						type = readUnsignedBody(data, field, fieldSize);
					else if (fieldId == idDefaultDuration)
						defaultDuration = readUnsignedBody(data, field, fieldSize);
					else if (fieldId == idCodecID)
						trackCodec.assign((const char*)data + field, (size_t)fieldSize);
					else if (fieldId == idVideo)
					{
						size_t videoEnd = field + (size_t)fieldSize;
						for (size_t inner = field; inner < videoEnd; )
						{
							uint32_t innerId;
							uint64_t innerSize;
							if (!readElementId(data, videoEnd, inner, innerId) || !readElementSize(data, videoEnd, inner, innerSize, unknown) ||
								unknown || innerSize > videoEnd - inner)
								break;

							if (innerId == idPixelWidth)
								width = readUnsignedBody(data, inner, innerSize);
							else if (innerId == idPixelHeight)
								height = readUnsignedBody(data, inner, innerSize);

							inner += (size_t)innerSize;
						}
					}

					field += (size_t)fieldSize;
				}

				if (videoTrack == 0 && type == trackTypeVideo && number > 0)
				{
					videoTrack = number;
					videoWidth = (int)width;
					videoHeight = (int)height;
					codec = trackCodec;
					frameSeconds = defaultDuration / 1000000000.0;
				}
			}
		}

		if (unknown || size > segmentEnd - at)
			break;
		at += (size_t)size;
	}

	if (videoTrack == 0)
	{
		error(filePath + " has no video track");
		return false;
	}

	//Trailing nulls and case are not worth arguing over, the codec is written like "V_VP9"
	codec = lowercase(codec.substr(0, codec.find('\0')));
	if (codec != "v_vp8" && codec != "v_vp9")
	{
		error(filePath + " is a webm of " + codec + ", only VP8 and VP9 video can be played");
		return false;
	}

	codecIsVp9 = codec == "v_vp9";

	//Second pass: every frame of that track, in the order they're shown
	bool warnedLacing = false;
	double secondsPerTick = timecodeScale / 1000000000.0;
	uint64_t clusterTime = 0;

	//Reads one block, adding its frame if it belongs to the video track
	auto readBlock = [&](size_t block, size_t blockEnd)
	{
		uint64_t number;
		if (!readVarInt(data, blockEnd, block, number) || block + 3 > blockEnd)
			return;

		int16_t relative = (int16_t)((data[block] << 8) | data[block + 1]);
		unsigned char flags = data[block + 2];
		block += 3;

		if (number != videoTrack)
			return;

		//Several frames packed into one block, which video essentially never does
		if (flags & 0x06)
		{
			if (!warnedLacing)
				error(filePath + " has laced video blocks, which aren't supported, so some frames are missing");
			warnedLacing = true;
			return;
		}

		Frame frame;
		frame.offset = block;
		frame.size = (uint32_t)(blockEnd - block);
		frame.time = (double)((int64_t)clusterTime + relative) * secondsPerTick;
		frames.push_back(frame);
	};

	for (size_t at = segment; at < segmentEnd; )
	{
		uint32_t id;
		uint64_t size;
		bool unknown;
		if (!readElementId(data, segmentEnd, at, id) || !readElementSize(data, segmentEnd, at, size, unknown))
			break;

		if (id != idCluster)
		{
			if (unknown || size > segmentEnd - at)
				break;
			at += (size_t)size;
			continue;
		}

		//A cluster of unknown size runs to the next one, so its children are walked until one turns up
		size_t clusterEnd = unknown || size > segmentEnd - at ? segmentEnd : at + (size_t)size;
		size_t child = at;
		at = clusterEnd;

		while (child < clusterEnd)
		{
			uint32_t childId;
			uint64_t childSize;
			size_t before = child;
			if (!readElementId(data, clusterEnd, child, childId) || !readElementSize(data, clusterEnd, child, childSize, unknown) ||
				unknown || childSize > clusterEnd - child)
				break;

			if (childId == idCluster)
			{
				at = before;
				break;
			}

			if (childId == idTimecode)
				clusterTime = readUnsignedBody(data, child, childSize);
			else if (childId == idSimpleBlock)
				readBlock(child, child + (size_t)childSize);
			else if (childId == idBlockGroup)
			{
				size_t groupEnd = child + (size_t)childSize;
				for (size_t inner = child; inner < groupEnd; )
				{
					uint32_t innerId;
					uint64_t innerSize;
					if (!readElementId(data, groupEnd, inner, innerId) || !readElementSize(data, groupEnd, inner, innerSize, unknown) ||
						unknown || innerSize > groupEnd - inner)
						break;

					if (innerId == idBlock)
						readBlock(inner, inner + (size_t)innerSize);

					inner += (size_t)innerSize;
				}
			}

			child += (size_t)childSize;
		}
	}

	if (frames.empty())
	{
		error(filePath + " has a video track with no frames in it");
		return false;
	}

	//Blocks are stored in the order they're decoded, which for VP9 alt-ref frames isn't the order they're shown
	std::stable_sort(frames.begin(), frames.end(), [](const Frame& a, const Frame& b) { return a.time < b.time; });

	//How long the last frame is up for: what the file says, else the track's frame rate, else the gap before it
	double lastFrame = frames.back().time;
	if (frameSeconds <= 0)
		frameSeconds = frames.size() > 1 ? lastFrame / (double)(frames.size() - 1) : 1.0 / 30.0;

	duration = scaledDuration > 0 ? scaledDuration * secondsPerTick : lastFrame + frameSeconds;
	if (duration <= lastFrame)
		duration = lastFrame + frameSeconds;

	return true;
}

#ifdef HAVE_LIBVPX

/*
	One decoded frame, scaled into a square of RGBA and turned from the YCbCr video codecs use into
	the colors a texture wants. Each destination pixel averages the source pixels it covers, so a
	video far bigger than the square doesn't come out as a sparkling mess of every fourth pixel
*/
static void convertToRgba(const vpx_image_t* image, int size, std::vector<unsigned char>& out)
{
	int sourceWidth = (int)image->d_w;
	int sourceHeight = (int)image->d_h;
	if (sourceWidth < 1 || sourceHeight < 1)
		return;

	out.resize((size_t)size * size * 4);

	bool deep = (image->fmt & VPX_IMG_FMT_HIGHBITDEPTH) != 0;
	int shift = deep ? std::max(0, (int)image->bit_depth - 8) : 0;

	//Takes one sample from a plane as 0-255, whatever bit depth the video has
	auto sample = [&](int plane, int x, int y) -> int
	{
		const unsigned char* row = image->planes[plane] + (size_t)y * image->stride[plane];
		if (!deep)
			return row[x];

		uint16_t value;
		memcpy(&value, row + (size_t)x * 2, sizeof(uint16_t));
		return value >> shift;
	};

	//Chroma is usually stored at half width and height, and the image says by how much
	int chromaShiftX = image->x_chroma_shift;
	int chromaShiftY = image->y_chroma_shift;
	int chromaWidth = (sourceWidth + (1 << chromaShiftX) - 1) >> chromaShiftX;
	int chromaHeight = (sourceHeight + (1 << chromaShiftY) - 1) >> chromaShiftY;

	//Studio range keeps 16-235 for black to white, full range uses all of 0-255, and the two matrices differ in how blue and red mix
	bool full = image->range == VPX_CR_FULL_RANGE;
	bool bt709 = image->cs == VPX_CS_BT_709;
	float lumaScale = full ? 1.0f : 255.0f / 219.0f;
	float lumaOffset = full ? 0.0f : 16.0f;
	float toRedV = bt709 ? (full ? 1.5748f : 1.7927f) : (full ? 1.402f : 1.5960f);
	float toGreenU = bt709 ? (full ? -0.1873f : -0.2132f) : (full ? -0.344136f : -0.3918f);
	float toGreenV = bt709 ? (full ? -0.4681f : -0.5329f) : (full ? -0.714136f : -0.8130f);
	float toBlueU = bt709 ? (full ? 1.8556f : 2.1124f) : (full ? 1.772f : 2.0172f);

	for (int y = 0; y < size; y++)
	{
		int top = y * sourceHeight / size;
		int bottom = std::max(top + 1, (y + 1) * sourceHeight / size);
		int chromaTop = top >> chromaShiftY;
		int chromaBottom = std::min(chromaHeight, std::max(chromaTop + 1, ((bottom - 1) >> chromaShiftY) + 1));

		for (int x = 0; x < size; x++)
		{
			int left = x * sourceWidth / size;
			int right = std::max(left + 1, (x + 1) * sourceWidth / size);
			int chromaLeft = left >> chromaShiftX;
			int chromaRight = std::min(chromaWidth, std::max(chromaLeft + 1, ((right - 1) >> chromaShiftX) + 1));

			int luma = 0;
			for (int sourceY = top; sourceY < bottom; sourceY++)
				for (int sourceX = left; sourceX < right; sourceX++)
					luma += sample(VPX_PLANE_Y, sourceX, sourceY);
			luma /= (bottom - top) * (right - left);

			int blue = 0, red = 0;
			for (int sourceY = chromaTop; sourceY < chromaBottom; sourceY++)
			{
				for (int sourceX = chromaLeft; sourceX < chromaRight; sourceX++)
				{
					blue += sample(VPX_PLANE_U, sourceX, sourceY);
					red += sample(VPX_PLANE_V, sourceX, sourceY);
				}
			}
			int chromaCount = (chromaBottom - chromaTop) * (chromaRight - chromaLeft);
			blue /= chromaCount;
			red /= chromaCount;

			float brightness = (luma - lumaOffset) * lumaScale;
			float u = (float)(blue - 128);
			float v = (float)(red - 128);

			unsigned char* pixel = out.data() + ((size_t)y * size + x) * 4;
			pixel[0] = (unsigned char)std::clamp(brightness + toRedV * v, 0.0f, 255.0f);
			pixel[1] = (unsigned char)std::clamp(brightness + toGreenU * u + toGreenV * v, 0.0f, 255.0f);
			pixel[2] = (unsigned char)std::clamp(brightness + toBlueU * u, 0.0f, 255.0f);
			pixel[3] = 255;
		}
	}
}

#endif

bool VideoPlayer::decodeFrame(const Frame& frame, bool convert)
{
#ifndef HAVE_LIBVPX
	return false;
#else
	if (vpx_codec_decode(&decoder->codec, bytes.data() + frame.offset, frame.size, nullptr, 0) != VPX_CODEC_OK)
		return false;

	/*
		Every picture the frame produced has to be taken, whether or not it's drawn, so libvpx can have
		its buffers back. Only the last one is worth converting: while catching up the ones before it
		are never seen, and a VP9 stream can hand back a frame that is only there for later ones to refer to
	*/
	const vpx_image_t* newest = nullptr;
	vpx_codec_iter_t iterator = nullptr;
	while (const vpx_image_t* image = vpx_codec_get_frame(&decoder->codec, &iterator))
		newest = image;

	if (!newest)
		return false;

	if (convert)
		convertToRgba(newest, outputSize, pixels);

	return true;
#endif
}

bool VideoPlayer::open(const std::string& filePath, int size)
{
	scope("VideoPlayer::open");

	path = filePath;
	outputSize = std::clamp(size, 1, 4096);

#ifndef HAVE_LIBVPX
	error("This build has no libvpx, so " + filePath + " can't be played");
	return false;
#else
	if (!readFile(filePath))
		return false;

	decoder = new Decoder;

	vpx_codec_dec_cfg_t config = {};
	config.threads = 1;
	config.w = videoWidth;
	config.h = videoHeight;

	//A libvpx built against different headers than External/vpx says so here rather than misbehaving later
	const vpx_codec_iface_t* codecInterface = codecIsVp9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx();
	vpx_codec_err_t started = vpx_codec_dec_init(&decoder->codec, codecInterface, &config, 0);
	if (started != VPX_CODEC_OK)
	{
		error("libvpx wouldn't start a decoder for " + filePath + ": " + vpx_codec_err_to_string(started));
		delete decoder;
		decoder = nullptr;
		return false;
	}

	decoder->started = true;
	pixels.assign((size_t)outputSize * outputSize * 4, 0);

	if (!decodeFrame(frames[0], true))
	{
		error("The first frame of " + filePath + " wouldn't decode");
		return false;
	}

	shownFrame = 0;
	playTime = 0;
	return true;
#endif
}

bool VideoPlayer::advance(double seconds)
{
	if (!isOpen())
		return false;

	//A long hitch (loading a save, a window drag) shouldn't leave the video with a pile of frames to catch up on
	if (!(seconds > 0))
		seconds = 0;
	playTime += std::min(seconds, 0.25);

	if (duration > 0)
	{
		while (playTime >= duration)
			playTime -= duration;
	}

	//The newest frame that should be showing by now
	int target = 0;
	for (int a = (int)frames.size() - 1; a >= 0; a--)
	{
		if (frames[a].time <= playTime)
		{
			target = a;
			break;
		}
	}

	if (target == shownFrame)
		return false;

	//Each frame is built from the one before it, so catching up (or starting the loop again) means decoding them all
	int index = target > shownFrame ? shownFrame + 1 : 0;
	int decoded = 0;
	bool drew = false;

	while (index <= target && decoded < maxDecodesPerAdvance)
	{
		decoded++;
		drew |= decodeFrame(frames[index], index == target || decoded == maxDecodesPerAdvance);
		index++;
	}

	shownFrame = index - 1;

	//Too far behind to catch up in one go, so it plays slowly for a moment instead of falling further behind every frame
	if (shownFrame < target)
		playTime = frames[shownFrame].time;

	return drew;
}

VideoPlayer::~VideoPlayer()
{
#ifdef HAVE_LIBVPX
	if (decoder && decoder->started)
		vpx_codec_destroy(&decoder->codec);
#endif
	delete decoder;
}
