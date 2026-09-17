#version 330 core

layout(location = 0) in vec3 ModelSpace;
layout(location = 1) in vec3 NormalVector;
layout(location = 2) in vec3 TangentVector;
layout(location = 3) in vec3 BitangentVector;
layout(location = 4) in vec2 TextureCoords;
layout(location = 5) in vec4 PreColor;
layout(location = 6) in int  InstanceFlags;
layout(location = 7) in mat4 ModelTransform;

layout (std140) uniform BasicUniforms
{
	//Model Matrix:
	mat4 TranslationMatrix;
	mat4 RotationMatrix;
	mat4 ScaleMatrix;
	
	//Material uniforms:
	//These are -1 if not used, otherwise they point to what layer of their 2d texture array they are on
	int useAlbedo;
	int useNormal;
	int useMetalness;
	int useRoughness;
	int useHeight;
	int useAO;
	
	bool nonInstanced;
	bool cameraSpacePosition;

	vec4 DecalArea;
};

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
//See MeshFlag_DecalCutout in Mesh.h
flat out int  decalCutout;
//Bricks only, see brick.vert
flat out int  material;
out vec3 brickLocal;
flat out vec3 brickBoxSize;

void main()
{
	//Hidden but still casting a shadow, like your own player in first person, see ModelInstance::setHidden
	//The shadow shaders don't read the flag, here every vertex goes outside the view so nothing is drawn
	//A non instanced draw has no instance of its own to read flags from, it would get whichever one happens
	//to sit first in the model's buffer, so none of them apply to it, see Mesh::renderOnce
	if(!nonInstanced && (InstanceFlags & 262144) != 0)
	{
		gl_ClipDistance[0] = -1.0;
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	preColor = PreColor;
	opacity = 1.0;
	useDecal = (!nonInstanced && (InstanceFlags & 131072) == 131072) ? ((InstanceFlags & 130560) >> 9) : -1;
	decalCutout = (!nonInstanced && (InstanceFlags & 524288) != 0) ? 1 : 0;
	material = 0;
	brickLocal = vec3(0.0);
	brickBoxSize = vec3(0.0);
	uvs = TextureCoords;
	
	mat4 transform;
	if(nonInstanced)
	{
		preColor = vec4(0,0,0,0);
		transform = TranslationMatrix * RotationMatrix * ScaleMatrix;
	}
	else
		transform = ModelTransform;
	
	worldPos = (transform * vec4(ModelSpace,1)).xyz;
		
	if(cameraSpacePosition)
	{
		//Grass reaches a little past the end of the fog, so its edge is never visible
		float grassRadius = max(300.0, FogDistanceMax + 10.0);
		worldPos = vec3(CameraPosition.x+ModelSpace.x*grassRadius,0,CameraPosition.z+ModelSpace.z*grassRadius);
		uvs = worldPos.xz / 20.0;
	}
	
	normal = (transform * vec4(NormalVector,0)).xyz;
	tangent = (transform * vec4(TangentVector,0)).xyz;
	bitangent = (transform * vec4(BitangentVector,0)).xyz;

	gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), ClipPlane);
	gl_Position = CameraProjection * CameraView * vec4(worldPos,1.0);
}

