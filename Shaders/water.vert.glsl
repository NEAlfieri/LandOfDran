#version 330 core

layout(location = 0) in vec2 GridPosition;

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

//How far the grid reaches from the camera and how big one grid cell is, see LoopClient::waterRadius
uniform float waterRadius;
uniform float gridSpacing;

out vec3 worldPos;
out vec4 clipSpace;

const float TAU = 6.28318530718;

//xy: direction and spatial frequency, z: amplitude. These must match the first waves in water.frag
//Speeds are whole cycles per 100 seconds, which is the period WaveTime wraps at
const int BIG_WAVES = 3;
const vec3 bigWaves[BIG_WAVES] = vec3[](vec3(0.21, 0.13, 0.12), vec3(-0.17, 0.29, 0.08), vec3(0.53, -0.41, 0.04));
const float bigWaveCycles[BIG_WAVES] = float[](11.0, 17.0, 31.0);

void main()
{
	//Snapping to whole cells keeps vertices from sliding across the waves as the camera moves
	vec2 center = floor(CameraPosition.xz / gridSpacing) * gridSpacing;
	vec2 p = center + GridPosition * waterRadius;

	float height = 0.0;
	for(int i = 0; i < BIG_WAVES; i++)
		height += bigWaves[i].z * sin(dot(p, bigWaves[i].xy) + WaveTime * TAU * bigWaveCycles[i] / 100.0);

	worldPos = vec3(p.x, WaterLevel + height, p.y);
	clipSpace = CameraProjection * CameraView * vec4(worldPos, 1.0);
	gl_Position = clipSpace;
}
