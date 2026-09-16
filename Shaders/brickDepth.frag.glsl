#version 330 core

/*
	The depth pre-pass, paired with brick.vert: fills the depth buffer with the bricks that end up in front
	so the real shading pass never runs on the ones behind them, see renderScene in LoopClient.cpp
	It has to discard exactly where model.frag does, or a brick would block light it doesn't really block
*/

//Bricks only: a BrickMaterial, and where in its grid box this is, see brick.vert
flat in int material;
in vec3 brickLocal;
flat in vec3 brickBoxSize;

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

//Keep in sync with model.frag
const int MaterialHologram = 6;
const float hologramBarSpacing = 1.0;
const float hologramGap = 0.4;

//Same as model.frag's, see the comment there
float distanceAroundBrick()
{
	vec2 halfSize = max(brickBoxSize.xz * 0.5, vec2(0.001));
	vec2 fromCenter = brickLocal.xz - halfSize;

	if(abs(fromCenter.x) / halfSize.x >= abs(fromCenter.y) / halfSize.y)
	{
		if(fromCenter.x > 0.0)
			return 2.0 * halfSize.x + halfSize.y + fromCenter.y;
		return 4.0 * halfSize.x + 3.0 * halfSize.y - fromCenter.y;
	}

	if(fromCenter.y < 0.0)
		return halfSize.x + fromCenter.x;
	return 3.0 * halfSize.x + 2.0 * halfSize.y - fromCenter.x;
}

void main()
{
	//Bars of nothing that walk around a hologram brick, which light goes straight through
	if(material == MaterialHologram)
	{
		float perimeter = 4.0 * (max(brickBoxSize.x, 0.001) + max(brickBoxSize.z, 0.001)) * 0.5;
		float bars = max(1.0, floor(perimeter / hologramBarSpacing + 0.5));
		if(fract(distanceAroundBrick() / perimeter * bars - WaveTime) < hologramGap)
			discard;
	}
}
