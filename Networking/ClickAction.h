#pragma once

#include "../LandOfDran.h"
#include "PacketEnums.h"

/*
	A click action is what a client is told to play the moment they click, without waiting to hear
	back from the server about it.

	The server sends one of these ahead of time saying what the next click with a given item looks
	like: its sounds, its animation, its muzzle flash, its light, and how long after the click each
	one happens. When the click comes the client plays it straight away, so a shot is heard and seen
	at once rather than a round trip later.

	Only the look of it is predicted. The shot itself, what it hits, and how much ammo is left are
	all still worked out by the server, so the worst a wrong guess can do is show a flash that
	shouldn't have happened. Nothing here decides anything about the game.

	Because the server says what the *next* click does, the client never needs to know any of the
	rules: an empty gun is simply sent the dry click track instead of the firing one.
*/

//What one step of a click action does
enum ClickActionStepKind : unsigned char
{
	ClickStep_Sound = 0,
	ClickStep_Animation = 1,
	ClickStep_Emitter = 2,
	ClickStep_Light = 3
};

//One thing that happens a fixed time after the click
struct ClickActionStep
{
	//How long after the click this happens
	uint16_t atMS = 0;
	ClickActionStepKind kind = ClickStep_Sound;

	//Sound: which one, and how it's played
	uint16_t soundID = 0;
	float pitch = 1.0f;
	float volume = 1.0f;

	//Animation: which of the item model's animations to restart
	uint8_t animationID = 0;

	//Emitter: which type ejects
	uint16_t emitterTypeID = 0;

	//Emitter and light: how long it lasts, and where it sits in the item's own space, so a muzzle
	//flash stays on the end of the barrel wherever the item is being drawn
	uint16_t forMS = 0;
	glm::vec3 offset = glm::vec3(0);

	//Light: the colour it shines, how bright, and how big its corona is
	glm::vec3 color = glm::vec3(1);
	float brightness = 0;
	float coronaWidth = 0;
};

struct ClickAction
{
	//The item this belongs to. A click with anything else plays nothing
	netIDType itemID = NO_ID;

	/*
		Goes up every time the server replaces the action. A playback that started under an older one
		is left to finish, but no new one starts from it, so switching weapons or dying can't leave a
		stale track waiting to fire.
	*/
	uint32_t generation = 0;

	/*
		repeatMS is how often the track may play at all, which is the weapon's rate of fire. A click
		that comes sooner is one the server would throw away, so nothing is shown for it.

		repeatLimit is how many more times it may play beyond the first before the server has to say
		otherwise, which the server sets from what's left in the magazine. It covers both an automatic
		weapon carrying on while the button is held and someone clicking faster than the round trip:
		without it the client runs ahead and shows shots the magazine didn't have. The server sends a
		fresh action after every shot, so the count resyncs constantly.
	*/
	uint16_t repeatMS = 0;
	uint8_t repeatLimit = 0;

	std::vector<ClickActionStep> steps;

	//How many bytes writeTo needs
	unsigned int packedSize() const;

	//Writes everything but the leading packet type byte, returns how many bytes it wrote
	unsigned int writeTo(enet_uint8* dest) const;

	//Reads one back, false if the data ran out or is nonsense
	bool readFrom(const enet_uint8* source, unsigned int length);
};

//Most of a track is a handful of steps, and this keeps a bad packet from making us allocate wildly
constexpr unsigned int maxClickActionSteps = 32;
