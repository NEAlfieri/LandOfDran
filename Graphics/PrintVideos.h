#pragma once

#include "../LandOfDran.h"
#include "../Bricks/PrintTypes.h"
#include "VideoPlayer.h"

class TextureManager;

/*
	The .webm prints a client has loaded, each playing into the decal layer its print was given

	A video print is an ordinary print everywhere else: the server only knows its name, bricks wear it
	by ID, and the renderer draws the decal layer it was loaded into. The only difference is that these
	layers have new pixels put in them as the video plays, see TextureManager::setDecalPixels

	Only videos a brick in the world is actually wearing are decoded, and only so many at once, so a
	folder full of them costs nothing until they're used
*/
class PrintVideos
{
	struct Video
	{
		//Index into PrintTypes, and the decal layer that print was loaded into
		int print = -1;
		int decalLayer = -1;

		std::unique_ptr<VideoPlayer> player;
	};

	std::vector<Video> videos;

	//How much video is being held in memory, which is capped so a folder of films can't eat it all
	size_t loadedBytes = 0;

	//How many are allowed to play at once, from graphics/maxvideoprints
	int maxPlaying = 4;

	//How many played and how many wanted to on the last update, for the debug menu
	int playingCount = 0;
	int wantedCount = 0;

	//Puts a player's current frame in its decal layer, with the smaller mip levels it needs
	void upload(const Video& video, std::shared_ptr<TextureManager> textures) const;

	public:

	/*
		Opens one print's .webm and puts its first frame in the decal layer it was given
		Call while the other decals are being loaded and before TextureManager::finalizeDecals, so that
		first frame gets its mip levels made along with everything else
		False, with an error logged, if the video wouldn't open, and then the print has no decal at all
	*/
	bool add(int print, const PrintType& type, int decalLayer, std::shared_ptr<TextureManager> textures);

	/*
		Plays the videos bricks are wearing, and puts each new frame in its decal layer
		isUsed says whether any brick in the world is drawn wearing a print, see InstancedBrickRenderer::isPrintUsed
	*/
	void update(float deltaSeconds, std::shared_ptr<TextureManager> textures, const std::function<bool(uint16_t)>& isUsed);

	void setMaxPlaying(int count) { maxPlaying = std::max(0, count); }

	size_t size() const { return videos.size(); }

	//"2/3 video prints playing" for the debug menu, "" if there are no video prints
	std::string getStatus() const;
};
