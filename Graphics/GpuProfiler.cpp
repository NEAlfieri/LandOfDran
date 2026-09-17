#include "GpuProfiler.h"

int GpuProfiler::findZone(const char* name, int depth)
{
	for (unsigned int a = 0; a < zones.size(); a++)
	{
		if (zones[a].name == name)
			return (int)a;
	}

	Zone zone;
	zone.name = name;
	zone.depth = depth;
	for (int frame = 0; frame < framesInFlight; frame++)
		glGenQueries(2, zone.queries[frame]);

	zones.push_back(zone);
	return (int)zones.size() - 1;
}

void GpuProfiler::collect()
{
	for (Zone& zone : zones)
	{
		if (!zone.used[slot])
			continue;
		zone.used[slot] = false;

		//The frame this slot measured finished long ago, so this never waits, but a driver that
		//somehow lost the result would give a stale number rather than a wrong one
		GLint ready = 0;
		glGetQueryObjectiv(zone.queries[slot][1], GL_QUERY_RESULT_AVAILABLE, &ready);
		if (!ready)
			continue;

		GLuint64 began = 0, ended = 0;
		glGetQueryObjectui64v(zone.queries[slot][0], GL_QUERY_RESULT, &began);
		glGetQueryObjectui64v(zone.queries[slot][1], GL_QUERY_RESULT, &ended);

		//The CPU time was measured when this slot's frame was submitted, so it's folded in here with its
		//own frame's GPU time rather than as it happens, keeping the two columns about the same frames
		double frameGpuMS = (double)(ended - began) / 1000000.0;
		double frameCpuMS = zone.cpuThisFrame[slot];

		zone.gpuMS += frameGpuMS;
		zone.cpuMS += frameCpuMS;
		zone.worstGpuMS = std::max(zone.worstGpuMS, frameGpuMS);
		zone.worstCpuMS = std::max(zone.worstCpuMS, frameCpuMS);
		zone.samples++;
	}
}

void GpuProfiler::setEnabled(bool on)
{
	if (on == enabled)
		return;

	enabled = on;

	//Only checked once something actually wants timings, so a driver query isn't made every launch
	if (enabled && supported && !GLEW_ARB_timer_query && !GLEW_VERSION_3_3)
	{
		supported = false;
		error("This driver has no timer queries, the GPU profiler won't show anything");
	}

	if (!enabled)
	{
		//Queries still in flight measured a frame nobody will ask about
		openZones.clear();
		for (Zone& zone : zones)
		{
			for (int frame = 0; frame < framesInFlight; frame++)
				zone.used[frame] = false;
		}
		framesRecorded = 0;
	}
}

void GpuProfiler::beginFrame(float deltaMS)
{
	if (!isEnabled())
		return;

	openZones.clear();

	//Only read back a slot once its frame has had framesInFlight - 1 frames to finish
	if (framesRecorded >= framesInFlight)
		collect();
	framesRecorded++;

	sinceAverageMS += deltaMS;
	if (sinceAverageMS >= averageOverMS)
	{
		sinceAverageMS = 0;
		window++;
		for (Zone& zone : zones)
		{
			if (zone.samples > 0)
			{
				zone.shownGpuMS = (float)(zone.gpuMS / zone.samples);
				zone.shownCpuMS = (float)(zone.cpuMS / zone.samples);
				zone.shownWorstGpuMS = (float)zone.worstGpuMS;
				zone.shownWorstCpuMS = (float)zone.worstCpuMS;
			}
			zone.gpuMS = 0;
			zone.cpuMS = 0;
			zone.worstGpuMS = 0;
			zone.worstCpuMS = 0;
			zone.samples = 0;
		}
	}
}

void GpuProfiler::begin(const char* name)
{
	if (!isEnabled())
		return;

	int index = findZone(name, (int)openZones.size());
	Zone& zone = zones[index];

	//A zone opened twice in one frame would overwrite its own queries, so only the first is timed.
	//It still goes on the open list, as -1, so its end() pops itself rather than the zone around it
	if (zone.used[slot])
	{
		openZones.push_back(-1);
		return;
	}

	zone.used[slot] = true;
	zone.cpuStart = std::chrono::steady_clock::now();
	glQueryCounter(zone.queries[slot][0], GL_TIMESTAMP);
	openZones.push_back(index);
}

void GpuProfiler::end()
{
	if (!isEnabled() || openZones.empty())
		return;

	int index = openZones.back();
	openZones.pop_back();
	if (index < 0)
		return;

	Zone& zone = zones[index];
	glQueryCounter(zone.queries[slot][1], GL_TIMESTAMP);
	zone.cpuThisFrame[slot] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - zone.cpuStart).count();
}

void GpuProfiler::endFrame()
{
	if (!isEnabled())
		return;

	//An unbalanced begin would leave its queries never ended, and the pair would never be read
	for (int index : openZones)
	{
		if (index >= 0)
			zones[index].used[slot] = false;
	}
	openZones.clear();

	slot = (slot + 1) % framesInFlight;
}

std::vector<GpuProfiler::Result> GpuProfiler::getResults() const
{
	std::vector<Result> results;
	results.reserve(zones.size());
	for (const Zone& zone : zones)
		results.push_back({ zone.name, zone.depth, zone.shownGpuMS, zone.shownCpuMS, zone.shownWorstGpuMS, zone.shownWorstCpuMS });
	return results;
}

std::vector<std::string> GpuProfiler::getReport() const
{
	std::vector<std::string> lines;
	lines.reserve(zones.size() + 1);
	lines.push_back("pass | GPU avg | GPU worst | CPU avg | CPU worst (ms)");

	char line[256];
	for (const Zone& zone : zones)
	{
		snprintf(line, sizeof(line), "%*s%-28s %7.2f %9.2f %7.2f %9.2f", zone.depth * 2, "", zone.name.c_str(),
			zone.shownGpuMS, zone.shownWorstGpuMS, zone.shownCpuMS, zone.shownWorstCpuMS);
		lines.push_back(line);
	}
	return lines;
}

void GpuProfiler::clear()
{
	destroyQueries();
	zones.clear();
	openZones.clear();
	framesRecorded = 0;
	slot = 0;
}

void GpuProfiler::destroyQueries()
{
	for (Zone& zone : zones)
	{
		for (int frame = 0; frame < framesInFlight; frame++)
			glDeleteQueries(2, zone.queries[frame]);
	}
}

GpuProfiler::~GpuProfiler()
{
	destroyQueries();
}
