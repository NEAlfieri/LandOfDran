#pragma once

#include "../LandOfDran.h"

#include "../Graphics/ShaderSpecification.h"
#include "../Graphics/Material.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/RenderContext.h"
#include "../Interface/InputMap.h"
#include "../Interface/SettingsMenu.h"
#include "../Interface/DebugMenu.h"
#include "../Interface/ServerBrowser.h"
#include "../Interface/EscapeMenu.h"
#include "../Physics/PhysicsWorld.h"
#include "../Interface/ChatWindow.h"
#include "../Graphics/RenderTarget.h"
#include "../Graphics/Environment.h"
#include "../Graphics/Skybox.h"
#include "../Graphics/InstancedBrickRenderer.h"
#include "../Graphics/PointLights.h"
#include "ClickActionPlayer.h"
#include "../Graphics/WaterRipples.h"
#include "../Graphics/ParticleSystem.h"
#include "../Graphics/Rain.h"
#include "../Graphics/GpuProfiler.h"
#include "../Graphics/ItemIconRenderer.h"
#include "../Bricks/GhostBrick.h"
#include "../Bricks/SelectionBox.h"
#include "../Bricks/VehicleGhost.h"
#include "../Bricks/BrickTypes.h"
#include "../Bricks/PrintTypes.h"
#include "../Graphics/PrintVideos.h"
#include "../Interface/BrickSelector.h"
#include "../Interface/BrickHotbar.h"
#include "../Interface/PaintMenu.h"
#include "../Interface/ItemHotbar.h"
#include "../Interface/AppearanceEditor.h"
#include "../Interface/WrenchDialog.h"
#include "../Interface/PrintMenu.h"
#include "../Interface/VehicleLoader.h"
#include "../Interface/BrickSaveMenu.h"
#include "../Interface/PlayerListWindow.h"
#include "../Audio/AudioSystem.h"
#include "../Audio/AcousticProbe.h"
#include "../Audio/VoiceChat.h"

/*
	This exists so we can make all of this available to the various PacketsFromServer files since packets can do a wide range of activities
	and may need access to most of the game state 
	It's owned by LoopClient and passed through Client to various packet processing methods
*/
struct ClientProgramData
{
	Material* grassMaterial = nullptr;
	GLuint grassVao = 0;

	/*
		TODO: Move this to environment class and don't hardcode it to 3
		These are the matrices the shadow maps were actually drawn with, which is not always this frame's:
		the farther cascades are only redrawn every so often, see the shadow pass in LoopClient::renderEverything.
		Everything that samples the maps has to use these, or it would look them up in the wrong place
	*/
	glm::mat4 lightSpaceMatricies[3];

	//Where the camera was, and how wide the cascade was, when each one was last drawn
	glm::vec3 cascadeDrawnFrom[3] = { glm::vec3(0), glm::vec3(0), glm::vec3(0) };
	float cascadeRadius[3] = { 0, 0, 0 };
	//Cleared when the shadow maps are (re)created, so the first frame after that draws all three
	bool cascadeDrawn[3] = { false, false, false };
	//Counts frames, so the cascades that don't redraw every frame can take it in turns
	unsigned int cascadeFrame = 0;
	GLuint lightSpaceMatriciesUniformModel = 0;
	GLuint lightSpaceMatriciesUniformBrick = 0;
	//The one cascade being drawn into, see the shadow pass in LoopClient::renderEverything
	GLint shadowCascadeMatrixUniformModel = -1;
	GLint shadowCascadeMatrixUniformBrick = -1;
	GLint shadowTintMatrixUniform = -1;
	GLint shadowCascadeMinOpacityUniform = -1;
	GLint shadowTintMinOpacityUniform = -1;
	//skipContaining and skipPoint in brickShadowCascade.vert, for each program that uses it
	GLint shadowCascadeSkipContainingUniform = -1;
	GLint shadowCascadeSkipPointUniform = -1;
	GLint shadowTintSkipContainingUniform = -1;
	GLint shadowTintSkipPointUniform = -1;

	//Lives for the whole program, bricks of whichever server we're on are passed to it by Simulation's BrickHolder
	InstancedBrickRenderer* brickRenderer = nullptr;

	//Lives for the whole program, lights of whichever server we're on are passed to it each frame, see LoopClient::renderEverything
	PointLights* pointLights = nullptr;

	//Lives for the whole program, particle and emitter types come from whichever server we're on, see LoopClient::updateParticles
	ParticleSystem* particles = nullptr;

	//Named brick sizes with icons, for the brick selector
	BrickTypes brickTypes;

