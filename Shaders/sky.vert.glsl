#version 330 core

layout (std140) uniform CameraUniforms
{
	//Camera Uniforms:
	mat4 CameraProjection;
	mat4 CameraView;
	mat4 CameraAngle;
	vec3 CameraPosition;
	vec3 CameraDirection;
};

out vec3 viewRay;

void main()
{
	//One oversized triangle covering the whole screen, no vertex buffer needed
	vec2 ndc = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);

	vec4 viewSpace = inverse(CameraProjection) * vec4(ndc, 1.0, 1.0);
	viewRay = (inverse(CameraAngle) * vec4(viewSpace.xyz / viewSpace.w, 0.0)).xyz;

	//Right on the far plane, so the sky can be drawn after the scene and only reach the pixels nothing else covered
	gl_Position = vec4(ndc, 1.0, 1.0);
}
