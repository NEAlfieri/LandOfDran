#version 330 core

//Decals like bullet holes: flat squares already in world space, lit by model.frag, see Graphics/WorldDecals.h

layout(location = 0) in vec3 WorldSpace;
layout(location = 1) in vec3 NormalVector;
layout(location = 2) in vec3 TangentVector;
layout(location = 3) in vec3 BitangentVector;
layout(location = 4) in vec2 TextureCoords;
//What the decal's albedo is multiplied by, the color of the brick it's on, or white
layout(location = 5) in vec4 Tint;

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

out vec2 uvs;
out vec3 normal;
out vec3 tangent;
out vec3 bitangent;
out vec3 worldPos;
out vec4 preColor;
out float opacity;
flat out int  useDecal;
flat out int  decalCutout;
flat out int  material;
out vec3 brickLocal;
flat out vec3 brickBoxSize;

//MaterialWorldDecal in model.frag
const int MaterialWorldDecal = 100;

void main()
{
	preColor = Tint;
	opacity = Tint.a;
	useDecal = -1;
	decalCutout = 0;
	material = MaterialWorldDecal;
	brickLocal = vec3(0.0);
	brickBoxSize = vec3(0.0);
	uvs = TextureCoords;

	worldPos = WorldSpace;
	normal = NormalVector;
	tangent = TangentVector;
	bitangent = BitangentVector;

	gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), ClipPlane);
	gl_Position = CameraProjection * CameraView * vec4(worldPos,1.0);
}
