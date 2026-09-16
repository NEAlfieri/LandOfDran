#version 330 core

//Same inputs as brick.vert, draws straight into one shadow cascade, see InstancedBrickRenderer::renderShadowCascade
layout(location = 0) in vec3 CubePosition;
layout(location = 5) in vec3 BrickCorner;
layout(location = 6) in vec3 BrickSize;
layout(location = 7) in vec4 BrickColor;
layout(location = 8) in vec4 VertexColor;
layout(location = 9) in float BrickAngle;
layout(location = 10) in float BrickMaterial;

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

uniform mat4 lightSpaceMatrix;

//Identity for placed bricks, a vehicle's transform for its bricks, see InstancedBrickRenderer::renderShadowCascade
//skipPoint is in the same space as BrickCorner, so for a vehicle it's the light's position in the vehicle's space
uniform mat4 brickTransform;

//Bricks less opaque than this are skipped: half for plain shadows, 0 for the colored shadow passes where every transparent brick tints
uniform float minOpacity;

//Drawing special bricks' shapes, see brick.vert
uniform bool specialMesh;

//Point light shadows: bricks whose grid box has skipPoint (the light) inside don't cast, so a light in the middle of its brick isn't buried by it
uniform bool skipContaining;
uniform vec3 skipPoint;

//Only used by shadowTint.frag
out vec4 casterColor;

//STUD_SIZE and PLATE_SIZE in Bricks/Brick.h
const vec3 gridScale = vec3(1.0, 0.4, 1.0);

//Same shape effects as brick.vert
const float loopRadians = 6.2831853 / 100.0;
const float unduloAmplitude = 0.3;
const float bouncyStretch = 0.35;

float bouncyScale()
{
	return 1.0 + bouncyStretch * (0.5 - 0.5 * cos(WaveTime * loopRadians * 80.0));
}

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
	casterColor = BrickColor;
	if(specialMesh && VertexColor.a >= 0.01)
		casterColor = vec4(VertexColor.rgb, VertexColor.a * BrickColor.a);

	//Collapsing every corner to one point leaves nothing to rasterize
	if(casterColor.a < minOpacity)
	{
		gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
		return;
	}

	//BrickCorner and BrickSize are the turned box a brick fills on the grid, special or not
	if(skipContaining && all(greaterThan(skipPoint, BrickCorner * gridScale)) && all(lessThan(skipPoint, (BrickCorner + BrickSize) * gridScale)))
	{
		gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
		return;
	}

	//Point light shadows (the passes with skipContaining) are only redrawn now and then, so they keep the brick's still shape
	//instead of one frozen partway through moving
	int material = int(BrickMaterial + 0.5);
	bool animate = !skipContaining;
	float stretch = animate && material == 2 ? bouncyScale() : 1.0;

	vec3 worldPos;
	if(specialMesh)
	{
		float c = cos(BrickAngle * 1.5707963);
		float s = sin(BrickAngle * 1.5707963);
		mat3 turn = mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);

		vec3 shape = turn * CubePosition;
		float bottom = -BrickSize.y * gridScale.y * 0.5;
		shape.y = (shape.y - bottom) * stretch + bottom;
		worldPos = (BrickCorner + BrickSize * 0.5) * gridScale + shape;
	}
	else
	{
		vec3 shape = CubePosition * BrickSize * gridScale;
		shape.y *= stretch;
		worldPos = BrickCorner * gridScale + shape;
	}

	worldPos = (brickTransform * vec4(worldPos, 1.0)).xyz;

	if(animate && material == 1)
		worldPos += unduloOffset(worldPos);

	gl_Position = lightSpaceMatrix * vec4(worldPos, 1.0);
}
