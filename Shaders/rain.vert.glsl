#version 330 core

//One instance per drop with no data of its own: where each drop is comes from its instance ID, see Rain::render

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

//Seconds, loops every Rain::timeLoopSeconds
uniform float rainTime;

//World size of one pixel per unit away from the camera
uniform float pixelScale;

//Drops fill a box around the camera this far out to each side and this tall, and fall this fast
//boxHeight / fallSpeed has to go into Rain::timeLoopSeconds a whole number of times, or every drop jumps when the time loops
const float boxRadius = 24.0;
const float boxHeight = 32.0;
const float fallSpeed = 40.0;

//World size of a drop's streak, it's stretched out like it's moving fast
const float streakLength = 0.8;
const float streakWidth = 0.02;

out vec3 worldPos;
//x from -1 to 1 across the streak, y from 0 at its top to 1 at its bottom
out vec2 streak;
out float opacity;

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

//Into the span from start to start + size, so a drop keeps its place in the world but always stays near the camera
float wrap(float value, float start, float size)
{
	return start + mod(value - start, size);
}

void main()
{
	uint seed = uint(gl_InstanceID) * 2654435761u + 1u;
	vec3 boxStart = CameraPosition - vec3(boxRadius, boxHeight * 0.5, boxRadius);

	vec3 top;
	top.x = wrap(random(seed) * boxRadius * 2.0, boxStart.x, boxRadius * 2.0);
	top.z = wrap(random(seed) * boxRadius * 2.0, boxStart.z, boxRadius * 2.0);
	top.y = wrap(random(seed) * boxHeight - rainTime * fallSpeed, boxStart.y, boxHeight);
	vec3 bottom = top - vec3(0.0, streakLength * mix(0.7, 1.3, random(seed)), 0.0);

	//Fades toward the edges of the box, so drops don't pop in and out as they wrap around, and right next to the camera
	vec3 fromCamera = top - CameraPosition;
	opacity = (1.0 - smoothstep(0.55, 1.0, length(fromCamera.xz) / boxRadius))
		* (1.0 - smoothstep(0.7, 1.0, abs(fromCamera.y) / (boxHeight * 0.5)))
		* smoothstep(0.5, 2.0, length(fromCamera));

	vec4 viewTop = CameraView * vec4(top, 1.0);
	vec4 viewBottom = CameraView * vec4(bottom, 1.0);

	//Which way the streak runs across the screen, it's widened at a right angle to that
	vec2 screenTop = viewTop.xy / max(-viewTop.z, 0.01);
	vec2 screenBottom = viewBottom.xy / max(-viewBottom.z, 0.01);
	vec2 along = screenBottom - screenTop;
	along = dot(along, along) > 1e-10 ? normalize(along) : vec2(0.0, -1.0);
	vec2 across = vec2(-along.y, along.x);

	//A triangle strip: top left, top right, bottom left, bottom right
	streak = vec2(gl_VertexID % 2 == 0 ? -1.0 : 1.0, gl_VertexID < 2 ? 0.0 : 1.0);

	vec4 viewPos = mix(viewTop, viewBottom, streak.y);

	//At least a pixel wide so it doesn't flicker, and fainter to make up for it
	float width = max(streakWidth, max(-viewPos.z, 0.0) * pixelScale);
	opacity *= streakWidth / width;
	viewPos.xy += across * streak.x * width * 0.5;

	worldPos = mix(top, bottom, streak.y);
	gl_Position = CameraProjection * viewPos;
}
