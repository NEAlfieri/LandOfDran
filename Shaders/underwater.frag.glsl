#version 330 core

in vec2 uv;

out vec4 color;

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

//The finished scene, only used when distort is true
uniform sampler2D ScreenCopy;

//False if the screen couldn't be copied, then only the tint is drawn and blended over the scene
uniform bool distort;

const float TAU = 6.28318530718;

//Drawn over the whole screen while the camera is under the water
void main()
{
	//WaveTime wraps every 100 seconds, so each wave does a whole number of cycles in that time and loops without a jump
	float cycle = WaveTime * TAU / 100.0;
	float waves = sin(uv.x * 9.0 + uv.y * 4.0 + cycle * 23.0)
		+ sin(uv.x * -6.0 + uv.y * 11.0 + cycle * 31.0)
		+ sin((uv.x + uv.y) * 17.0 - cycle * 41.0) * 0.5;
	waves = waves / 5.0 + 0.5;

	//Deeper blue toward the edges of the screen, with an edge that slowly wobbles
	//Whole multiples of the angle, so the wobble meets itself where atan wraps around
	vec2 centered = uv - 0.5;
	float angle = atan(centered.y, centered.x);
	float wobble = sin(angle * 5.0 + cycle * 29.0) * 0.025 + sin(angle * 9.0 - cycle * 37.0) * 0.015 + (waves - 0.5) * 0.04;
	float vignette = smoothstep(0.2, 0.75, length(centered) + wobble);

	//Darker at night, roughly following how bright the horizon is, like water.frag
	float brightness = clamp(dot(FogColor, vec3(0.333)) * 1.3, 0.05, 1.0);
	vec3 tint = mix(vec3(0.08, 0.38, 0.6), vec3(0.02, 0.2, 0.55), vignette) * brightness;

	float opacity = 0.15 + waves * 0.06 + vignette * (0.4 + waves * 0.1);

	if(!distort)
	{
		color = vec4(tint, opacity);
		return;
	}

	//Like light bending through moving water, the scene shifts along crossing ripples, a bit more toward the edges
	//These also do whole numbers of cycles every 100 seconds
	vec2 ripple = vec2(
		sin(uv.y * 23.0 + uv.x * 5.0 + cycle * 47.0) + sin(uv.y * 41.0 - uv.x * 13.0 - cycle * 61.0) * 0.5,
		sin(uv.x * 19.0 - uv.y * 7.0 + cycle * 43.0) + sin(uv.x * 37.0 + uv.y * 11.0 + cycle * 53.0) * 0.5);
	vec2 offset = ripple * mix(0.0015, 0.004, vignette);

	//Red and blue bend slightly different amounts, which shows as faint color fringes toward the edges
	vec2 fringe = centered * vignette * 0.006;

	//Samples near the border would otherwise pull in the opposite edge of the screen
	vec2 halfTexel = 0.5 / vec2(textureSize(ScreenCopy, 0));
	vec2 at = uv + offset;
	vec3 scene = vec3(
		texture(ScreenCopy, clamp(at + fringe, halfTexel, 1.0 - halfTexel)).r,
		texture(ScreenCopy, clamp(at, halfTexel, 1.0 - halfTexel)).g,
		texture(ScreenCopy, clamp(at - fringe, halfTexel, 1.0 - halfTexel)).b);

	color = vec4(mix(scene, tint, opacity), 1.0);
}
