#include "DefaultPreferences.h"

void populateDefaults(std::shared_ptr<SettingManager> settings)
{
	/*
		Be sure to have the overwrite parameter always be set to false for these
		Don't want to overwrite the user's intended settings
  		Leave last parameter blank if you don't want it to show up in the options GUI
    	Users can still edit it manually in the text file that's exported
	*/

	//Global SDL / GL settings:
	//RenderContext falls back to 3.3 if the driver can't make this
	settings->addInt("graphics/openglmajor", 		4, false,		"OpenGL Version Major",1,4);
	settings->addInt("graphics/openglminor", 		6, false,		"OpenGL Version Minor",0,10);
	settings->addInt("graphics/multisamplebuffers", 	1, false,		"Anti-Aliasing Buffers",0,10);
	settings->addInt("graphics/multisamplesamples", 	8, false,		"Anti-Aliasing Samples",0,64);
	settings->addBool("graphics/compatibilityprofile",false, false,	"OpenGL Compatibility Profile");
	settings->addBool("graphics/debug", 		  false,false,			"OpenGL Debug Mode");
	settings->setTooltip("graphics/debug",			"Useful when attaching RenderDoc");

	//Render context specific settings:
	settings->addInt("graphics/startresolutionx", 	1024, false,	"Start Resolution X",1,4096);
	settings->addInt("graphics/startresolutiony", 	1024, false,	"Start Resolution Y",1,4096);
	settings->addBool("graphics/startfullscreen", 	false, false,	"Start in fullscreen");
	settings->addBool("graphics/usevsync", 			false, false,	"Vertical Sync");
	settings->setTooltip("graphics/usevsync",		"Limits FPS to the monitor's refresh rate");
	settings->addInt("graphics/fieldofview",		90, false,		"Field of View", 60, 120);
	settings->setTooltip("graphics/fieldofview",	"Degrees from the top to the bottom of the screen, holding zoom narrows it to 15");

	//General settings
	settings->addBool("logger/verbose",				false, false,	"Enable verbose logging");
	settings->addFloat("input/mousesensitivity",		1.0f, false, 	"Mouse Sensitivity",0,10);
	settings->addBool("input/invertmousey",			false, false,	"Invert mouse Y axis");
	
	//Network settings
	//settings->addInt("network/incomingbandwidth",		0, false, 		"ENet incoming assumption, 0 for dynamic",0,500000);
	//settings->addInt("network/outgoingbandwidth",		0, false, 		"ENet outgoing assumption, 0 for dynamic",0,500000);
	//The last server, port, and name connected with are remembered in Config/state.txt instead, see ClientProgramData::state
	settings->addInt("network/packetholdtime",		10000, false, "Packet Hold Time (ms)", 1, 65535);
	settings->setTooltip("network/packetholdtime",	"Longest a packet from the server waits to be applied, like one for an object that hasn't arrived yet, before it's thrown out");
	settings->addInt("network/snapshotbuffer",			4, true,  "Snapshot buffer size", 2, 8);
	
	//GUI Settings
	settings->addFloat("gui/opacity",				0.75f, false,		"HUD Opacity",0,1);
	settings->addEnum("gui/scaling",					1, 				"GUI Scaling Factor", 	{"Small","Normal","Large","Largest"});
	settings->addFloat("gui/rounding", 				0.0, false,		"UI element rounding",0.0,15.0);
	settings->addColor("gui/windowcolor", glm::vec4(0.06, 0.06, 0.06, 0.940), false, "Window Background Color");
	settings->addColor("gui/textcolor", 			glm::vec4(1,1,1,1), false, "Default Text Color");
	settings->addColor("gui/framecolor", 			glm::vec4(0.16,0.29,0.48,0.54), false, "Frame Color (normal)");
	settings->addColor("gui/framehovercolor", 		glm::vec4(0.26,0.59,0.98,0.4), false, "Frame Color (hover)");
	settings->addColor("gui/frameclickcolor", 		glm::vec4(0.26,0.59,0.98,0.67), false, "Frame Color (clicked)");
	settings->addColor("gui/titlecolor", 			glm::vec4(0.16,0.29,0.48,0.54), false, "Titel Bar Color");
	settings->addColor("gui/highlight", 			glm::vec4(0.24,0.52,0.88,1.0), false, "Titel Bar Color");

	//Audio settings
	settings->addFloat("audio/mastervolume",			0.5f, false,		"Master Volume",0,1);
	settings->addFloat("audio/musicvolume",			0.5f, false,		"Music Volume",0,1);
	settings->addEnum("audio/reverbquality",			2,					"Environmental Reverb",	{"Off","Low","Medium","High"});
	settings->setTooltip("audio/reverbquality",		"Echo from nearby walls and water, measured with raycasts");
	settings->addEnum("audio/occlusionquality",		2,					"Muffle Behind Walls",	{"Off","Low","Medium","High"});
	settings->setTooltip("audio/occlusionquality",	"Muffles sounds with bricks or objects in the way, measured with raycasts");
	settings->addFloat("audio/voicevolume",			1.0f, false,		"Voice Chat Volume",0,1);
	settings->addFloat("audio/microphonevolume",		1.0f, false,		"Microphone Volume",0,4);
	settings->addString("audio/microphone",			"Default", false,	"Microphone");
	settings->setTooltip("audio/microphone",			"What voice chat records from, Default follows the system's default microphone");

	//Graphics settings
	settings->addEnum("graphics/waterquality",		1, 	 			"Water Quality",	{"No Reflection/Refraction", "Half Res Reflection/Refraction", "Full Res Reflection/Refraction"});
	settings->addEnum("graphics/shadowresolution",	1, 	 			"Shadow Resolution", 	{"2k Shadows","4k Shadows","8k Shadows"});
	settings->addEnum("graphics/shadowsoftness",		1,  			"Shadow Softness" , 	{"Hard","Soft (3x3 texels)","Softer (5x5 texels)","Softest (7x7 texels)"});
	settings->addBool("graphics/shadowcolor",		true, false, 	"Colored Shadows");
	settings->setTooltip("graphics/shadowcolor",		"Light through transparent bricks takes on their color");
	settings->addBool("graphics/depthprepass",		true, false,	"Depth Pre-Pass");
	settings->setTooltip("graphics/depthprepass",	"Draws bricks into the depth buffer first so hidden ones are never shaded. Much faster inside a big build, slightly slower looking at almost nothing");
	settings->addBool("graphics/imagebasedlighting",	true, false,	"Image Based Lighting");
	settings->setTooltip("graphics/imagebasedlighting", "Servers with a .hdr sky light everything with it and show it in reflections, otherwise it's only drawn as the sky");
	settings->addInt("graphics/pointshadows",		4, false,		"Point Light Shadows", 0, 8);
	settings->setTooltip("graphics/pointshadows",	"How many of the lights nearest the camera cast shadows, each costs up to six extra shadow passes a frame");
	settings->addEnum("graphics/godrayquality",		1, 	 			"God ray samples", 	{"None","32 samples","64 samples","96 samples","128 samples"});
	settings->addEnum("graphics/spritedensity",		1, 				"Sprite density",	{"Low","Medium","High","Very High"});
	settings->addFloat("graphics/brickdebrisseconds",	3.6f, false,	"Brick Debris Seconds",0,15);
	settings->setTooltip("graphics/brickdebrisseconds", "How long removed bricks stay as debris, 0 for none");
	settings->addInt("graphics/maxparticles",		20000, false,	"Max Particles", 0, 100000);
	settings->setTooltip("graphics/maxparticles",	"Emitters stop making particles while this many are alive, 0 for no particles");
	settings->addInt("graphics/maxvideoprints",		4, false,		"Video Prints Playing", 0, 32);
	settings->setTooltip("graphics/maxvideoprints",	"How many .webm prints play at once, the rest hold a frame. Each one decodes video every frame it's in the world");
	settings->addEnum("graphics/rainquality",		2,				"Rain Quality",		{"Sound Only","Low","Medium","High"});
	settings->setTooltip("graphics/rainquality",		"Falling drops, splashes, and wet surfaces when a server makes it rain. Costs nothing while it isn't raining");

	//Hosting settings
	settings->addBool("hosting/useevalpassword",	false, false,	"Enable Lua Password");
	settings->setTooltip("hosting/useevalpassword",	"Enables the password for remote Lua execution");
	settings->addString("hosting/evalpassword",	"changeme", false, "Lua console password");	
}
