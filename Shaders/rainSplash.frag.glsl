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
	float FogHeight;
};

flat in int part;
in vec2 local;
in float age;
in float opacity;

out vec4 color;

void main()
{
	float alpha = 0.0;
	if(part == 0)
	{
		//A thin ring at the edge of the quad, fading as it spreads
		float distance = length(local);
		alpha = smoothstep(0.55, 0.8, distance) * (1.0 - smoothstep(0.8, 1.0, distance)) * (1.0 - age) * 0.7;
	}
	else
	{
		//Three droplets arcing up and out, fallen back down by the end. In world units, the quad is 0.5 wide and 0.2 tall
		vec2 position = vec2(local.x * 0.25, (local.y * 0.5 + 0.5) * 0.2);
		for(int i = 0; i < 3; i++)
		{
			vec2 droplet = vec2(float(i - 1) * 0.12 * age, 4.0 * age * (1.0 - age) * (i == 1 ? 0.16 : 0.11));
			alpha = max(alpha, 1.0 - smoothstep(0.012, 0.025, length(position - droplet)));
		}
		alpha *= (1.0 - age * age) * 0.8;
	}

	alpha *= opacity;
	if(alpha < 0.003)
		discard;

	//Takes on the light of the sky, darker at night
	vec3 tint = clamp(FogColor * 0.8 + SkyColor * 0.15 + vec3(0.1), 0.0, 1.0);
	color = vec4(tint, alpha);
}
