#include "PointLights.h"

#include <glm/gtc/constants.hpp>

//Nearest a caster can be to a light and still cast a shadow
static constexpr float shadowNearPlane = 0.1f;

//Texels past the 90 degrees each cube face has to cover, so shadow filters near a face's edge don't run off it
static constexpr float facePaddingTexels = 3.0f;

//Most coronae drawn at once
static constexpr int maxCoronae = 1024;

//Cube faces in the order model.frag picks them: +x, -x, +y, -y, +z, -z
static const glm::vec3 faceDirections[6] = { glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1) };
static const glm::vec3 faceUps[6] = { glm::vec3(0, 1, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0) };

//Tangent of half a cube face's field of view, a little over 45 degrees to fit the padding
static float faceTanHalf(int resolution)
{
	return resolution / (resolution - 2.0f * facePaddingTexels);
}

//How much of a spotlight reaches something at this cosine from its direction, with a soft edge, same as model.frag's pointLighting
static float spotFactor(float spotCosine, float cosine)
{
	if (spotCosine < -1.5f)
		return 1.0f;

	return glm::smoothstep(spotCosine, spotCosine + (1.0f - spotCosine) * 0.25f, cosine);
}

void PointLights::setShadowSettings(int count, int resolution, bool colored, std::shared_ptr<TextureManager> textures)
{
	count = std::clamp(count, 0, PointLightUniforms::maxShadowed);
	resolution = std::max(resolution, 16);
	if (shadowMaps && tintMaps && count == shadowCount && resolution == faceResolution && colored == coloredShadows)
		return;

	shadowCount = count;
	faceResolution = resolution;
	coloredShadows = colored;

	//Half resolution to save memory like the sun's, but still one layer per face either way, since a single layer wouldn't be an array texture
	RenderTarget::RenderTargetSettings tintSettings;
	tintSettings.width = colored && count > 0 ? std::max(resolution / 2, 1) : 1;
	tintSettings.height = tintSettings.width;
	tintSettings.layers = std::max(count, 1) * 6;
	tintSettings.channels = 3;
	tintSettings.depthCompare = true;
	//Untinted light, which each transparent brick then takes its color out of
	tintSettings.clearColor = glm::vec4(1);
	tintMaps.reset();
	tintMaps = std::make_shared<RenderTarget>(tintSettings, textures);

	RenderTarget::RenderTargetSettings settings;
	//With no shadowed lights there's still a tiny array bound, so model.frag's sampler always has a texture of the right type
	settings.width = count > 0 ? resolution : 1;
	settings.height = settings.width;
	settings.layers = std::max(count, 1) * 6;
	settings.useColor = false;
	settings.depthCompare = true;
	shadowMaps.reset();
	shadowMaps = std::make_shared<RenderTarget>(settings, textures);

	for (ShadowSlot& slot : slots)
		slot = ShadowSlot();
}

