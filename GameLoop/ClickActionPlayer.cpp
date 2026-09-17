#include "ClickActionPlayer.h"

#include "../Audio/AudioSystem.h"
#include "../Graphics/ParticleSystem.h"
#include "../SimObjects/Item.h"

void ClickActionPlayer::setAction(const ClickAction& newAction)
{
	const bool sameItem = action.itemID == newAction.itemID;
	action = newAction;

	/*
		The track is good for one play plus however many more the server says are left, less whatever
		we've already predicted that it hasn't answered for. This action stands for one shot it has
		now processed, so one of ours stops being unconfirmed
	*/
	if (sameItem)
		unconfirmedPlays = std::max(0, unconfirmedPlays - 1);
	else
		unconfirmedPlays = 0;

	playsLeft = std::max(0, 1 + (int)newAction.repeatLimit - unconfirmedPlays);

	/*
		Tracks already running are left to finish, so the last shot of a burst still plays out its
		sounds, but they can't start another go: their generation no longer matches
	*/
}

void ClickActionPlayer::clear()
{
	action = ClickAction();
}

void ClickActionPlayer::reset()
{
	clear();
	playing.clear();
	effects.clear();
	held = false;
	lastStartMS = -1000000;
	playsLeft = 0;
	unconfirmedPlays = 0;
	haveItemTransform = false;
}

void ClickActionPlayer::onClick(netIDType heldItemID, double nowMS, AudioSystem* audio, const std::shared_ptr<Item>& item)
{
	held = true;

	//Holding something the server hasn't told us about, so there's nothing to predict
	if (!hasAction() || heldItemID != action.itemID)
		return;

	//Too soon after the last one: the server won't take this click either, so nothing is shown for it
	if (action.repeatMS > 0 && nowMS - lastStartMS < action.repeatMS)
		return;

	//More shots than the server last said were left, so the rest are ones that won't happen
	if (playsLeft <= 0)
		return;

	playsLeft--;
	unconfirmedPlays++;
	lastStartMS = nowMS;

	Playback playback;
	playback.generation = action.generation;
	playback.startMS = nowMS;
	playback.nextStep = 0;
	playback.repeatsLeft = action.repeatMS > 0 ? action.repeatLimit : 0;

	playing.push_back(playback);

	//Anything the track does at the moment of the click happens now rather than a frame later
	update(nowMS, audio, nullptr, item, lastItemPosition, lastItemRotation, haveItemTransform);
}

void ClickActionPlayer::onRelease()
{
	held = false;

	//Whatever is partway through finishes its own steps, it just doesn't go round again
	for (Playback& playback : playing)
		playback.repeatsLeft = 0;
}

void ClickActionPlayer::runStep(const ClickActionStep& step, double nowMS, AudioSystem* audio, const std::shared_ptr<Item>& item)
{
	switch (step.kind)
	{
		case ClickStep_Sound:
		{
			if (!audio)
				break;

			//Follows the item if we know where it is, so a shot moves with whoever fired it
			if (item)
				audio->playSound(step.soundID, SoundLocation::on(item), step.pitch, step.volume);
			else if (haveItemTransform)
				audio->playSound(step.soundID, SoundLocation::at(lastItemPosition), step.pitch, step.volume);
			else
				audio->playSound(step.soundID, SoundLocation::flat(), step.pitch, step.volume);

			break;
		}

		case ClickStep_Animation:
		{
			if (item)
				item->startPredictedAnimation(step.animationID);
			break;
		}

		case ClickStep_Emitter:
		case ClickStep_Light:
		{
			RunningEffect effect;
			effect.isLight = step.kind == ClickStep_Light;
			effect.endMS = nowMS + step.forMS;
			effect.offset = step.offset;
			effect.emitterTypeID = step.emitterTypeID;
			effect.color = step.color;
			effect.brightness = step.brightness;
			effect.coronaWidth = step.coronaWidth;

			if (effect.isLight)
			{
				effect.lightID = nextLightID;

				//Wraps back to the top well before it could ever reach a real light's ID
				nextLightID = nextLightID > 1 ? nextLightID - 1 : NO_ID - 1;
			}

			effects.push_back(effect);
			break;
		}
	}
}

void ClickActionPlayer::update(double nowMS, AudioSystem* audio, ParticleSystem* particles, const std::shared_ptr<Item>& item,
	const glm::vec3& itemPosition, const glm::quat& itemRotation, bool itemVisible)
{
	if (itemVisible)
	{
		lastItemPosition = itemPosition;
		lastItemRotation = itemRotation;
		haveItemTransform = true;
	}

	//Run each track forward to now, firing everything it owes
	for (unsigned int a = 0; a < playing.size(); a++)
	{
		Playback& playback = playing[a];

		//The action was replaced while this was running, so it finishes what it started and stops
		const bool current = playback.generation == action.generation;

		while (current && playback.nextStep < action.steps.size())
		{
			const ClickActionStep& step = action.steps[playback.nextStep];
			if (nowMS < playback.startMS + step.atMS)
				break;

			runStep(step, playback.startMS + step.atMS, audio, item);
			playback.nextStep++;
		}

		if (!current || playback.nextStep < action.steps.size())
			continue;

		/*
			An automatic weapon can't wait for the server between shots, so the track goes round
			again on its own while the button is down, as many times as the server allowed
		*/
		if (held && action.repeatMS > 0 && playback.repeatsLeft > 0 && playsLeft > 0
			&& nowMS >= playback.startMS + action.repeatMS)
		{
			playback.startMS += action.repeatMS;
			playback.nextStep = 0;
			playback.repeatsLeft--;
			playsLeft--;
			unconfirmedPlays++;
			lastStartMS = playback.startMS;
			continue;
		}

		//Nothing left to do
		playing.erase(playing.begin() + a);
		a--;
	}

	//Keep the emitters going and drop anything that's run out
	for (unsigned int a = 0; a < effects.size(); a++)
	{
		RunningEffect& effect = effects[a];

		if (nowMS >= effect.endMS)
		{
			effects.erase(effects.begin() + a);
			a--;
			continue;
		}

		if (!effect.isLight && particles && haveItemTransform)
		{
			glm::vec3 where = lastItemPosition + lastItemRotation * effect.offset;
			particles->emit(effect.clock, effect.emitterTypeID, where, lastItemRotation, glm::vec3(0), nowMS);
		}
	}
}

void ClickActionPlayer::addLights(std::vector<PointLightSource>& lights) const
{
	if (!haveItemTransform)
		return;

	for (const RunningEffect& effect : effects)
	{
		if (!effect.isLight)
			continue;

		PointLightSource light;
		//Ours alone, not an object the server knows about, but still numbered so its shadows keep their slot
		light.id = effect.lightID;
		light.position = lastItemPosition + lastItemRotation * effect.offset;
		light.color = effect.color;
		light.brightness = effect.brightness;
		light.coronaWidth = effect.coronaWidth;
		light.range = sqrtf(std::max(0.0f, effect.brightness) * 50.0f);

		lights.push_back(light);
	}
}
