#version 330 core

in vec3 worldPos;
in vec4 clipSpace;

out vec4 color;

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

uniform sampler2D Reflection;
uniform sampler2D Refraction;

//Off when graphics/waterquality is off, reflection is also off while the camera is underwater
uniform bool useReflection;
uniform bool useRefraction;
uniform bool cameraUnderwater;

const float TAU = 6.28318530718;

//The first three are the waves water.vert displaces the mesh with, the rest are ripples only used for shading
const int WAVES = 6;
const vec3 waves[WAVES] = vec3[](
	vec3(0.21, 0.13, 0.12),
	vec3(-0.17, 0.29, 0.08),
	vec3(0.53, -0.41, 0.04),
	vec3(1.3, 0.9, 0.02),
	vec3(-2.1, 1.7, 0.012),
	vec3(3.1, 2.9, 0.006));
const float waveCycles[WAVES] = float[](11.0, 17.0, 31.0, 43.0, 59.0, 73.0);

//Rings spreading out from things moving through the surface, see WaterRipples
//Must match WaterRipples::maxDrawn
const int MAX_RIPPLES = 16;
uniform int RippleCount;
//xy: center on the surface, z: radius of the leading ring, w: width of the band of rings trailing it
uniform vec4 RippleRings[MAX_RIPPLES];
//x: steepness, y: wave number
uniform vec2 RippleWaves[MAX_RIPPLES];

//footprint is how much of the surface one pixel covers, rings finer than that would just sparkle so they fade out
vec2 rippleSlope(vec2 p, float footprint)
{
	vec2 slope = vec2(0.0);
	for(int i = 0; i < RippleCount; i++)
	{
		vec2 fromCenter = p - RippleRings[i].xy;
		float distanceToCenter = length(fromCenter);

		//How far behind the leading ring this is, only the band of rings trailing it is disturbed
		float behind = RippleRings[i].z - distanceToCenter;
		float band = RippleRings[i].w;
		if(behind <= 0.0 || behind >= band)
			continue;

		float waveNumber = RippleWaves[i].y;
		float aliasFade = clamp(2.0 - 2.0 * footprint * waveNumber / 3.14159265, 0.0, 1.0);
		float envelope = sin(3.14159265 * behind / band);

		//Outward slope of rings shaped like sin(waveNumber * behind), which rise and fall across the band
		slope -= fromCenter / distanceToCenter * RippleWaves[i].x * envelope * envelope * cos(waveNumber * behind) * aliasFade;
	}
	return slope;
}

vec3 waveNormal(vec2 p, vec2 rippleSlope)
{
	vec2 slope = rippleSlope;
	for(int i = 0; i < WAVES; i++)
		slope += waves[i].z * waves[i].xy * cos(dot(p, waves[i].xy) + WaveTime * TAU * waveCycles[i] / 100.0);
	return normalize(vec3(-slope.x, 1.0, -slope.y));
}

//Same as in model.frag, see SkyUniforms in ShaderSpecification.h
layout (std140) uniform SkyUniforms
{
	vec4 SkyIrradiance[18];
	float SkyboxBlend;
	int DaySkybox;
	int NightSkybox;
	float SkyLightDay;
	float SkyLightNight;
	float SkyReflectionLevels;
};

uniform samplerCube SkyDay;
uniform samplerCube SkyNight;

//Same as in model.frag
vec3 skyIrradiance(int first, vec3 n)
{
	vec3 irradiance = SkyIrradiance[first].xyz
		+ SkyIrradiance[first + 1].xyz * n.y
		+ SkyIrradiance[first + 2].xyz * n.z
		+ SkyIrradiance[first + 3].xyz * n.x
		+ SkyIrradiance[first + 4].xyz * (n.x * n.y)
		+ SkyIrradiance[first + 5].xyz * (n.y * n.z)
		+ SkyIrradiance[first + 6].xyz * (3.0 * n.z * n.z - 1.0)
		+ SkyIrradiance[first + 7].xyz * (n.x * n.z)
		+ SkyIrradiance[first + 8].xyz * (n.x * n.x - n.y * n.y);
	return max(irradiance, vec3(0.0));
}

