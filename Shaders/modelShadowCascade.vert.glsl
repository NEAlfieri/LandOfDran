#version 330 core

//Same inputs as model.vert, draws straight into one shadow cascade
layout(location = 0) in vec3 ModelSpace;
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

	//What a material with no texture for them is drawn with, see Material's constant values
	vec4 ConstantAlbedo;
	vec4 ConstantMOR;
};

uniform mat4 lightSpaceMatrix;

void main()
{
	//A see-through face plate (MeshFlag_DecalCutout) would cast a square shadow, a face's lines are too thin to miss
	if(!nonInstanced && (InstanceFlags & 524288) != 0)
	{
		gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	mat4 transform = nonInstanced ? TranslationMatrix * RotationMatrix * ScaleMatrix : ModelTransform;
	gl_Position = lightSpaceMatrix * transform * vec4(ModelSpace, 1.0);
}