void PointLights::update(const std::vector<PointLightSource>& lights, std::shared_ptr<ShaderManager> shaders, const glm::vec3& cameraPosition, const glm::mat4& viewProjection, float fogEnd)
{
	//Gribb-Hartmann, normalized so a plane's distance comes out in world units
	glm::vec4 rows[4];
	for (int row = 0; row < 4; row++)
		rows[row] = glm::vec4(viewProjection[0][row], viewProjection[1][row], viewProjection[2][row], viewProjection[3][row]);
	glm::vec4 planes[6] = { rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1], rows[3] - rows[1], rows[3] + rows[2], rows[3] - rows[2] };
	for (glm::vec4& plane : planes)
		plane /= glm::length(glm::vec3(plane));

	struct Candidate
	{
		const PointLightSource* light;
		float distance;
	};
	std::vector<Candidate> candidates;
	coronaInstances.clear();

	for (const PointLightSource& light : lights)
	{
		float reach = std::max(light.range, light.coronaWidth * 0.5f);
		if (reach <= 0)
			continue;

		//Hidden in the fog, or can't reach anything in view
		float distance = glm::length(light.position - cameraPosition);
		if (distance - reach > fogEnd)
			continue;

		bool inView = true;
		for (const glm::vec4& plane : planes)
		{
			if (glm::dot(glm::vec3(plane), light.position) + plane.w < -reach)
			{
				inView = false;
				break;
			}
		}
		if (!inView)
			continue;

		//A glow drawn right where the camera is has no shape to it and would just wash the view out, which is what a
		//light that follows a camera around (the free camera's, see ClientData::setFreeCamera) would otherwise do
		bool coronaAtCamera = distance < 1.0f;

		if (light.coronaWidth > 0 && light.brightness > 0 && !coronaAtCamera && coronaInstances.size() < maxCoronae * 8)
		{
			//The corona is always as bright as its brightest channel allows, dimmer lights just get fainter ones
			float strongest = std::max(light.color.r, std::max(light.color.g, light.color.b));
			glm::vec3 glowColor = strongest > 0 ? light.color / strongest : glm::vec3(0);
			float strength = strongest * (1.0f - std::exp(-light.brightness / 20.0f));

			//A spotlight's glow is only seen from inside its beam
			glm::vec3 toCamera = distance > 0.001f ? (cameraPosition - light.position) / distance : light.direction;
			strength *= spotFactor(light.spotCosine, glm::dot(toCamera, light.direction));

			if (strength > 0)
				coronaInstances.insert(coronaInstances.end(), { light.position.x, light.position.y, light.position.z, light.coronaWidth, glowColor.r, glowColor.g, glowColor.b, strength });
		}

		if (light.range > 0)
			candidates.push_back({ &light, distance });
	}

	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
	if (candidates.size() > (size_t)PointLightUniforms::maxLit)
		candidates.resize(PointLightUniforms::maxLit);

	//Lights keep their slot for as long as they stay among the nearest, so their shadows don't have to be redrawn
	int shadowed = std::min((int)candidates.size(), shadowCount);
	int slotOf[PointLightUniforms::maxLit];
	bool slotTaken[PointLightUniforms::maxShadowed] = {};
	for (int c = 0; c < (int)candidates.size(); c++)
	{
		slotOf[c] = -1;
		if (c >= shadowed)
			continue;

		for (int s = 0; s < shadowCount; s++)
		{
			if (slots[s].lightID == candidates[c].light->id)
			{
				slotOf[c] = s;
				slotTaken[s] = true;
				break;
			}
		}
	}

	for (int s = 0; s < shadowCount; s++)
	{
		if (!slotTaken[s])
			slots[s] = ShadowSlot();
	}

	for (int c = 0; c < shadowed; c++)
	{
		if (slotOf[c] != -1)
			continue;

		for (int s = 0; s < shadowCount; s++)
		{
			if (!slotTaken[s])
			{
				slotTaken[s] = true;
				slotOf[c] = s;
				slots[s].lightID = candidates[c].light->id;
				break;
			}
		}
	}

	PointLightUniforms& uniforms = shaders->pointLightUniforms;
	int resolution = std::max(faceResolution, 16);
	float tanHalf = faceTanHalf(resolution);
	uniforms.PointLightCount = (GLint)candidates.size();
	uniforms.PointShadowTexelScale = 2.0f * tanHalf / resolution;

	//Widest angle from a face's axis that it covers, out at its corners
	float faceReach = std::atan(tanHalf * std::sqrt(2.0f)) + 0.02f;

	for (int c = 0; c < (int)candidates.size(); c++)
	{
		const PointLightSource& light = *candidates[c].light;
		uniforms.PointLightPositionRange[c] = glm::vec4(light.position, light.range);
		uniforms.PointLightColorShadow[c] = glm::vec4(light.color * light.brightness, (float)slotOf[c]);
		uniforms.PointLightSpotDirection[c] = glm::vec4(light.direction, light.spotCosine);

		if (slotOf[c] == -1)
			continue;

		ShadowSlot& slot = slots[slotOf[c]];
		slot.position = light.position;
		slot.range = light.range;
		slot.direction = light.direction;
		slot.spotCosine = light.spotCosine;

		//Anything in the beam is within its half angle of the direction, and lands on a face whose axis is within faceReach of it
		float halfAngle = light.spotCosine < -1.5f ? glm::pi<float>() : std::acos(std::clamp(light.spotCosine, -1.0f, 1.0f));

		glm::mat4 projection = glm::perspective(2.0f * std::atan(tanHalf), 1.0f, shadowNearPlane, std::max(light.range, shadowNearPlane * 2.0f));
		for (int face = 0; face < 6; face++)
		{
			slot.faces[face] = projection * glm::lookAt(slot.position, slot.position + faceDirections[face], faceUps[face]);
			uniforms.PointShadowMatrices[slotOf[c] * 6 + face] = slot.faces[face];

			float faceAngle = std::acos(std::clamp(glm::dot(faceDirections[face], light.direction), -1.0f, 1.0f));
			slot.faceNeeded[face] = faceAngle <= halfAngle + faceReach;
		}
	}

	shaders->updatePointLightUBO();

	litCount = (int)candidates.size();
	shadowedCount = shadowed;

	coronaCount = (GLsizei)(coronaInstances.size() / 8);
	glBindBuffer(GL_ARRAY_BUFFER, coronaBuffer);
	glBufferData(GL_ARRAY_BUFFER, coronaInstances.size() * sizeof(float), coronaInstances.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void PointLights::renderShadows(unsigned int sceneGeneration, bool tint, const std::function<bool(const glm::vec3& position, float range)>& movingCastersNear,
	const std::function<void(const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPosition, bool tinted)>& drawCasters,
	const std::function<void(const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPosition)>& drawTint)
{
	facesDrawn = 0;
	tint = tint && coloredShadows;
	facesTinted = tint;

	for (int s = 0; s < shadowCount; s++)
	{
		ShadowSlot& slot = slots[s];
		if (slot.lightID == NO_ID)
			continue;

		//Something moving out of range still has to have its old shadow cleared, hence checking the last result too
		bool movingNear = movingCastersNear(slot.position, slot.range);

		//Only a spotlight's direction changes which faces are drawn
		bool turned = slot.spotCosine != slot.renderedSpotCosine || (slot.spotCosine > -1.5f && slot.direction != slot.renderedDirection);

		bool stale = !slot.rendered || slot.renderedPosition != slot.position || slot.renderedRange != slot.range || turned ||
			slot.renderedSceneGeneration != sceneGeneration || movingNear || slot.movingCastersNear || slot.renderedTint != tint;
		slot.movingCastersNear = movingNear;

		if (!stale)
			continue;

		slot.rendered = true;
		slot.renderedPosition = slot.position;
		slot.renderedRange = slot.range;
		slot.renderedDirection = slot.direction;
		slot.renderedSpotCosine = slot.spotCosine;
		slot.renderedSceneGeneration = sceneGeneration;
		slot.renderedTint = tint;

		for (int face = 0; face < 6; face++)
		{
			if (!slot.faceNeeded[face])
				continue;

			shadowMaps->useLayer(s * 6 + face);
			drawCasters(slot.faces[face], slot.position, tint);
			facesDrawn++;

			if (tint)
			{
				tintMaps->useLayer(s * 6 + face);
				drawTint(slot.faces[face], slot.position);
			}
		}
	}
}

void PointLights::bindShadowMaps() const
{
	shadowMaps->bindDepthResult(PointShadowArray);
	tintMaps->bindDepthResult(PointTintDepthArray);
	tintMaps->bindColorResult(PointTintColorArray);
}

void PointLights::renderCoronae(std::shared_ptr<ShaderManager> shaders) const
{
	if (coronaCount < 1)
		return;

	shaders->coronaShader->use();

	//Billboards face the camera, but the mirrored water reflection camera flips their winding
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindVertexArray(coronaVao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, coronaCount);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
}

std::string PointLights::getStats() const
{
	return std::to_string(litCount) + " lit, " + std::to_string(shadowedCount) + " shadowed, " + std::to_string(facesDrawn) + (facesTinted ? " tinted" : "") + " shadow faces drawn";
}

PointLights::PointLights()
{
	//corona.vert makes each quad's corners from gl_VertexID, so the only attributes are per corona
	glGenVertexArrays(1, &coronaVao);
	glGenBuffers(1, &coronaBuffer);
	glBindVertexArray(coronaVao);
	glBindBuffer(GL_ARRAY_BUFFER, coronaBuffer);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	for (GLuint attribute = 0; attribute < 2; attribute++)
	{
		glEnableVertexAttribArray(attribute);
		glVertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(attribute * 4 * sizeof(float)));
		glVertexAttribDivisor(attribute, 1);
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

PointLights::~PointLights()
{
	shadowMaps.reset();
	glDeleteVertexArrays(1, &coronaVao);
	glDeleteBuffers(1, &coronaBuffer);
}