//Keep in sync with skyFogAmount in sky.frag
float skyFogAmount(vec3 ray)
{
	float depth = FogHeight - CameraPosition.y;
	if(ray.y <= 0.0)
		return 1.0;
	if(depth <= 0.0)
		return 0.0;

	return clamp((depth / ray.y - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
}

//Keep in sync with skyboxColor in sky.frag
vec3 skyboxColor(int kind, samplerCube cube, vec3 ray, vec3 gradient, float fog)
{
	if(kind == 0)
		return gradient;

	vec3 image = textureLod(cube, ray, 0.0).rgb;
	if(kind == 2)
		image = pow(image / (image + vec3(1.0)), vec3(1.0 / 2.2));
	return mix(image, FogColor, fog);
}

//Same as sky.frag, minus the sun and moon
vec3 skyColorFor(vec3 ray)
{
	vec3 gradient = mix(FogColor, SkyColor, smoothstep(0.0, 0.4, ray.y));
	float fog = skyFogAmount(ray);
	return mix(skyboxColor(DaySkybox, SkyDay, ray, gradient, fog), skyboxColor(NightSkybox, SkyNight, ray, gradient, fog), SkyboxBlend);
}

//Same as in model.frag, see PointLightUniforms in ShaderSpecification.h
layout (std140) uniform PointLightUniforms
{
	int PointLightCount;
	float PointShadowTexelScale;
	vec4 PointLightPositionRange[32];
	vec4 PointLightColorShadow[32];
	vec4 PointLightSpotDirection[32];
	mat4 PointShadowMatrices[48];
};

uniform sampler2DArrayShadow PointShadowArray;

//How much of a shadowed point light reaches the water, one hardware filtered sample since the waves hide hard edges anyway
float pointLightShadow(int slot, vec3 fromLight, vec3 surfaceNormal)
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
	vec4 lightClip = PointShadowMatrices[layer] * vec4(worldPos + surfaceNormal * texelWorldSize * 1.5, 1.0);
	vec3 coords = lightClip.xyz / lightClip.w * 0.5 + 0.5;
	return texture(PointShadowArray, vec4(coords.xy, float(layer), clamp(coords.z, 0.0, 1.0)));
}

//Point lights on the water: a faint glow scattered back from the water itself and glints off the waves, tone mapped like model.frag
vec3 pointLightsOnWater(vec3 normal, vec3 facingNormal, vec3 toCamera)
{
	const vec3 waterAlbedo = vec3(0.05, 0.12, 0.14);

	//Only lights on the camera's side of the surface reach what the camera sees of it
	vec3 flatNormal = cameraUnderwater ? vec3(0.0, -1.0, 0.0) : vec3(0.0, 1.0, 0.0);

	vec3 total = vec3(0.0);
	for(int i = 0; i < PointLightCount; i++)
	{
		vec3 toLight = PointLightPositionRange[i].xyz - worldPos;
		float distanceSquared = dot(toLight, toLight);
		float range = PointLightPositionRange[i].w;
		if(distanceSquared >= range * range)
			continue;

		vec3 L = toLight / max(sqrt(distanceSquared), 0.0001);
		if(dot(flatNormal, L) <= 0.0)
			continue;

		float edge = distanceSquared / (range * range);
		float window = clamp(1.0 - edge * edge, 0.0, 1.0);
		float attenuation = window * window / (distanceSquared + 1.0);

		float spotCosine = PointLightSpotDirection[i].w;
		if(spotCosine > -1.5)
			attenuation *= smoothstep(spotCosine, spotCosine + (1.0 - spotCosine) * 0.25, dot(-L, PointLightSpotDirection[i].xyz));

		int slot = int(floor(PointLightColorShadow[i].a + 0.5));
		if(slot >= 0 && attenuation > 0.0)
			attenuation *= pointLightShadow(slot, -toLight, flatNormal);

		if(attenuation <= 0.0)
			continue;

		//Normalized Blinn-Phong glint with Schlick's Fresnel for water, which reflects about 2% head on
		float NdotL = max(dot(facingNormal, L), 0.0);
		vec3 H = normalize(L + toCamera);
		float glint = cameraUnderwater ? 0.0 : pow(max(dot(normal, H), 0.0), 200.0) * 8.0;
		float reflectance = 0.02 + 0.98 * pow(1.0 - max(dot(H, toCamera), 0.0), 5.0);

		total += PointLightColorShadow[i].rgb * attenuation * NdotL * (waterAlbedo * 2.0 / TAU + glint * reflectance);
	}

	return pow(total / (total + vec3(1.0)), vec3(1.0 / 2.2));
}

void main()
{
	vec3 toCamera = CameraPosition - worldPos;
	float distanceToCamera = length(toCamera);
	toCamera /= distanceToCamera;

	//Far away ripples are smaller than a pixel and just shimmer
	//Taken out here since derivatives aren't defined inside rippleSlope's loop
	float footprint = length(fwidth(worldPos.xz));
	vec3 normal = normalize(mix(waveNormal(worldPos.xz, rippleSlope(worldPos.xz, footprint)), vec3(0, 1, 0), clamp(distanceToCamera / 150.0, 0.0, 1.0)));
	vec3 facingNormal = cameraUnderwater ? -normal : normal;

	vec2 screenUV = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
	vec2 distortion = normal.xz * 0.05;

	//Darker at night, roughly following how bright the horizon is
	vec3 waterTint = vec3(0.1, 0.3, 0.35) * clamp(dot(FogColor, vec3(0.333)) * 1.3, 0.05, 1.0);

	//A .hdr sky lights the water below the surface instead, tone mapped like model.frag
	//This albedo comes out close to the tint above under a typical daytime sky
	float skyLight = SkyLightDay + SkyLightNight;
	if(skyLight > 0.0)
	{
		const vec3 bodyAlbedo = vec3(0.003, 0.04, 0.055);
		vec3 up = vec3(0.0, 1.0, 0.0);
		vec3 lit = bodyAlbedo * (SkyLightDay * skyIrradiance(0, up) + SkyLightNight * skyIrradiance(9, up)) * 2.0 / TAU;
		waterTint = mix(waterTint, pow(lit / (lit + vec3(1.0)), vec3(1.0 / 2.2)), skyLight);
	}

	vec3 refraction = waterTint;
	if(useRefraction)
		refraction = mix(texture(Refraction, clamp(screenUV + distortion, 0.001, 0.999)).rgb, waterTint, 0.3);

	//The reflection camera keeps a normal up vector, so its image is upside down compared to a real reflection
	vec3 reflection;
	if(useReflection)
		reflection = texture(Reflection, clamp(vec2(screenUV.x, 1.0 - screenUV.y) + distortion, 0.001, 0.999)).rgb;
	else
		reflection = skyColorFor(reflect(-toCamera, normal));

	//See-through looking straight down, reflective at grazing angles, never reflective from underneath
	float fresnel = cameraUnderwater ? 0.0 : pow(1.0 - max(dot(toCamera, facingNormal), 0.0), 3.0);

	vec3 halfVector = normalize(LightDirection + toCamera);
	float specular = cameraUnderwater ? 0.0 : pow(max(dot(normal, halfVector), 0.0), 300.0);

	vec3 result = mix(refraction, reflection, fresnel) + LightColor * specular * 0.1;

	float fogFactor = clamp((distanceToCamera - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
	color = vec4(mix(result, FogColor, fogFactor), 1.0);
}