	//Prints from Assets/brick/prints, each loaded into a layer of the decal array, see PrintType::decalLayer
	PrintTypes prints;

	//The .webm ones among them, playing into their layers, see Graphics/PrintVideos.h
	PrintVideos printVideos;

	//Lives for the whole program, every vehicle's wheels are instances of it, nullptr if it couldn't be loaded, see LoopClient::placeVehicleWheels
	Model* tireModel = nullptr;

	GhostBrick ghostBrick;

	//The box drawn around bricks to slice them into a vehicle
	SelectionBox selectionBox;

	//A saved vehicle following the crosshair until a left click places it
	VehicleGhost vehicleGhost;
	std::shared_ptr<BrickSelector> brickSelector = nullptr;
	std::shared_ptr<BrickHotbar> brickHotbar = nullptr;
	std::shared_ptr<PaintMenu> paintMenu = nullptr;
	std::shared_ptr<ItemHotbar> itemHotbar = nullptr;

	//Draws the model of each item you're carrying for the item bar to show, nullptr while graphics/itemicons3d is off
	std::shared_ptr<ItemIconRenderer> itemIcons = nullptr;

	//graphics/itemicons3d as of launch or the last settings save
	bool itemIcons3d = true;

	//Plays what the server said the next click with the held item does, without waiting for it, see Networking/ClickAction.h
	std::shared_ptr<ClickActionPlayer> clickActions = std::make_shared<ClickActionPlayer>();

	/*
		Things the game remembers between launches rather than settings the player picks: last server and name,
		window size, hot bar. Kept out of Config/settings.txt so they can be written often without rewriting that
	*/
	std::shared_ptr<SettingManager> state = nullptr;
	static constexpr const char* stateFilePath = "Config/state.txt";

	//graphics/startresolutionx and y as of launch or the last settings save, so a newly picked one can be applied
	glm::ivec2 appliedStartResolution = glm::ivec2(0);
	std::shared_ptr<RenderTarget> shadows = nullptr;
	//graphics/shadowresolution is off, so the sun and moon cast nothing and shadows is a single texel
	bool sunShadows = true;

	//graphics/shadowresolution, graphics/shadowsoftness, and graphics/shadowcolor as of launch or the last settings save
	int shadowResolution = 0;
	int shadowSoftness = 1;
	bool coloredShadows = true;

	//Depth of the nearest transparent brick and the color light picks up through transparent bricks, per cascade
	//A single texel while colored shadows are off, so the samplers in model.frag always have something bound
	std::shared_ptr<RenderTarget> shadowTint = nullptr;
	int shadowTintResolution = 0;

	//Colored shadows are on and there's at least one transparent brick to tint with, as of this frame's shadow pass
	bool tintShadowsActive = false;

	Environment environment;

	//Lives for the whole program, skyboxes of whichever server we're on are loaded into it, see LoopClient::renderEverything
	Skybox* skybox = nullptr;
	//graphics/imagebasedlighting as of launch or the last settings save
	bool imageBasedLighting = true;

	//graphics/depthprepass as of launch or the last settings save, see the pre-pass in LoopClient::renderScene
	bool depthPrePass = true;

	/*
		graphics/drawdistance as of launch or the last settings save: nothing further than this from the camera is
		drawn at all. Kept in step with the camera's far plane, which clips whatever the culls below still submit,
		see LoopClient::applyDrawDistance. 1000 is as far as anything was ever drawn, so it changes nothing there
	*/
	float drawDistance = 1000.0f;

	//Empty, sky.vert builds a fullscreen triangle from gl_VertexID but core profile still needs a VAO bound
	GLuint skyVao = 0;

	GLuint waterVao = 0;
	GLuint waterVbo = 0;
	GLsizei waterVertexCount = 0;

	//The scene above and below the water surface, both nullptr when graphics/waterquality is off
	std::shared_ptr<RenderTarget> waterReflection = nullptr;
	std::shared_ptr<RenderTarget> waterRefraction = nullptr;

	//Rings on the water from things moving through its surface, of whichever server we're on
	WaterRipples waterRipples;

	//Rain of whichever server we're on, and the map of what's overhead that keeps it outside
	Rain rain;

	//Times each render pass while the debug menu asks for it, otherwise costs nothing, see LoopClient::renderEverything
	GpuProfiler profiler;

	/*
		God rays: where the sky still shows around the sun, at half the width and height of the screen,
		nullptr while graphics/godrayquality is None. See LoopClient::renderGodRays
	*/
	std::shared_ptr<RenderTarget> godRayMask = nullptr;

