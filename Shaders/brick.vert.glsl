#version 330 core

//The shared cube, see makeCube in Graphics/InstancedBrickRenderer.cpp
layout(location = 0) in vec3 CubePosition;
layout(location = 1) in vec3 CubeNormal;
layout(location = 2) in vec3 CubeTangent;
layout(location = 3) in vec3 CubeBitangent;
layout(location = 4) in vec2 CubeUV;

//Per brick, in studs horizontally and plates vertically, with width and length already swapped for rotation
layout(location = 5) in vec3 BrickCorner;
layout(location = 6) in vec3 BrickSize;
layout(location = 7) in vec4 BrickColor;

//Special bricks only: with specialMesh set, locations 0-4 are the type's shape in world units around its middle instead of the cube
//A vertex color with alpha above 0 replaces the brick's color on that face
layout(location = 8) in vec4 VertexColor;
//Quarter turns counter-clockwise around +y, see Brick::getAngle
layout(location = 9) in float BrickAngle;

//A BrickMaterial from Bricks/Brick.h
layout(location = 10) in float BrickMaterial;

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
};

//Top and bottom faces repeat their texture once per stud, side faces stretch it once across the whole face
uniform bool tileByStuds;

//Identity for placed bricks, rotation and position for loose bricks like undo debris
uniform mat4 brickTransform;

//Drawing special bricks, see the inputs above
uniform bool specialMesh;

//STUD_SIZE and PLATE_SIZE in Bricks/Brick.h
const vec3 gridScale = vec3(1.0, 0.4, 1.0);

//Shape effects, the same in brickShadowCascade.vert, and InstancedBrickRenderer.cpp pads chunk bounds by them
//WaveTime starts over every 100 seconds, so every animation speed is a whole number of cycles per 100 seconds
const float loopRadians = 6.2831853 / 100.0;
//Farthest an undulo brick's corner moves along each axis, in world units
const float unduloAmplitude = 0.3;
//A bouncy brick grows up to this much of its height taller
const float bouncyStretch = 0.35;

/*
	The depth pre-pass draws these same bricks through brickDepth.frag instead, and the shading pass then
	only keeps fragments at exactly the depth it left behind. Two programs sharing this vertex shader are
	free to optimize it differently unless gl_Position is invariant, which would make those depths disagree
	by the odd last bit and drop whole faces out of the picture
*/
invariant gl_Position;

out vec2 uvs;
out vec3 normal;
out vec3 tangent;
out vec3 bitangent;
out vec3 worldPos;
out vec4 preColor;
out float opacity;
flat out int useDecal;
flat out int decalCutout;
flat out int material;
//Where in the brick's grid box this is, from its min corner in world units before shape effects, and that box's size
out vec3 brickLocal;
flat out vec3 brickBoxSize;

//How much taller a bouncy brick is right now
float bouncyScale()
{
	return 1.0 + bouncyStretch * (0.5 - 0.5 * cos(WaveTime * loopRadians * 80.0));
}

//Neighbouring bricks share corner positions, so they wiggle together without opening gaps
vec3 unduloOffset(vec3 position)
{
	float t = WaveTime * loopRadians;
	return unduloAmplitude * vec3(
		sin(position.x * 1.3 + position.y * 0.9 + t * 60.0),
		cos(position.y * 1.7 + position.z * 1.1 + t * 45.0),
		sin(position.z * 1.3 + position.x * 0.7 + t * 70.0));
}

void main()
{
	material = int(BrickMaterial + 0.5);
	float stretch = material == 2 ? bouncyScale() : 1.0;

	mat3 rotation = mat3(brickTransform);
	brickBoxSize = BrickSize * gridScale;

	if(specialMesh)
	{
		float c = cos(BrickAngle * 1.5707963);
		float s = sin(BrickAngle * 1.5707963);
		mat3 turn = mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);

		vec3 center = (BrickCorner + BrickSize * 0.5) * gridScale;
		vec3 shape = turn * CubePosition;
		brickLocal = shape + brickBoxSize * 0.5;

		//Stretches up from the bottom of its box
		float bottom = -brickBoxSize.y * 0.5;
		shape.y = (shape.y - bottom) * stretch + bottom;
		worldPos = (brickTransform * vec4(center + shape, 1.0)).xyz;

		rotation = rotation * turn;
		uvs = CubeUV;

		bool painted = VertexColor.a < 0.01;
		preColor = vec4(painted ? BrickColor.rgb : VertexColor.rgb, 1.0);
		opacity = BrickColor.a * (painted ? 1.0 : VertexColor.a);
	}
	else
	{
		vec3 shape = CubePosition * brickBoxSize;
		brickLocal = shape;
		shape.y *= stretch;
		worldPos = (brickTransform * vec4(BrickCorner * gridScale + shape, 1.0)).xyz;

		//Tiled faces are the top and bottom, whose texture axes run along x and z
		uvs = tileByStuds ? CubeUV * BrickSize.xz : CubeUV;

		preColor = vec4(BrickColor.rgb, 1.0);
		opacity = BrickColor.a;
	}

	if(material == 1)
		worldPos += unduloOffset(worldPos);

	normal = rotation * CubeNormal;
	tangent = rotation * CubeTangent;
	//Brick normal maps point green toward the top of the image, which is v = 0 since textures load unflipped, so it runs against the face's v
	bitangent = -(rotation * CubeBitangent);
	useDecal = -1;
	decalCutout = 0;

	gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), ClipPlane);
	gl_Position = CameraProjection * CameraView * vec4(worldPos,1.0);
}
