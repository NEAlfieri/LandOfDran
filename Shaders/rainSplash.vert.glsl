#version 330 core

//One instance per splash with no data of its own: each plays over and over, somewhere new near the camera every time, see Rain::render

layout (std140) uniform CameraUniforms
{
	//Camera Uniforms:
	mat4 CameraProjection;
	mat4 CameraView;
	mat4 CameraAngle;
	vec3 CameraPosition;
	vec3 CameraDirection;
};

layout (std140) uniform EnvironmentUniforms
{
	//See EnvironmentUniforms in ShaderSpecification.h
	vec3 SunDirection;
	float FogDistanceMin;
	vec3 LightDirection;
	float FogDistanceMax;
	vec3 LightColor;
	float WaveTime;
	vec3 SkyColor;
	float WaterLevel;
	vec3 FogColor;
	float HorizonHeight;
	vec4 ClipPlane;
	vec3 AmbientColor;
	float ShadowStrength;
	float RainIntensity;
	float RainWetness;
	float RainMapTop;
	float RainMapBottom;
	vec4 RainMapArea;
	float FogHeight;
};

//Depth from straight above of the area around the camera, see Rain
uniform sampler2D RainMap;

//Seconds, loops every Rain::timeLoopSeconds
uniform float rainTime;

//Splashes land within this far of the camera to each side, and each lasts this long
//splashSeconds has to go into Rain::timeLoopSeconds a whole number of times, or every splash skips when the time loops
const float splashRadius = 20.0;
const float splashSeconds = 0.4;

//Part 0 is a ring spreading across the surface, part 1 droplets thrown up from it
flat out int part;
//-1 to 1 across the part's quad
out vec2 local;
//0 to 1 over the splash's life
out float age;
out float opacity;

//Two triangles making a quad from -1 to 1
const vec2 corners[6] = vec2[6](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));

//PCG, see Jarzynski and Olano, Hash Functions for GPU Rendering
uint hashInteger(uint value)
{
	uint state = value * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

//0 to 1, a different number each call
float random(inout uint seed)
{
	seed = hashInteger(seed);
	return float(seed) / 4294967295.0;
}

//Into the span from start to start + size, so a splash stays put in the world while the camera moves
float wrap(float value, float start, float size)
{
	return start + mod(value - start, size);
}

void main()
{
	uint seed = uint(gl_InstanceID) * 2654435761u + 7u;
	float cycles = rainTime / splashSeconds + random(seed);
	float size = mix(0.7, 1.3, random(seed));
	age = fract(cycles);

	//Somewhere new each time it plays
	uint spot = hashInteger(uint(gl_InstanceID) ^ (uint(floor(cycles)) * 2246822519u));
	vec2 start = CameraPosition.xz - vec2(splashRadius);
	vec3 center = vec3(wrap(random(spot) * splashRadius * 2.0, start.x, splashRadius * 2.0), HorizonHeight,
		wrap(random(spot) * splashRadius * 2.0, start.y, splashRadius * 2.0));

	//Lands on the topmost thing there, or the ground or water if that's higher
	vec2 mapUV = (center.xz - RainMapArea.xy) / RainMapArea.z;
	if(all(greaterThanEqual(mapUV, vec2(0.0))) && all(lessThanEqual(mapUV, vec2(1.0))))
		center.y = max(center.y, mix(RainMapTop, RainMapBottom, textureLod(RainMap, mapUV, 0.0).r));

	vec3 fromCamera = center - CameraPosition;
	opacity = 1.0 - smoothstep(0.5, 1.0, length(fromCamera.xz) / splashRadius);

	part = gl_VertexID / 6;
	local = corners[gl_VertexID % 6];

	//Too faint, or far above or below, like the ground under a tall tower
	if(opacity <= 0.0 || abs(fromCamera.y) > 40.0)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	vec3 worldPos;
	if(part == 0)
	{
		//Spreads quickly then slows, lifted a little off the surface so it doesn't fight it for depth
		float radius = mix(0.04, 0.3, sqrt(age)) * size;
		worldPos = center + vec3(local.x * radius, 0.03, local.y * radius);
	}
	else
	{
		//Standing up and turned to face the camera, 0.5 wide and 0.2 tall
		vec3 toCamera = vec3(-fromCamera.x, 0.0, -fromCamera.z);
		vec3 right = dot(toCamera, toCamera) > 1e-6 ? normalize(vec3(toCamera.z, 0.0, -toCamera.x)) : vec3(1.0, 0.0, 0.0);
		worldPos = center + right * local.x * 0.25 * size + vec3(0.0, (local.y * 0.5 + 0.5) * 0.2 * size, 0.0);
	}

	gl_Position = CameraProjection * CameraView * vec4(worldPos, 1.0);
}