	//graphics/godrayquality as of launch or the last settings save, as places looked at along each ray, 0 for off
	int godRaySamples = 0;

	GLint godRaySunScreenUniform = -1;
	GLint godRaySampleCountUniform = -1;
	GLint godRayStrengthUniform = -1;

	//Copy of the finished scene that underwater.frag draws back warped, made the first time the camera goes under the water or a vignette shows
	std::shared_ptr<RenderTarget> underwaterScene = nullptr;
	//False if the screen couldn't be copied into underwaterScene, then the tints are just blended over the scene
	bool underwaterSceneCopies = false;

	std::shared_ptr<RenderContext>	context = nullptr;
	std::shared_ptr<UserInterface>	gui = nullptr;
	std::shared_ptr<SettingsMenu>	settingsMenu = nullptr;
	std::shared_ptr<DebugMenu>		debugMenu = nullptr;
	std::shared_ptr<ServerBrowser>	serverBrowser = nullptr;
	std::shared_ptr<ShaderManager>	shaders = nullptr;
	std::shared_ptr<TextureManager> textures = nullptr;
	std::shared_ptr<InputMap>		input = nullptr;
	std::shared_ptr<EscapeMenu>		escapeMenu = nullptr;
	std::shared_ptr<ChatWindow>		chatWindow = nullptr;
	std::shared_ptr<AppearanceEditor> appearanceEditor = nullptr;
	std::shared_ptr<WrenchDialog>	wrenchDialog = nullptr;
	std::shared_ptr<PrintMenu>		printMenu = nullptr;
	std::shared_ptr<VehicleLoader>	vehicleLoader = nullptr;
	std::shared_ptr<BrickSaveMenu>	brickSaveMenu = nullptr;
	std::shared_ptr<PlayerListWindow>	playerList = nullptr;

	//File names of the images in Assets/faces, each one's index is its layer in the decal array, see LoopClient's constructor
	std::vector<std::string> faceNames;

	//File names of the images in Assets/shirts, their layers in the decal array come after every face
	std::vector<std::string> shirtNames;

	/*
		File names of the hat descriptors in PlayerAppearance::partsFolder that loaded, and their models in the same order,
		which live for the whole program with every dynamic's hat an instance of one, see LoopClient's constructor
	*/
	std::vector<std::string> hatNames;
	std::vector<Model*> hatModels;

	//The model of a part by its file name, nullptr for none or one this game doesn't have
	Model* getPartModel(const std::string& partName) const
	{
		for (size_t a = 0; a < hatNames.size() && a < hatModels.size(); a++)
		{
			if (hatNames[a] == partName)
				return hatModels[a];
		}
		return nullptr;
	}

	//The decal array layer of a face from Assets/faces or a shirt from Assets/shirts, the face if both folders have that name
	//-1 for none or one this game doesn't have
	int getDecal(const std::string& decalName) const
	{
		if (decalName.empty())
			return -1;

		for (size_t a = 0; a < faceNames.size(); a++)
		{
			if (faceNames[a] == decalName)
				return (int)a;
		}
		for (size_t a = 0; a < shirtNames.size(); a++)
		{
			if (shirtNames[a] == decalName)
				return (int)(faceNames.size() + a);
		}
		return -1;
	}

	//Lives for the whole program, sound types come from whichever server we're on
	std::shared_ptr<AudioSystem>	audio = nullptr;

	//Lives for the whole program, recording is only done in a server and talkers are forgotten on leaving it
	std::shared_ptr<VoiceChat>		voice = nullptr;

	//Raycasts for reverb and muffling, see LoopClient::run and the occlusion test set up in LoopClient's constructor
	AcousticProbe acousticProbe;

	//TODO: Move to simulation
	std::shared_ptr<PhysicsWorld>	physicsWorld = nullptr;

	//The rest of this struct is passed as const to various PacketsFromServer functions
	//If a packet wants to communicate to the main loop it should do so here
	struct PacketSignals
	{
		//These two are set by AcceptConnection.cpp
		bool startPhaseOneLoading = false;
		unsigned int typesToLoad = 0;
		//Set by AddSimObjectType
		bool finishedPhaseOneLoading = false;

		//Call this after each frame
		void reset()
		{
			finishedPhaseOneLoading = false;
			startPhaseOneLoading = false;
		}
	} signals;

	//You should be able to edit the signals from received packets function bodies, just not the pointers to essential software systems
	PacketSignals * getSignals() const { return const_cast<PacketSignals*>(&signals); }
};
