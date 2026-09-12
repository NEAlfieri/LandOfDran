#pragma once

#include "../LandOfDran.h"
#include "../Utility/GlobalStartup.h" //getTicksMS

struct Snapshot
{
	glm::vec3 position = glm::vec3(0, 0, 0);
	glm::quat rotation = glm::quat(1, 0, 0, 0);
	float time = 0;
};

/*
	Takes rotation and position snapshots with associated time from server
	and can be called on at any point to give an interpolated transform for the current time point
*/
class Interpolator
{
	private:

	std::vector<Snapshot> snapshots;
	int testLastRemoveMe = 0;

	//How many snapshots this interpolator has ever received, used to tell "still ramping up right after creation"
	//(buffer is small because the object hasn't existed long enough yet) apart from "buffer shrank because the
	//network is struggling" (an established object should have already reached idealBufferSize) - see addSnapshot
	unsigned int snapshotsEverAdded = 0;

	public:

	int getNumSnapshots() const { return snapshots.size(); }

	void addSnapshot(const glm::vec3& pos, const glm::quat& rot, float idealBufferSize, unsigned int msSinceLastSend);
	glm::vec3 getPosition();
	glm::quat getRotation();
};