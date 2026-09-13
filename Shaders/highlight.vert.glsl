#version 330 core

layout(location = 0) in vec3 ModelSpace;
layout(location = 7) in mat4 ModelTransform;
layout(location = 11) in vec4 HighlightColor;
layout(location = 12) in float HighlightThickness;
layout(location = 13) in vec3 SmoothNormal;

layout (std140) uniform BasicUniforms
{
	//Model Matrix:
	mat4 TranslationMatrix;
	mat4 RotationMatrix;
	mat4 ScaleMatrix;

	//Material uniforms:
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

out vec4 highlightColor;

/*
	When true, renders this instance at its true (non-inflated) shape instead of the extruded outline shell.
	Used for the stencil mask sub-pass that marks the highlighted object's own silhouette, so the outline
	pass afterward can exclude exactly that area - see LoopClient::renderEverything
*/
uniform bool maskPass;

void main()
{
	highlightColor = HighlightColor;

	//No highlight applied to this instance, collapse to a degenerate triangle so nothing gets rasterized
	if(HighlightColor.a <= 0.0)
	{
		gl_Position = vec4(0,0,0,0);
		return;
	}

	vec3 worldPos = (ModelTransform * vec4(ModelSpace,1)).xyz;

	if(!maskPass)
	{
		//Using the averaged SmoothNormal (see Mesh.h) rather than a mesh's usual per-face normal keeps the
		//extruded shell welded together across adjacent faces instead of tearing apart at every hard edge
		vec3 rawNormal = (ModelTransform * vec4(SmoothNormal,0)).xyz;
		float rawNormalLength = length(rawNormal);

		//Guards against a degenerate ModelTransform (e.g. a hidden instance, whose transform is zeroed out) which
		//would otherwise turn a normalize() of a zero vector into NaNs that survive into gl_Position
		if(rawNormalLength < 0.0001)
		{
			gl_Position = vec4(0,0,0,0);
			return;
		}

		//Push the surface outward along its normal; only backfaces are drawn during this pass (see the GL_FRONT
		//culling set up around this shader's use), so this inflated shell peeks out from behind the model's own
		//frontfaces to form the outline silhouette
		worldPos += (rawNormal / rawNormalLength) * HighlightThickness;
	}

	gl_Position = CameraProjection * CameraView * vec4(worldPos,1.0);
}
