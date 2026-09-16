#version 330 core

out vec4 color;

in vec2 uvs;
in vec3 worldPos;
in vec3 tangent;
in vec3 bitangent;
in vec4 preColor;
//Only below 1 for transparent bricks, which are drawn with blending on
in float opacity;
in vec3 normal;
flat in int useDecal;
//Only the decal is drawn, see MeshFlag_DecalCutout in Mesh.h
flat in int decalCutout;
//Bricks only, 0 for everything else: a BrickMaterial, and where in its grid box this is, see brick.vert
flat in int material;
in vec3 brickLocal;
flat in vec3 brickBoxSize;

//BrickMaterial in Bricks/Brick.h, Undulo and Bouncy only change the shape in brick.vert
const int MaterialPearl = 3;
const int MaterialChrome = 4;
const int MaterialBlink = 5;
const int MaterialHologram = 6;
const int MaterialGlow = 7;
const int MaterialSlippery = 8;
const int MaterialFoil = 9;
const int MaterialRainbow = 10;

//About how far apart hologram bars are, in world units, a brick gets however many fit evenly around it
const float hologramBarSpacing = 1.0;
//Share of each bar spacing that's see-through
const float hologramGap = 0.4;

//How much of the brick's paint the rainbow and foil hues replace, 0 to 1
const float rainbowStrength = 0.5;
const float foilStrength = 0.25;
//How far foil's crinkles tilt the surface
const float foilCrinkle = 0.15;

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

	//Texture coordinates of a decal's top left and bottom right corners, see Mesh::decalArea
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

uniform sampler2DArray PBRArray;

//Depth from straight above of the area around the camera, only bound while RainWetness is above 0, see Rain
uniform sampler2D RainMap;
uniform sampler2DArray DecalArray;
uniform sampler2DArrayShadow ShadowArray;

//Sun or moon view of each shadow cascade, nearest first, see Camera::calculateLightSpaceMatricies
uniform mat4 lightSpaceMatricies[3];

//graphics/shadowsoftness: 0 is one filtered sample, 1 to 3 are 3x3, 5x5 and 7x7 texel filters
uniform int shadowSoftness;

//Depth of the transparent brick nearest the light, and the color light picks up passing through transparent bricks, per cascade
//Only used when coloredShadows is set, see graphics/shadowcolor
uniform sampler2DArrayShadow TintDepthArray;
uniform sampler2DArray TintColorArray;
uniform bool coloredShadows;

uniform bool debug;

//Unlit brightening on top of lighting, e.g. the ghost brick's pulse, 0 (the default) for none
uniform float glow;

//Above 0 draws nothing but this ID into the red channel, for the appearance editor's mouse picking
uniform int pickingID;

//rgb blended over the final color by a, the appearance editor's highlight on the part under the mouse, 0 (the default) for none
uniform vec4 editorHighlight;

