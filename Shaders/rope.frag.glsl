#version 330 core

in float across;
in float along;
in vec4 ropeColor;
in float fogFactor;
in vec3 incomingLight;

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

void main()
{
	//The ribbon is flat, so it's darkened toward its edges the way something round would be
	float roundness = sqrt(max(1.0 - across * across, 0.0));

	//Strands wound around it, slanting across the ribbon
	float strands = 0.5 + 0.5 * sin((along * 2.0 + across * 0.9) * 3.14159265);

	color = ropeColor;
	color.rgb *= mix(0.45, 1.0, roundness) * mix(0.7, 1.0, strands);

	//Same steps as model.frag: the color is taken as nonlinear, lit, tone mapped, and gamma corrected
	color.rgb = pow(color.rgb, vec3(2.2)) * incomingLight;
	color.rgb = color.rgb / (color.rgb + vec3(1.0));
	color.rgb = pow(color.rgb, vec3(1.0 / 2.2));

	color.rgb = mix(color.rgb, FogColor, fogFactor);
}
