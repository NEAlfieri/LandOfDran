#pragma once

#include "../LandOfDran.h"
#include "../Networking/ClickAction.h"
#include "../SimObjects/ParticleTypes.h"
#include "../Graphics/PointLights.h"

class AudioSystem;
class ParticleSystem;
class Item;

/*
	Client only: plays the click action the server sent ahead of time, see Networking/ClickAction.h

	The point of it is that a shot is heard and seen the moment the button goes down, instead of a
	round trip later. Nothing it does decides anything: the server still works out the shot, what it
	hit, and the ammo, and its own packets will say so a moment later.

	Steps that make something lasting, a muzzle flash or the light off it, keep going here until
	their time is up. They're placed in the item's own space, so they stay on the end of the barrel
	while the item is drawn moving around in its owner's hand.
*/
class ClickActionPlayer
{
	//What the server says the next click does
	ClickAction action;

	//One click that's partway through its track
	struct Playback
	{
		//Which action this came from, so a replaced one doesn't keep firing new steps
		uint32_t generation = 0;
		double startMS = 0;
		//How far down the step list we've got
		unsigned int nextStep = 0;
		//How many more times the track may play while the button is held
		int repeatsLeft = 0;
	};

	std::vector<Playback> playing;

	//An emitter or a light one of the steps started, which lasts until its time runs out
	struct RunningEffect
	{
		bool isLight = false;
		double endMS = 0;
		//Where it sits in the item's own space
		glm::vec3 offset = glm::vec3(0);

		//Emitters keep their own clock so they eject at a steady rate however the frame rate moves
		uint16_t emitterTypeID = 0;
		EmitterClock clock;

		glm::vec3 color = glm::vec3(1);
		float brightness = 0;
		float coronaWidth = 0;

		/*
			Its own ID among the lights drawn each frame. PointLights keeps a light's shadow maps in a
			slot it matches by ID, so two flashes alive at once (an automatic weapon, or two guns) have
			to be told apart or they fight over one slot and flicker. Real lights are objects the server
			numbered upward from 0, so these count down from the top instead and can't collide
		*/
		netIDType lightID = NO_ID;
	};

	//What the next predicted light is numbered, counting down away from the server's own IDs
	netIDType nextLightID = NO_ID - 1;

	std::vector<RunningEffect> effects;

	//Whether the button is still down, which is what lets a track repeat
	bool held = false;

	/*
		When a track last started. A weapon can only fire so fast, and the server throws away a click
		that comes sooner, so predicting one would show and play a shot that never happened. Spamming
		the button would otherwise look like firing straight through an empty magazine
	*/
	double lastStartMS = -1000000;

	/*
		How many more times the track may play before the server has to send another. Set from the
		action's repeatLimit, and spent by a click or by an automatic weapon carrying on. It's what
		stops a fast clicker predicting more shots than the magazine holds
	*/
	int playsLeft = 0;

	/*
		Plays we've made that the server hasn't answered for yet. Each action it sends reflects one
		shot it has processed, and says what's left as of then, which is already out of date if we've
		predicted more since. Subtracting these stops a refresh handing back rounds we've spent, which
		would otherwise let someone clicking faster than the round trip fire past an empty magazine
	*/
	int unconfirmedPlays = 0;

	//Where the item was drawn last frame, for placing effects while it isn't visible this one
	glm::vec3 lastItemPosition = glm::vec3(0);
	glm::quat lastItemRotation = glm::quat(1, 0, 0, 0);
	bool haveItemTransform = false;

	void runStep(const ClickActionStep& step, double nowMS, AudioSystem* audio, const std::shared_ptr<Item>& item);

	public:

	//Replaces whatever the next click was going to do
	void setAction(const ClickAction& newAction);

	//Nothing is predicted until the server says otherwise, i.e. after switching to something that isn't a weapon
	void clear();

	netIDType getItemID() const { return action.itemID; }
	bool hasAction() const { return action.itemID != NO_ID && !action.steps.empty(); }

	/*
		A click happened while holding heldItemID. Starts the track if it's the item the action is
		for, otherwise does nothing at all
	*/
	void onClick(netIDType heldItemID, double nowMS, AudioSystem* audio, const std::shared_ptr<Item>& item);

	//The button came back up, so nothing repeats any more
	void onRelease();

	/*
		Runs the tracks forward and keeps any emitters and lights going. item is whatever the client
		is holding as it's drawn, or null while there's nothing to hang effects on
	*/
	void update(double nowMS, AudioSystem* audio, ParticleSystem* particles, const std::shared_ptr<Item>& item,
		const glm::vec3& itemPosition, const glm::quat& itemRotation, bool itemVisible);

	//Adds the lights any running step is shining to the ones drawn this frame
	void addLights(std::vector<PointLightSource>& lights) const;

	//Forgets everything, for leaving a server
	void reset();
};
