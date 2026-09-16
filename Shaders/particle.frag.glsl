#version 330 core

in vec2 uv;
in vec4 particleColor;
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

uniform sampler2D ParticleTexture;
//False for particle types without a texture, which are plain squares
uniform bool useTexture;
//Blended with GL_SRC_ALPHA, GL_ONE if true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA if not, see ParticleSystem::render
uniform bool additive;
//See particle.vert
uniform bool lit;

void main()
{
	color = particleColor;
	if(useTexture)
		color *= texture(ParticleTexture, uv);

	if(color.a < 0.004)
		discard;

	//Same steps as model.frag: the color is taken as nonlinear, lit, tone mapped, and gamma corrected
	if(lit)
	{
		color.rgb = pow(color.rgb, vec3(2.2)) * incomingLight;
		color.rgb = color.rgb / (color.rgb + vec3(1.0));
		color.rgb = pow(color.rgb, vec3(1.0 / 2.2));
	}

	//Fog hides an additive particle by having it add less, and an alpha blended one by turning it the fog's color
	if(additive)
		color.rgb *= 1.0 - fogFactor;
	else
		color.rgb = mix(color.rgb, FogColor, fogFactor);
}
