#version 330 core

//One instance per particle, see ParticleSystem::update
layout(location = 0) in vec4 ParticlePositionSize;
layout(location = 1) in vec4 ParticleColor;
layout(location = 2) in float ParticleAngle;

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

//Lights placed by Lua, nearest the camera first, see PointLights::update and PointLightUniforms in ShaderSpecification.h
layout (std140) uniform PointLightUniforms
{
	int PointLightCount;
	//World size of one shadow map texel per unit of distance along a cube face's axis
	float PointShadowTexelScale;
	//xyz position, w how far the light reaches
	vec4 PointLightPositionRange[32];
	//rgb color times brightness, a the light's shadow slot, -1 for none
	vec4 PointLightColorShadow[32];
	//xyz which way a spotlight points, w cosine of half its cone angle, below -1 for lights that shine every way
	vec4 PointLightSpotDirection[32];
	//Six cube faces per shadow slot: +x, -x, +y, -y, +z, -z
	mat4 PointShadowMatrices[48];
};

//Only the part lit particles need, see SkyUniforms in ShaderSpecification.h
layout (std140) uniform SkyUniforms
{
	//Day's 9 irradiance coefficients then night's, the first of each is the light arriving from every way on average
	vec4 SkyIrradiance[18];
	float SkyboxBlend;
	int DaySkybox;
	int NightSkybox;
	float SkyLightDay;
	float SkyLightNight;
};

uniform sampler2DArrayShadow ShadowArray;
uniform sampler2DArrayShadow PointShadowArray;
uniform mat4 lightSpaceMatricies[3];

//Particle types with lit set, see ParticleTypeData::lit
uniform bool lit;

out vec2 uv;
out vec4 particleColor;
out float fogFactor;
//For lit particles, the light reaching the particle's middle, before its color and tone mapping like in model.frag
out vec3 incomingLight;

const float PI = 3.14159265359;

//How much sunlight reaches a spot in the air, from one hardware filtered sample of the first cascade it's inside
float sunShadow(vec3 position)
{
	float mapSize = float(textureSize(ShadowArray, 0).x);
	for(int i = 0; i < 3; i++)
	{
		//Lifted a couple of texels toward the light, so a particle resting on a surface isn't shadowed by it
		mat4 light = lightSpaceMatricies[i];
		float texelWorldSize = 2.0 / (length(vec3(light[0][0], light[1][0], light[2][0])) * mapSize);
		vec3 coords = (light * vec4(position + LightDirection * texelWorldSize * 2.0, 1.0)).xyz * 0.5 + 0.5;
		if(any(lessThan(coords.xy, vec2(0.0))) || any(greaterThan(coords.xy, vec2(1.0))))
			continue;

		return texture(ShadowArray, vec4(coords.xy, float(i), clamp(coords.z, 0.0, 1.0)));
	}

	//Nothing past the last cascade is shadowed, though that's only ever deep in the fog
	return 1.0;
}

//How much of a shadowed point light reaches a spot in the air, one sample like sunShadow
float pointShadow(int slot, vec3 position, vec3 fromLight)
{
	vec3 axisDistance = abs(fromLight);
	int face;
	if(axisDistance.x >= axisDistance.y && axisDistance.x >= axisDistance.z)
		face = fromLight.x > 0.0 ? 0 : 1;
	else if(axisDistance.y >= axisDistance.z)
		face = fromLight.y > 0.0 ? 2 : 3;
	else
		face = fromLight.z > 0.0 ? 4 : 5;
	int layer = slot * 6 + face;

	float texelWorldSize = PointShadowTexelScale * max(axisDistance.x, max(axisDistance.y, axisDistance.z));
	vec3 lifted = position - fromLight / max(length(fromLight), 0.0001) * texelWorldSize * 2.0;

	vec4 lightClip = PointShadowMatrices[layer] * vec4(lifted, 1.0);
	vec3 coords = lightClip.xyz / lightClip.w * 0.5 + 0.5;
	return texture(PointShadowArray, vec4(coords.xy, float(layer), clamp(coords.z, 0.0, 1.0)));
}

//Point light reaching a spot from every direction, same falloff and spotlight cones as pointLighting in model.frag
vec3 pointLights(vec3 position)
{
	vec3 total = vec3(0.0);
	for(int i = 0; i < PointLightCount; i++)
	{
		vec3 toLight = PointLightPositionRange[i].xyz - position;
		float distanceSquared = dot(toLight, toLight);
		float range = PointLightPositionRange[i].w;
		if(distanceSquared >= range * range)
			continue;

		float edge = distanceSquared / (range * range);
		float window = clamp(1.0 - edge * edge, 0.0, 1.0);
		float attenuation = window * window / (distanceSquared + 1.0);

		float spotCosine = PointLightSpotDirection[i].w;
		if(spotCosine > -1.5)
			attenuation *= smoothstep(spotCosine, spotCosine + (1.0 - spotCosine) * 0.25, dot(-toLight / max(sqrt(distanceSquared), 0.0001), PointLightSpotDirection[i].xyz));
		if(attenuation <= 0.0)
			continue;

		int slot = int(floor(PointLightColorShadow[i].a + 0.5));
		if(slot >= 0)
			attenuation *= pointShadow(slot, position, -toLight);

		total += PointLightColorShadow[i].rgb * attenuation;
	}
	return total;
}

void main()
{
	//A triangle strip: (-1,-1), (1,-1), (-1,1), (1,1)
	vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;

	//Images are loaded top row first, so the top of the quad is v = 0
	uv = vec2(corner.x, -corner.y) * 0.5 + 0.5;

	float s = sin(ParticleAngle);
	float c = cos(ParticleAngle);
	vec2 spun = vec2(c * corner.x - s * corner.y, s * corner.x + c * corner.y);

	//The rows of the view matrix are the camera's right and up directions, size is the particle's full width
	vec3 center = ParticlePositionSize.xyz;
	vec3 right = vec3(CameraView[0][0], CameraView[1][0], CameraView[2][0]);
	vec3 up = vec3(CameraView[0][1], CameraView[1][1], CameraView[2][1]);
	vec3 worldPos = center + (right * spun.x + up * spun.y) * ParticlePositionSize.w * 0.5;

	float distance = length(CameraPosition - center);
	fogFactor = clamp((distance - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
	particleColor = ParticleColor;

	//What model.frag gives a white surface facing every light at once, including its floor on how dark sun shadows get
	incomingLight = vec3(1.0);
	if(lit)
	{
		vec3 shadowLight = vec3(clamp(sunShadow(center), 0.35, 1.0));
		vec3 ambientShadow = mix(vec3(1.0), shadowLight, ShadowStrength);
		//A .hdr sky replaces the ambient color, like in model.frag
		vec3 skyLight = (SkyLightDay * SkyIrradiance[0].xyz + SkyLightNight * SkyIrradiance[9].xyz) / PI;
		vec3 ambient = AmbientColor * (1.0 - SkyLightDay - SkyLightNight) + skyLight;
		incomingLight = LightColor * shadowLight / PI + ambient * ambientShadow + pointLights(center) / PI;
	}

	gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), ClipPlane);
	gl_Position = CameraProjection * CameraView * vec4(worldPos, 1.0);
}
