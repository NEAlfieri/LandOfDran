#version 330 core

//One instance per light with a corona, see PointLights::update
layout(location = 0) in vec4 CoronaPositionWidth;
layout(location = 1) in vec4 CoronaColorStrength;

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

//-1 to 1 across the quad
out vec2 offset;
out vec3 glowColor;

void main()
{
	//A triangle strip: (-1,-1), (1,-1), (-1,1), (1,1)
	offset = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;

	vec3 center = CoronaPositionWidth.xyz;
	float halfWidth = CoronaPositionWidth.w * 0.5;

	//Pulled toward the camera by up to half its width, so a wall right behind the light doesn't cut the glow in half
	vec3 toCamera = CameraPosition - center;
	float distance = length(toCamera);
	center += toCamera / max(distance, 0.001) * min(halfWidth, distance * 0.5);

	//The rows of the view matrix are the camera's right and up directions
	vec3 right = vec3(CameraView[0][0], CameraView[1][0], CameraView[2][0]);
	vec3 up = vec3(CameraView[0][1], CameraView[1][1], CameraView[2][1]);
	vec3 worldPos = center + (right * offset.x + up * offset.y) * halfWidth;

	float fogFactor = clamp((distance - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
	glowColor = CoronaColorStrength.rgb * CoronaColorStrength.a * (1.0 - fogFactor);

	gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), ClipPlane);
	gl_Position = CameraProjection * CameraView * vec4(worldPos, 1.0);
}
