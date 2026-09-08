#version 330 core

layout(location = 0) in vec2 ModelSpace;
layout(location = 1) in vec3 FacePosition;
layout(location = 2) in vec2 FaceSize;
layout(location = 3) in uint FaceDirection;

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

uniform mat4 lightSpaceMatricies[3];

out vec2 uvs;
out vec3 normal;
out vec3 tangent;
out vec3 bitangent;
out vec3 worldPos;
out vec4 preColor;
flat out int  useDecal;
out vec4 shadowPos[3];

uniform vec2 uvsByIdxFront[6] = {
	vec2(1,0),
	vec2(0,0),
	vec2(1,1),
	vec2(0,1),
	vec2(1,1),
	vec2(0,0)
};

uniform vec2 uvsByIdxBack[6] = {
	vec2(1,0),
	vec2(1,1),
	vec2(0,0),
	vec2(0,1),
	vec2(0,0),
	vec2(1,1)
};

uniform vec3 normalsByDirection[6] = {
	vec3(0,0,1), 
	vec3(0,1,0), //top
	vec3(1,0,0),
	vec3(-1,0,0),
	vec3(0,-1,0), //bottom
	vec3(0,0,-1)
};

uniform int brickFaceDirection;
uniform vec3 brickChunkPos;

void main()
{
	preColor = vec4(0,0,0,0);
	useDecal = -1;
	
	uvs = (brickFaceDirection > 3) ? uvsByIdxBack[gl_VertexID % 6] : uvsByIdxFront[gl_VertexID % 6];
	
	mat4 transform = TranslationMatrix * RotationMatrix * ScaleMatrix;
	
	vec2 coords = FaceSize * ModelSpace;
	
	vec3 vertexPosition;
	switch(FaceDirection)
	{
		case 0u:
			vertexPosition.x = coords.x;
			vertexPosition.y = coords.y;
		case 1u:
			vertexPosition.x = coords.x;
			vertexPosition.z = coords.y;
		case 2u:
			vertexPosition.z = coords.x;
			vertexPosition.y = coords.y;
		case 3u:
			vertexPosition.z = coords.x;
			vertexPosition.y = coords.y;
		case 4u:
			vertexPosition.x = coords.x;
			vertexPosition.z = coords.y;
		case 5u:
			vertexPosition.x = coords.x;
			vertexPosition.y = coords.y;

	}
	vertexPosition += FacePosition;
	vertexPosition += brickChunkPos;
	
	worldPos = (transform * vec4(vertexPosition,1)).xyz;
	
	normal = (transform * vec4(normalsByDirection[brickFaceDirection],0)).xyz;
	//tangent = (transform * vec4(TangentVector,0)).xyz;
	//bitangent = (transform * vec4(BitangentVector,0)).xyz;
	
	for(int i = 0; i<3; i++)
		shadowPos[i] = lightSpaceMatricies[i] * vec4(worldPos,1.0);
	
	gl_Position = CameraProjection * CameraView * vec4(worldPos,1.0);
}

