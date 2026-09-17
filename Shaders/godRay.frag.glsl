#version 330 core

/*
	Paired with underwater.vert, blended onto the finished scene: the rays themselves, see
	LoopClient::renderGodRays

	Every pixel marches across godRayMask toward wherever the sun is on screen and adds up how much of the
	way there is open sky. A pixel with a clear line to the sun collects a lot, one behind the shadow a wall
	throws out from the sun's edge collects little, and the difference between them is the ray. Weight falls
	off along the march so a pixel takes most of its light from the sky right beside it, which is what bends
	the result into streaks instead of an even glow around the sun
*/

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

//Where the sky still shows and how bright the sun is there, see godRayMask.frag
uniform sampler2D GodRayMask;

//Where the sun is in the mask's texture coordinates, off the edge of it if the sun is off screen
uniform vec2 sunScreenPosition;

//graphics/godrayquality, how many places along the march are looked at
uniform int sampleCount;

//How bright the rays come out, already faded for sunset, night, and the sun going behind the camera
uniform float strength;

//How far along the way to the sun the march goes, 1 reaches it
const float rayDensity = 1.0;

//How much the far end of the march counts against the near end. Applied over the whole march rather than
//per sample, so the samples setting only changes how smooth the rays are, never how they look
const float rayFalloff = 0.35;

//Below 1 this lifts the faint far end of a ray toward the bright end, see where it's used
const float rayContrast = 0.45;

void main()
{
	vec2 stride = (uv - sunScreenPosition) * (rayDensity / float(sampleCount));
	float perSample = pow(rayFalloff, 1.0 / float(sampleCount));

	vec2 coord = uv;
	float weight = 1.0;
	float total = 0.0;
	float totalWeight = 0.0;

	for(int i = 0; i < sampleCount; i++)
	{
		coord -= stride;
		//The mask reads black outside itself (GL_CLAMP_TO_BORDER), so a march that leaves the screen
		//looking for a sun just off the edge collects nothing out there rather than smearing
		total += texture(GodRayMask, coord).r * weight;
		totalWeight += weight;
		weight *= perSample;
	}

	float rays = totalWeight > 0.0 ? total / totalWeight : 0.0;

	//How much of the line was lit falls off like 1/distance from the sun, which puts the streaks a hundred
	//times below the glare right at it. This lifts them back into the picture without blowing out that core
	rays = pow(rays, rayContrast);

	//The color of the sun disc sky.frag draws, orange while it's low and near white overhead
	vec3 sunTint = mix(vec3(1.0, 0.45, 0.15), vec3(1.0, 0.95, 0.85), smoothstep(0.0, 0.3, SunDirection.y));

	color = vec4(sunTint * rays * strength, 1.0);
}
