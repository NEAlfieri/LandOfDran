#include "Interpolator.h"

void Interpolator::reset()
{
	snapshots.clear();
	snapshotsEverAdded = 0;
}

void Interpolator::addSnapshot(const glm::vec3& pos, const glm::quat& rot,float idealBufferSize,unsigned int msSinceLastSend)
{
	//Discard all obsolete snapshots before the current interpolation start point
	for (int i = snapshots.size() - 1; i >= 0; i--)
	{
		if (snapshots[i].time < getTicksMS())
		{
			//Keep only one snapshot before the current time
			if (i != 0)
			{
				snapshots.erase(snapshots.begin(), snapshots.begin() + (i - 1));
			}
			break;
		}
	}

	//lastSentTime on the server only tracks its own send cadence, not what the client actually received -
	//under packet loss the real gap since our last received update can be much larger than msSinceLastSend
	//implies, and 255 here means "clamped, was at least this long" rather than "unknown"/zero
	float lastFrameTime = getTicksMS();
	if (snapshots.size() > 0)
		//Anchoring to at least getTicksMS() (instead of a possibly-stale buffered snapshot time) guarantees
		//this new snapshot lands in the future, so a catch-up after a starved buffer interpolates smoothly
		//instead of teleporting to a target time that's already in the past
		lastFrameTime = std::max(lastFrameTime, snapshots.back().time);

	//std::cout << "Current time: " << getTicksMS() << " Last frame time : " << lastFrameTime << " snapshots: " << snapshots.size() << "\n";
	int callTimeDiff = getTicksMS() - testLastRemoveMe;
	testLastRemoveMe = getTicksMS();

	snapshotsEverAdded++;

	//If we're running low on snapshots (server lag?) then interpolate slower, and vice-versa
	float timeBetween = msSinceLastSend;
	float diff = snapshots.size() - idealBufferSize;
	diff = 1.0 - (diff * 0.1);
	//More than 10 over (a burst of updates handled in one frame after a hitch) would take the root of a negative number,
	//the NaN time that gives makes getPosition return NaN, and the drawn player vanishes for good
	if (diff < 1.0)
		diff = sqrt(std::max(diff, 0.1f));

	//Don't apply the slowdown side of that while still ramping up right after creation - a small buffer here just
	//means the object hasn't existed long enough yet to have idealBufferSize snapshots, not that the network is
	//struggling to keep up. Without this, every newly created object visibly moves in slow motion for its first
	//several updates while the buffer fills, and how long that takes scales directly with latency
	if (diff > 1.0 && snapshotsEverAdded <= (unsigned int)idealBufferSize)
		diff = 1.0;

	timeBetween *= diff;

	float time = lastFrameTime + timeBetween;

	/*
		0 is the sender restarting our clock outright. 255 is its gap clamped: it only has a byte to say how long it
		has been, so every longer gap arrives as that and none of them is a playback length worth trusting
		An object that has been sitting still sends nothing but the 1500ms heartbeat, so the first update after it
		starts moving again always lands here - creeping to that one over a made up interval, and then queueing
		everything after it behind that, is what left a player who just started walking looking a second behind
		Whatever is queued ahead was timed against a rhythm that just ended, so it goes rather than sitting in front
		of this one and holding playback back. getPosition needs them in time order too
	*/
	if (msSinceLastSend == 0 || msSinceLastSend >= 255)
	{
		time = getTicksMS();

		while (!snapshots.empty() && snapshots.back().time > time)
			snapshots.pop_back();
	}

	snapshots.emplace_back(Snapshot({ pos,rot,time }));
}

glm::vec3 Interpolator::getPosition()
{
	float curTime = getTicksMS();

	for (unsigned int a = 0; a < snapshots.size(); a++)
	{
		if (snapshots[a].time < curTime)
		{
			if (a + 1 >= snapshots.size())
				return snapshots[a].position;

			if (snapshots[a + 1].time < curTime)
				continue;

			//std::cout << "Using " << a << " of " << snapshots.size() << "\n";

			float timeBetween = snapshots[a + 1].time - snapshots[a].time;
			float timePast = curTime - snapshots[a].time;
			float progress = timePast / timeBetween;

			return lerp(snapshots[a].position, snapshots[a + 1].position, progress);
		}
	}

	//Every snapshot is still ahead, like the one a dynamic is made with, which is timed right now
	//Waiting at the first one keeps something new from being drawn at the origin and gliding over to where it really is
	if (!snapshots.empty())
		return snapshots[0].position;

	return glm::vec3(0, 0, 0);
}

glm::quat Interpolator::getRotation()
{
	float curTime = getTicksMS();

	for (unsigned int a = 0; a < snapshots.size(); a++)
	{
		if (snapshots[a].time < curTime)
		{
			if (a + 1 >= snapshots.size())
				return snapshots[a].rotation;

			if (snapshots[a + 1].time < curTime)
				continue;

			float timeBetween = snapshots[a + 1].time - snapshots[a].time;
			float timePast = curTime - snapshots[a].time;
			float progress = timePast / timeBetween;

			return glm::slerp(snapshots[a].rotation, snapshots[a + 1].rotation, progress);
		}
	}

	//Same as getPosition
	if (!snapshots.empty())
		return snapshots[0].rotation;

	return glm::quat(1, 0, 0, 0);
}

