#version 330 core

in vec3 viewRay;

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

//See SkyUniforms in ShaderSpecification.h, only the part the sky needs
layout (std140) uniform SkyUniforms
{
	vec4 SkyIrradiance[18];
	//0 while it's day, 1 at night
	float SkyboxBlend;
	//SkyboxKind of each: 0 for the gradient below, 1 for screen color images, 2 for a .hdr
	int DaySkybox;
	int NightSkybox;
};

uniform samplerCube SkyDay;
uniform samplerCube SkyNight;

//How far grass and water reach from the camera, see model.vert's cameraSpacePosition and LoopClient::waterRadius
const float surfaceRadius = 300.0;

/*
	How much fog sits between the camera and the sky along ray, so the fogged edge of the grass or water
	blends into the skybox above it. The fog fills everything under FogHeight, so a ray leaves it after
	climbing the rest of the way up to that height, and how far it traveled to get there fogs it the
	same way distance fogs the world, see model.frag
	Keep in sync with water.frag
*/
float skyFogAmount(vec3 ray)
{
	//How much fog is above the camera, negative once the camera has climbed out of it
	float depth = FogHeight - CameraPosition.y;

	//A ray going down or straight ahead never leaves the fog, one above fog it never entered is clear
	if(ray.y <= 0.0)
		return 1.0;
	if(depth <= 0.0)
		return 0.0;

	float throughFog = depth / ray.y;
	return clamp((throughFog - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
}

//One of the two skies along ray, the gradient if Lua didn't pick a skybox for it
//Keep in sync with water.frag
vec3 skyboxColor(int kind, samplerCube cube, vec3 ray, vec3 gradient)
{
	if(kind == 0)
		return gradient;

	vec3 image = textureLod(cube, ray, 0.0).rgb;
	//Tone mapped like model.frag
	if(kind == 2)
		image = pow(image / (image + vec3(1.0)), vec3(1.0 / 2.2));
	return image;
}

//The sun and moon drawn over a sky, except a .hdr, which has its own sun in it
vec3 withSunAndMoon(int kind, vec3 sky, vec3 ray, float visibleHorizon)
{
	if(kind == 2 || ray.y <= visibleHorizon)
		return sky;

	float sunAmount = dot(ray, SunDirection);
	vec3 sunTint = mix(vec3(1.0, 0.45, 0.15), vec3(1.0, 0.95, 0.85), smoothstep(0.0, 0.3, SunDirection.y));
	sky += sunTint * pow(max(sunAmount, 0.0), 300.0) * 0.6;
	sky = mix(sky, sunTint, smoothstep(0.9990, 0.9995, sunAmount));

	float moonAmount = dot(ray, -SunDirection);
	return mix(sky, vec3(0.85, 0.88, 0.95), smoothstep(0.9994, 0.9997, moonAmount));
}

void main()
{
	vec3 ray = normalize(viewRay);

	//Keep in sync with skyColorFor in water.frag
	vec3 gradient = mix(FogColor, SkyColor, smoothstep(0.0, 0.4, ray.y));

	//From above the ground, the fogged far edge of the grass or water sits below straight ahead, with nothing drawn
	//between the two. The sun and moon need to set behind that edge, not straight ahead, or they vanish early
	float height = max(CameraPosition.y - HorizonHeight, 0.0);
	float visibleHorizon = -height / sqrt(height * height + surfaceRadius * surfaceRadius);

	vec3 day = withSunAndMoon(DaySkybox, skyboxColor(DaySkybox, SkyDay, ray, gradient), ray, visibleHorizon);
	vec3 night = withSunAndMoon(NightSkybox, skyboxColor(NightSkybox, SkyNight, ray, gradient), ray, visibleHorizon);

	//The gradient already fades to the fog color at the horizon, only a skybox image needs the fog drawn over it
	float fog = skyFogAmount(ray);
	if(DaySkybox != 0)
		day = mix(day, FogColor, fog);
	if(NightSkybox != 0)
		night = mix(night, FogColor, fog);

	color = vec4(mix(day, night, SkyboxBlend), 1.0);
}
