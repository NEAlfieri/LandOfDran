#version 330 core

/*
	Paired with sky.vert, which puts one triangle right on the far plane: the source the rays are blurred
	out from, see LoopClient::renderGodRays. Everything that blocks the sun has already been drawn flat
	black into this target and filled its depth, so this only reaches the pixels where the sky still shows,
	and paints the sun disc into them. godRay.frag reads a black pixel as "nothing of the sun along here"

	The disc is a little wider than the one sky.frag draws, so a sun mostly hidden behind a wall still has
	an edge left to stream out of
*/

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

//How far grass and water reach from the camera, same as sky.frag's
const float surfaceRadius = 300.0;

void main()
{
	vec3 ray = normalize(viewRay);

	//The same visible horizon sky.frag sets the sun behind, so the rays go out with it rather than hanging
	//in the air over the fogged far edge of the ground
	float height = max(CameraPosition.y - HorizonHeight, 0.0);
	float visibleHorizon = -height / sqrt(height * height + surfaceRadius * surfaceRadius);
	if(ray.y <= visibleHorizon)
	{
		color = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	/*
		The disc, plus a wide soft halo around it. The halo matters more than it looks: a ray's brightness is
		how much of the line to the sun crossed lit sky, so a hard little disc is only crossed for a moment
		and the rays die within a disc width of it. Spreading the source out carries them much further, and
		softens the ring-shaped contours a hard edge leaves as the march steps over it
	*/
	float sunAmount = max(dot(ray, SunDirection), 0.0);
	float disc = smoothstep(0.9975, 0.9993, sunAmount);
	float halo = pow(sunAmount, 900.0);
	color = vec4(vec3(max(disc, halo)), 1.0);
}
