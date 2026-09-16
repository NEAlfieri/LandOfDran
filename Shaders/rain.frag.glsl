#version 330 core

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
};

//Depth from straight above of the area around the camera, see Rain
uniform sampler2D RainMap;

in vec3 worldPos;
in vec2 streak;
in float opacity;

out vec4 color;

//How see-through a drop is at its most solid
const float dropOpacity = 0.35;

void main()
{
	//Nothing falls through the ground, the water, or anything overhead
	if(worldPos.y < HorizonHeight)
		discard;

	vec2 mapUV = (worldPos.xz - RainMapArea.xy) / RainMapArea.z;
	if(all(greaterThanEqual(mapUV, vec2(0.0))) && all(lessThanEqual(mapUV, vec2(1.0))) &&
		worldPos.y < mix(RainMapTop, RainMapBottom, textureLod(RainMap, mapUV, 0.0).r))
		discard;

	//Soft across, and fading in from the top of the streak
	float alpha = (1.0 - streak.x * streak.x) * mix(0.3, 1.0, streak.y) * opacity * dropOpacity;

	//Takes on the light of the sky. At night that leaves drops nearly the same color as everything behind them, so they're
	//lifted toward moonlit white as the sky darkens, which leaves daylight alone
	float darkness = 1.0 - clamp(dot(FogColor, vec3(0.333)) * 3.0, 0.0, 1.0);
	vec3 tint = clamp(FogColor * 0.75 + SkyColor * 0.15 + vec3(0.06 + 0.22 * darkness), 0.0, 1.0);
	color = vec4(tint, alpha);
}