//Start tutorial code
//https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.2.lighting_textured/1.2.pbr.fs
const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMapGrad(vec2 realUV,vec2 dx,vec2 dy)
{
    //Extra code so that if useNormal(map) is 0, texture map normal defaults to 0,1,0 aka it's just the interpolated vertex normal
	vec3 tangentNormal = vec3(0,1,0);
	if(useNormal != -1)
		tangentNormal = textureGrad(PBRArray, vec3(realUV,useNormal) ,dx,dy).xyz * 2.0 - 1.0;
	else
		return normal;
	
	mat3 TBN = mat3(
		normalize(tangent),
		normalize(bitangent),
		normalize(normal)
	);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float NdotV,float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   
//End tutorial code

uniform float test;

//Fully saturated color for a hue from 0 to 1, red at 0 and 1
vec3 hueColor(float hue)
{
	return clamp(abs(fract(hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

//Smooth noise from 0 to 1 that changes about once per unit, for foil's crinkles
float hash(vec3 p)
{
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float valueNoise(vec3 p)
{
	vec3 cell = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	return mix(mix(mix(hash(cell), hash(cell + vec3(1, 0, 0)), f.x), mix(hash(cell + vec3(0, 1, 0)), hash(cell + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(cell + vec3(0, 0, 1)), hash(cell + vec3(1, 0, 1)), f.x), mix(hash(cell + vec3(0, 1, 1)), hash(cell + vec3(1, 1, 1)), f.x), f.y), f.z);
}

//How far around the outside of the brick's box, seen from above, the nearest point on its edge is
//Starts at the min corner and goes along +x first, so each side continues into the next
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

//Per axis sample offsets (in texels) and weights for filterShadow, s is how far into its texel the position is
//level 1 to 3 is a 3x3, 5x5 or 7x7 texel filter
//Castaño's optimized PCF from The Witness, see SampleShadowMapOptimizedPCF in https://github.com/TheRealMJP/Shadows
int shadowFilterAxis(float s, int level, out vec4 weight, out vec4 offset)
{
	if(level == 1)
	{
		weight.xy = vec2(3.0 - 2.0 * s, 1.0 + 2.0 * s);
		offset.xy = vec2((2.0 - s) / weight.x - 1.0, s / weight.y + 1.0);
		return 2;
	}
	if(level == 2)
	{
		weight.xyz = vec3(4.0 - 3.0 * s, 7.0, 1.0 + 3.0 * s);
		offset.xyz = vec3((3.0 - 2.0 * s) / weight.x - 2.0, (3.0 + s) / weight.y, s / weight.z + 2.0);
		return 3;
	}
	weight = vec4(5.0 * s - 6.0, 11.0 * s - 28.0, -(11.0 * s + 17.0), -(5.0 * s + 1.0));
	offset = vec4((4.0 * s - 5.0) / weight.x - 3.0, (4.0 * s - 16.0) / weight.y - 1.0, -(7.0 * s + 5.0) / weight.z + 1.0, -s / weight.w + 3.0);
	return 4;
}

//How much of the light reaches coords (xy in the map, z its depth) in one cascade, 1 for fully lit
//Every sample is a hardware filtered 2x2 comparison, so the filter is smooth rather than a grid of hard edges
float filterShadow(sampler2DArrayShadow map, vec3 coords, int layer, int level)
{
	if(level <= 0)
		return texture(map, vec4(coords.xy, layer, coords.z));

	vec2 mapSize = vec2(textureSize(map, 0).xy);
	vec2 uv = coords.xy * mapSize;
	vec2 base = floor(uv + 0.5);
	vec2 into = uv + 0.5 - base;
	base -= 0.5;

	vec4 uWeight, uOffset, vWeight, vOffset;
	int taps = shadowFilterAxis(into.x, level, uWeight, uOffset);
	shadowFilterAxis(into.y, level, vWeight, vOffset);

	float lit = 0.0;
	float total = 0.0;
	for(int x = 0; x < taps; x++)
	{
		for(int y = 0; y < taps; y++)
		{
			float weight = uWeight[x] * vWeight[y];
			lit += weight * texture(map, vec4((base + vec2(uOffset[x], vOffset[y])) / mapSize, layer, coords.z));
			total += weight;
		}
	}

	return lit / total;
}

//World size of one texel in a cascade. The light matrices are orthographic, so their first row is a world axis scaled by 2 / cascade width
float shadowTexelWorldSize(int cascade, float mapSize)
{
	mat4 light = lightSpaceMatricies[cascade];
	return 2.0 / (length(vec3(light[0][0], light[1][0], light[2][0])) * mapSize);
}

//How much light, and of what color, reaches this surface according to one cascade, 1 for fully lit and untinted
//edgeDistance is how far inside the cascade's edges the surface is, as a fraction of its width, and negative if it's outside
vec3 cascadeLight(int cascade, vec3 surfaceNormal, float grazing, out float edgeDistance)
{
	float mapSize = float(textureSize(ShadowArray, 0).x);
	float texelWorldSize = shadowTexelWorldSize(cascade, mapSize);

	//Farther cascades have bigger texels, so they get smaller filters, keeping soft edges about as wide in the world as in the nearest one
	float filterWidth = float(shadowSoftness * 2 + 1) * shadowTexelWorldSize(0, mapSize) / texelWorldSize;
	int level = clamp(int(floor((filterWidth - 1.0) * 0.5 + 0.5)), 0, shadowSoftness);
	float reach = float(level + 1);

	//Surfaces tilted away from the light need to be lifted further off themselves before the filter's outer samples stop hitting them
	vec3 offsetPos = worldPos + surfaceNormal * texelWorldSize * (0.5 + reach * grazing);
	vec3 coords = (lightSpaceMatricies[cascade] * vec4(offsetPos, 1.0)).xyz * 0.5 + 0.5;

	vec2 inside = min(coords.xy, 1.0 - coords.xy) - reach / mapSize;
	edgeDistance = min(inside.x, inside.y);
	if(edgeDistance < 0.0)
		return vec3(1.0);

	//Casters nearer the light than a cascade are flattened onto its near plane by GL_DEPTH_CLAMP, see LoopClient::renderEverything
	coords.z = clamp(coords.z, 0.0, 1.0);
	float lit = filterShadow(ShadowArray, coords, cascade, level);
	if(!coloredShadows || lit <= 0.0)
		return vec3(lit);

	//Only the part of the filter behind a transparent brick picks up its color
	float behindTransparent = 1.0 - filterShadow(TintDepthArray, coords, cascade, level);
	vec3 tint = texture(TintColorArray, vec3(coords.xy, cascade)).rgb;
	return lit * mix(vec3(1.0), tint, behindTransparent);
}

//How wide the damp band around the edge of a shelter is, in world units, see rainExposure
const float shelterBlend = 1.8;

//Points on two rings around a spot, so averaging over them has no direction of its own
const vec2 shelterTaps[8] = vec2[8](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
	vec2(0.46, 0.46), vec2(-0.46, 0.46), vec2(0.46, -0.46), vec2(-0.46, -0.46));

//1 where rain falls freely onto a spot at this height, 0 where something is overhead
float rainOpenAt(vec2 at, float height)
{
	vec2 mapUV = (at - RainMapArea.xy) / RainMapArea.z;

	//Past the edge of the map, deep in the fog, everything counts as out in the open
	if(any(lessThan(mapUV, vec2(0.0))) || any(greaterThan(mapUV, vec2(1.0))))
		return 1.0;

	float overhead = mix(RainMapTop, RainMapBottom, textureLod(RainMap, mapUV, 0.0).r);
	return clamp((height - overhead) / 0.1 + 1.0, 0.0, 1.0);
}

//How much rain reaches this surface: 0 under cover or facing down, 1 facing up out in the open, less for walls
float rainExposure(vec3 surfaceNormal)
{
	float facing = smoothstep(-0.3, 0.1, surfaceNormal.y) * mix(0.7, 1.0, clamp(surfaceNormal.y, 0.0, 1.0));
	if(facing <= 0.0)
		return 0.0;

	//Lifted off the surface by a couple of map texels, so a wall's own top doesn't count as covering its sides
	vec3 position = worldPos + surfaceNormal * RainMapArea.w * 2.0 + vec3(0.0, 0.02, 0.0);

	//Rain blows in and splashes past the edge of a roof, so ground fades from wet to dry over a couple of studs
	//instead of tracing its outline. Walls only look straight up: blurring theirs would wet the sheltered side from around the corner
	float spread = shelterBlend * smoothstep(0.55, 0.95, surfaceNormal.y);
	if(spread <= 0.0)
		return facing * rainOpenAt(position.xz, position.y);

	float open = rainOpenAt(position.xz, position.y);
	for(int i = 0; i < 8; i++)
		open += rainOpenAt(position.xz + shelterTaps[i] * spread, position.y);

	return facing * open / 9.0;
}

//Lights placed by Lua, nearest the camera first, see PointLights::update and PointLightUniforms in ShaderSpecification.h
layout (std140) uniform PointLightUniforms
{
	int PointLightCount;
	//World size of one shadow map texel per unit of distance along a cube face's axis
	float PointShadowTexelScale;
	//xyz position, w how far the light reaches
	vec4 PointLightPositionRange[32];
	//rgb color times brightness, a the light's shadow slot, -1 for none
	vec4 PointLightColorShadow[32];
	//xyz which way a spotlight points, w cosine of half its cone angle, below -1 for lights that shine every way
	vec4 PointLightSpotDirection[32];
	//Six cube faces per shadow slot: +x, -x, +y, -y, +z, -z
	mat4 PointShadowMatrices[48];
};

//Six layers per shadow slot, in the same order as PointShadowMatrices
uniform sampler2DArrayShadow PointShadowArray;

//Same as TintDepthArray and TintColorArray, a layer per cube face like PointShadowArray, only used when coloredShadows is set
uniform sampler2DArrayShadow PointTintDepthArray;
uniform sampler2DArray PointTintColorArray;

//How much of a shadowed point light, and of what color, reaches this surface, 1 for fully lit and untinted
vec3 pointLightShadow(int slot, vec3 fromLight, vec3 surfaceNormal, float lightFacing)
{
	vec3 axisDistance = abs(fromLight);
	int face;
	if(axisDistance.x >= axisDistance.y && axisDistance.x >= axisDistance.z)
		face = fromLight.x > 0.0 ? 0 : 1;
	else if(axisDistance.y >= axisDistance.z)
		face = fromLight.y > 0.0 ? 2 : 3;
	else
		face = fromLight.z > 0.0 ? 4 : 5;
	int layer = slot * 6 + face;

	//Texels get bigger farther from the light, so farther surfaces are lifted further off themselves
	float texelWorldSize = PointShadowTexelScale * max(axisDistance.x, max(axisDistance.y, axisDistance.z));
	int level = min(shadowSoftness, 1);
	float grazing = sqrt(1.0 - lightFacing * lightFacing);
	vec3 offsetPos = worldPos + surfaceNormal * texelWorldSize * (0.5 + float(level + 1) * grazing);

	vec4 lightClip = PointShadowMatrices[layer] * vec4(offsetPos, 1.0);
	vec3 coords = lightClip.xyz / lightClip.w * 0.5 + 0.5;
	coords.z = clamp(coords.z, 0.0, 1.0);
	float lit = filterShadow(PointShadowArray, coords, layer, level);
	if(!coloredShadows || lit <= 0.0)
		return vec3(lit);

	//One hardware filtered sample, the tint map is half resolution and already soft
	float behindTransparent = 1.0 - texture(PointTintDepthArray, vec4(coords.xy, layer, coords.z));
	vec3 tint = texture(PointTintColorArray, vec3(coords.xy, layer)).rgb;
	return lit * mix(vec3(1.0), tint, behindTransparent);
}

//Skies from Lua's setSkybox, see SkyUniforms in ShaderSpecification.h
layout (std140) uniform SkyUniforms
{
	//Day's 9 irradiance coefficients then night's, see Skybox::Slot::irradiance
	vec4 SkyIrradiance[18];
	float SkyboxBlend;
	int DaySkybox;
	int NightSkybox;
	//How much each sky lights the world, 0 unless it's a .hdr and graphics/imagebasedlighting is on
	float SkyLightDay;
	float SkyLightNight;
	//Mip level of the roughest reflections
	float SkyReflectionLevels;
};

//Full size sky, with blurrier reflections in each mip level after the first, see Skybox::prefilter
uniform samplerCube SkyDay;
uniform samplerCube SkyNight;

//Light reaching a surface facing n from one sky, first is 0 for day and 9 for night
//Keep in sync with basis in Skybox::loadSlot
vec3 skyIrradiance(int first, vec3 n)
{
	vec3 irradiance = SkyIrradiance[first].xyz
		+ SkyIrradiance[first + 1].xyz * n.y
		+ SkyIrradiance[first + 2].xyz * n.z
		+ SkyIrradiance[first + 3].xyz * n.x
		+ SkyIrradiance[first + 4].xyz * (n.x * n.y)
		+ SkyIrradiance[first + 5].xyz * (n.y * n.z)
		+ SkyIrradiance[first + 6].xyz * (3.0 * n.z * n.z - 1.0)
		+ SkyIrradiance[first + 7].xyz * (n.x * n.z)
		+ SkyIrradiance[first + 8].xyz * (n.x * n.x - n.y * n.y);
	return max(irradiance, vec3(0.0));
}

//The split sum approximation's second half without a lookup texture, see Karis, Physically Based Shading on Mobile
vec2 environmentBRDF(float NdotV, float roughness)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, pow(2.0, -9.28 * NdotV)) * r.x + r.y;
	return vec2(-1.04, 1.04) * a004 + r.zw;
}

//Light from every point light that reaches this surface: inverse square falloff, eased to exactly nothing at each light's range
vec3 pointLighting(vec3 N, vec3 V, float NdotV, vec3 albedo, vec3 mor, vec3 F0, vec3 surfaceNormal)
{
	vec3 total = vec3(0.0);
	for(int i = 0; i < PointLightCount; i++)
	{
		vec3 toLight = PointLightPositionRange[i].xyz - worldPos;
		float distanceSquared = dot(toLight, toLight);
		float range = PointLightPositionRange[i].w;
		if(distanceSquared >= range * range)
			continue;

		float lightDistance = sqrt(distanceSquared);
		vec3 L = toLight / max(lightDistance, 0.0001);
		float NdotL = max(dot(N, L), 0.0);
		float lightFacing = dot(surfaceNormal, L);
		if(NdotL <= 0.0 || lightFacing <= 0.0)
			continue;

		float edge = distanceSquared / (range * range);
		float window = clamp(1.0 - edge * edge, 0.0, 1.0);
		float attenuation = window * window / (distanceSquared + 1.0);

		//Spotlights fade out over the outer part of their cone, see spotFactor in PointLights.cpp
		float spotCosine = PointLightSpotDirection[i].w;
		if(spotCosine > -1.5)
		{
			attenuation *= smoothstep(spotCosine, spotCosine + (1.0 - spotCosine) * 0.25, dot(-L, PointLightSpotDirection[i].xyz));
			if(attenuation <= 0.0)
				continue;
		}

		vec3 lit = vec3(1.0);
		int slot = int(floor(PointLightColorShadow[i].a + 0.5));
		if(slot >= 0)
		{
			lit = pointLightShadow(slot, -toLight, surfaceNormal, lightFacing);
			if(max(lit.r, max(lit.g, lit.b)) <= 0.0)
				continue;
		}

		vec3 H = normalize(V + L);
		float NDF = DistributionGGX(N, H, mor.b);
		float G = GeometrySmith(NdotV, NdotL, mor.b);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
		vec3 specular = NDF * G * F / (4.0 * NdotV * NdotL + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - mor.r);

		total += (kD * albedo / PI + specular) * PointLightColorShadow[i].rgb * attenuation * NdotL * lit;
	}
	return total;
}

void main()
{			
	vec2 dxuv = dFdx(uvs);
	vec2 dyuv = dFdy(uvs);

	//A decal only covers the part of the mesh's texture coordinates in DecalArea, like the front of a torso
	vec2 decalUvs = (uvs - DecalArea.xy) / (DecalArea.zw - DecalArea.xy);
	vec2 dxDecal = dFdx(decalUvs);
	vec2 dyDecal = dFdy(decalUvs);
	int decal = all(greaterThanEqual(decalUvs, vec2(0.0))) && all(lessThanEqual(decalUvs, vec2(1.0))) ? useDecal : -1;

	//Before picking, so clicking the see-through part of a face plate picks whatever is behind it
	if(decalCutout != 0 && (decal == -1 || textureGrad(DecalArray,vec3(decalUvs,decal),dxDecal,dyDecal).a < 0.5))
		discard;

	//See AppearanceEditor::renderPreview
	if(pickingID > 0)
	{
		color = vec4(float(pickingID) / 255.0, 0.0, 0.0, 1.0);
		return;
	}

	//Bars of nothing that walk around the brick's sides, and over its top and bottom toward the middle
	if(material == MaterialHologram)
	{
		float perimeter = 4.0 * (max(brickBoxSize.x, 0.001) + max(brickBoxSize.z, 0.001)) * 0.5;
		float bars = max(1.0, floor(perimeter / hologramBarSpacing + 0.5));
		if(fract(distanceAroundBrick() / perimeter * bars - WaveTime) < hologramGap)
			discard;
	}

	vec3 viewVector = normalize(CameraPosition - worldPos);
	
	vec4 albedo_ = vec4(1,1,1,1);
	if(useAlbedo != -1)
		albedo_ = textureGrad(PBRArray,vec3(uvs,useAlbedo),dxuv,dyuv);
		

	float nonLinearAlbedoF = 1.0;											
	vec3 albedo = pow(albedo_.rgb,vec3(1.0 + 1.2 * nonLinearAlbedoF));
	albedo = mix(albedo.rgb,preColor.rgb,preColor.a);

	/*
		What the surface is painted, in the same space as the color that ends up drawn rather than the
		linear one lighting works in. Glow floors the drawn color at this, see MaterialGlow below
	*/
	vec3 unlitColor = preColor.rgb;

	//On top of the mesh's color, so a painted head still shows its face
	if(decal != -1)
	{
		vec4 decalAlbedo = textureGrad(DecalArray,vec3(decalUvs,decal),dxDecal,dyDecal);
		albedo = mix(albedo, pow(decalAlbedo.rgb,vec3(1.0 + 1.2 * nonLinearAlbedoF)), decalAlbedo.a);
		unlitColor = mix(unlitColor, decalAlbedo.rgb, decalAlbedo.a);
	}
	
	//Half paint, half a hue that cycles every 5 seconds (a whole number of times per WaveTime's 100 seconds),
	//shifted along a diagonal so the colors flow across a build in bands about 20 units apart
	if(material == MaterialRainbow)
	{
		float hue = fract(WaveTime * 0.2 - dot(worldPos, vec3(0.03, 0.02, 0.03)));
		albedo = mix(albedo, pow(hueColor(hue), vec3(1.0 + 1.2 * nonLinearAlbedoF)), rainbowStrength);
	}

	vec3 newNormal = getNormalFromMapGrad(uvs,dxuv,dyuv);

	//Perfectly smooth, none of the texture's bumps
	if(material == MaterialSlippery)
		newNormal = normalize(normal);

	//Crinkles tilt the surface a little in a pattern fixed in place
	if(material == MaterialFoil)
	{
		vec3 crinkle = vec3(valueNoise(worldPos * 3.0), valueNoise(worldPos * 3.0 + 17.0), valueNoise(worldPos * 3.0 + 31.0)) - 0.5;
		newNormal = normalize(newNormal + crinkle * foilCrinkle);
	}

	//Sun during the day, moon at night
	vec3 sunDirection = LightDirection;
	vec3 sunColor = LightColor;
	
	//Faces turned away from the light get no direct light anyway, sampling the shadow map there just adds acne
	vec3 sunlight = vec3(0.0);
	vec3 surfaceNormal = normalize(normal);
	float lightFacing = dot(surfaceNormal, sunDirection);
	if(lightFacing > 0.0)
	{
		float grazing = sqrt(1.0 - lightFacing * lightFacing);

		//Share of a cascade's width, along its edges, spent fading into the next cascade so the change in detail isn't a visible line
		const float blendBand = 0.1;

		vec3 lit = vec3(1.0);
		for(int i = 0; i<3; i++)
		{
			float edgeDistance;
			vec3 cascadeLit = cascadeLight(i, surfaceNormal, grazing, edgeDistance);
			if(edgeDistance < 0.0)
				continue;

			lit = cascadeLit;
			if(edgeDistance < blendBand)
			{
				//Nothing past the last cascade is shadowed, though that's only ever deep in the fog
				vec3 nextLit = vec3(1.0);
				if(i < 2)
				{
					float nextEdgeDistance;
					nextLit = cascadeLight(i + 1, surfaceNormal, grazing, nextEdgeDistance);
				}
				lit = mix(nextLit, cascadeLit, smoothstep(0.0, blendBand, edgeDistance));
			}
			break;
		}

		sunlight = lit;
	}

	//color = vec4(uvs.x,uvs.y,0,1);
	//color = vec4(normal,1);
	//return;
	
	vec3 mor = vec3(0,0,0.5);
	
	int morLayer = max(max(useMetalness,useRoughness),useAO);
		
	if(morLayer != -1)
		mor = textureGrad(PBRArray,vec3(uvs,morLayer),dxuv,dyuv).rgb;
	
	if(useMetalness == -1)
		mor.r = 0;
	if(useAO == -1)
		mor.g = 1;
	if(useRoughness == -1)
		mor.b = 0.5;

	//Both replace the brick texture's scuffed plastic roughness, which is far too rough to reflect anything recognizable
	if(material == MaterialPearl)
	{
		mor.r = 0.8;
		mor.b = 0.2;
	}
	else if(material == MaterialChrome)
	{
		mor.r = 0.95;
		mor.b = 0.08;
	}
	else if(material == MaterialSlippery)
	{
		//Not quite 0, which would shrink the sun's highlight to nothing
		mor.b = 0.05;
	}
	else if(material == MaterialFoil)
	{
		//The rainbow's hue shifts with how squarely and from which way the surface is seen, and across the crinkles
		float hue = fract(dot(newNormal, viewVector) * 1.5 + dot(viewVector, vec3(0.35, 0.6, 0.25)) + valueNoise(worldPos * 1.5) * 0.5);
		albedo = mix(albedo, pow(hueColor(hue), vec3(2.2)), foilStrength);
		mor.r = 0.9;
		mor.b = min(mor.b, 0.3);
	}

	//Rain darkens and smooths what it reaches, water filling in the texture's bumps, tops of things more than walls
	float wet = 0.0;
	//How much of that is standing water rather than damp: glassier, and it hides the texture almost completely
	float puddle = 0.0;
	if(RainWetness > 0.0 && pickingID <= 0)
	{
		//Water pools unevenly on anything near level, while walls shed it evenly. Fixed in place, so patches don't swim as the camera moves
		float level = smoothstep(0.55, 0.95, surfaceNormal.y);
		vec3 groundSpot = vec3(worldPos.x, 0.0, worldPos.z);
		float hollow = valueNoise(groundSpot * 0.15) * 0.7 + valueNoise(groundSpot * 0.55) * 0.3;

		//The lowest spots soak first and are the last to dry, so a shower starts patchy and puddles linger after it stops
		float soaked = smoothstep(hollow - 0.3, hollow + 0.3, RainWetness);
		wet = rainExposure(surfaceNormal) * mix(RainWetness, soaked, level);
		puddle = level * wet * smoothstep(0.55, 0.15, hollow);

		albedo *= 1.0 - 0.45 * wet;
		mor.b = mix(mor.b, min(mor.b, mix(0.3, 0.08, clamp(surfaceNormal.y, 0.0, 1.0))), wet);
		mor.b = mix(mor.b, 0.03, puddle * 0.7);
		newNormal = normalize(mix(newNormal, surfaceNormal, max(wet * 0.45, puddle * 0.85)));
	}

	float NdotV = max(dot(newNormal, viewVector), 0.0);	
	vec3 halfVector = normalize(viewVector + sunDirection);     
	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, mor.r);
    vec3 F = fresnelSchlickRoughness(NdotV, F0, mor.b);
	vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - mor.r;
	
	float NdotL = max(dot(normalize(newNormal), normalize(sunDirection)), 0.0);  
	float NDF = DistributionGGX(newNormal, halfVector, mor.b);   
	float G   = GeometrySmith(NdotV,NdotL,mor.b);   
	
	vec3 numerator    = NDF * G * F; 
	float denominator = 4 * NdotV * NdotL + 0.001; // 0.001 to prevent divide by zero
	vec3 specular = numerator / denominator;
	
	vec3 shadowLight = clamp(sunlight, 0.35, 1.0);
	color.rgb = (kD * albedo / PI + specular) * sunColor.rgb * NdotL * shadowLight;
	//Ambient stays on while the direct light fades out at the horizon, and stops being shadowed there too, since the
	//shadow maps are about to switch between the sun and moon
	vec3 ambientShadow = mix(vec3(1.0), shadowLight, ShadowStrength);

	//A .hdr sky lights the world in place of the ambient color: diffuse light from all over it and blurred reflections of it
	float skyLight = SkyLightDay + SkyLightNight;
	color.rgb += mor.g * albedo * AmbientColor * ambientShadow * (1.0 - skyLight);
	if(skyLight > 0.0)
	{
		vec3 irradiance = SkyLightDay * skyIrradiance(0, newNormal) + SkyLightNight * skyIrradiance(9, newNormal);
		vec3 reflectedRay = reflect(-viewVector, newNormal);
		float reflectionLevel = mor.b * SkyReflectionLevels;
		vec3 reflected = SkyLightDay * textureLod(SkyDay, reflectedRay, reflectionLevel).rgb + SkyLightNight * textureLod(SkyNight, reflectedRay, reflectionLevel).rgb;
		vec2 reflectance = environmentBRDF(NdotV, mor.b);
		color.rgb += (kD * albedo * irradiance / PI + reflected * (F * reflectance.x + reflectance.y)) * mor.g * ambientShadow;
	}
	color.rgb += pointLighting(newNormal, viewVector, NdotV, albedo, mor, F0, surfaceNormal);

	//Without a .hdr sky there are no reflections, so wet surfaces reflect the sky's gradient instead to still look shiny
	if(wet > 0.0 && skyLight < 1.0)
	{
		vec3 reflectedRay = reflect(-viewVector, newNormal);
		vec3 skyReflection = pow(mix(FogColor, SkyColor, smoothstep(0.0, 0.6, reflectedRay.y)), vec3(2.2));
		vec3 wetFresnel = fresnelSchlickRoughness(NdotV, F0, mor.b);
		color.rgb += skyReflection * wetFresnel * wet * mor.g * (1.0 - skyLight) * 0.5;
	}

	color.a = opacity;

	//Tone maping
	color.rgb = color.rgb / (color.rgb + vec3(1.0));
	//Gamma correction
	color.rgb = pow(color.rgb, vec3(1.0/2.2));

	//However dark it is, never drawn darker than its own color, its print included so glow doesn't wash one away
	if(material == MaterialGlow)
		color.rgb = max(color.rgb, unlitColor);

	//After tone mapping, which would otherwise squash the glow to almost nothing on bright or sunlit surfaces
	color.rgb = mix(color.rgb, vec3(1.0), glow);

	//Same pulse as the part under the mouse in the appearance editor, see AppearanceEditor::renderPreview
	//Once a second, which lines up with WaveTime starting over every 100 seconds
	if(material == MaterialBlink)
	{
		float pulse = 0.5 + 0.5 * sin(WaveTime * 6.2831853);
		vec3 highlight = dot(preColor.rgb, vec3(0.299, 0.587, 0.114)) > 0.7 ? vec3(0.25, 0.5, 1.0) : vec3(1.0);
		color.rgb = mix(color.rgb, highlight, 0.15 + 0.15 * pulse);
	}
	color.rgb = mix(color.rgb, editorHighlight.rgb, editorHighlight.a);

	float fogFactor = clamp((length(CameraPosition - worldPos) - FogDistanceMin) / (FogDistanceMax - FogDistanceMin), 0.0, 1.0);
	color.rgb = mix(color.rgb, FogColor, fogFactor);
}


 



















