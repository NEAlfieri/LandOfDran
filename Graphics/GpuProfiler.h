#pragma once

#include "../LandOfDran.h"

/*
	Per pass GPU and CPU timings, shown in the debug menu's Performance tab, see graphics/profiler

	Each zone gets a pair of GL_TIMESTAMP queries per frame. Results are only read back once a few
	frames have gone by, so asking for one never makes the CPU wait on the GPU and the timings
	themselves don't change the frame they're measuring
*/
class GpuProfiler
{
	//Frames of queries kept in flight before their results are read, enough that they're always ready
	static constexpr int framesInFlight = 4;

	struct Zone
	{
		std::string name;
		//How many zones were still open when this one began, for indenting the display
		int depth = 0;

		GLuint queries[framesInFlight][2] = {};
		bool used[framesInFlight] = {};

		//Summed since the last average was worked out, and the worst single frame in that time
		double gpuMS = 0;
		double cpuMS = 0;
		double worstGpuMS = 0;
		double worstCpuMS = 0;
		int samples = 0;

		//What the display shows: an average over the last averageOverMS, and the worst frame in it.
		//The worst is what a stutter is, an average hides it completely
		float shownGpuMS = 0;
		float shownCpuMS = 0;
		float shownWorstGpuMS = 0;
		float shownWorstCpuMS = 0;

		std::chrono::steady_clock::time_point cpuStart;
		//This frame's CPU time, only added to the totals once the frame's GPU result comes back
		double cpuThisFrame[framesInFlight] = {};
	};

	std::vector<Zone> zones;
	//Zones open right now, innermost last
	std::vector<int> openZones;

	int slot = 0;
	//Frames recorded so far, results are only read once framesInFlight of them have gone by
	int framesRecorded = 0;

	bool enabled = false;
	//Turned off for good if the driver has no timer queries
	bool supported = true;

	//How long timings are averaged over before the display changes, so the numbers can actually be read
	static constexpr float averageOverMS = 500.0f;
	float sinceAverageMS = 0;
	unsigned int window = 0;

	//Index of a zone by name, making it if this is the first time it's asked for
	int findZone(const char* name, int depth);

	//Reads back whatever the slot about to be reused measured, and folds it into the averages
	void collect();

	void destroyQueries();

	public:

	struct Result
	{
		std::string name;
		int depth = 0;
		float gpuMS = 0;
		float cpuMS = 0;
		//Worst single frame over the same window, which is what a stutter shows up as
		float worstGpuMS = 0;
		float worstCpuMS = 0;
	};

	//Nothing is timed while this is off, and begin/end cost a branch
	void setEnabled(bool on);
	bool isEnabled() const { return enabled && supported; }

	//Start and finish a frame's timings, everything in between has to be balanced
	void beginFrame(float deltaMS);
	void endFrame();

	//Zones can be nested, and don't have to be the same ones every frame
	void begin(const char* name);
	void end();

	//In the order the zones were first seen, which is the order they're drawn in
	std::vector<Result> getResults() const;

	//One line per zone, for the log, see ExecutableArguments::profileRendering
	std::vector<std::string> getReport() const;

	//Goes up every time the averages are worked out again, so a caller can log a report exactly once per window
	unsigned int getWindow() const { return window; }

	//Averages are kept per zone, so a zone that stops being drawn keeps its last numbers until this is called
	void clear();

	~GpuProfiler();
};

//Times a zone for as long as it's in scope
struct GpuZone
{
	GpuProfiler& profiler;

	GpuZone(GpuProfiler& _profiler, const char* name) : profiler(_profiler) { profiler.begin(name); }
	~GpuZone() { profiler.end(); }
};
