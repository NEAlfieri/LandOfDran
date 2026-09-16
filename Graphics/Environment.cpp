#include "Environment.h"

#include <glm/gtc/constants.hpp>

void Environment::calc(double worldTimeSeconds)
{
	fogDistanceMin = cycle.fogStart;
	fogDistanceMax = cycle.fogEnd;
	fogHeight = cycle.fogHeight;

	dayFraction = (float)(fmod(worldTimeSeconds, DAY_LENGTH_SECONDS) / DAY_LENGTH_SECONDS);
	if (dayFraction < 0)
		dayFraction += 1.0f;

	//Rises in +X and sets in -X, tilted toward +Z so it's never directly overhead
	const float tilt = glm::radians(20.0f);
	float angle = (dayFraction - 0.25f) * glm::two_pi<float>();
	sunDirection = glm::normalize(glm::vec3(cos(angle), sin(angle) * cos(tilt), sin(angle) * sin(tilt)));

	float elevation = sunDirection.y;
	skyboxBlend = 1.0f - glm::smoothstep(-0.15f, 0.1f, elevation);
	const SkyKeyframe& night = cycle.phases[NightPhase];
	const SkyKeyframe& day = cycle.phases[DaytimePhase];
	const SkyKeyframe& twilight = dayFraction < 0.5f ? cycle.phases[DawnPhase] : cycle.phases[DuskPhase];

	auto blend = [](const SkyKeyframe& a, const SkyKeyframe& b, float t) -> SkyKeyframe
	{
		return { glm::mix(a.skyColor, b.skyColor, t), glm::mix(a.fogColor, b.fogColor, t), glm::mix(a.lightColor, b.lightColor, t), glm::mix(a.ambientColor, b.ambientColor, t) };
	};

	SkyKeyframe current;
	if (elevation < 0.05f)
		current = blend(night, twilight, glm::smoothstep(-0.25f, 0.05f, elevation));
	else
		current = blend(twilight, day, glm::smoothstep(0.05f, 0.35f, elevation));

	skyColor = current.skyColor;
	fogColor = current.fogColor;
	ambientColor = current.ambientColor;

	//Direct light fades to nothing right at the horizon so switching between sun and moon doesn't pop
	if (elevation >= 0)
	{
		lightDirection = sunDirection;
		shadowStrength = glm::smoothstep(0.0f, 0.08f, elevation);
		lightColor = current.lightColor * shadowStrength;
	}
	else
	{
		lightDirection = -sunDirection;
		shadowStrength = glm::smoothstep(0.0f, 0.08f, -elevation);
		lightColor = night.lightColor * shadowStrength;
	}
}

void Environment::passUniforms(std::shared_ptr<ShaderManager> shaders) const
{
	EnvironmentUniforms& uniforms = shaders->environmentUniforms;
	uniforms.SunDirection = sunDirection;
	uniforms.LightDirection = lightDirection;
	uniforms.LightColor = lightColor;
	uniforms.SkyColor = skyColor;
	uniforms.FogColor = fogColor;
	uniforms.FogDistanceMin = fogDistanceMin;
	uniforms.FogDistanceMax = fogDistanceMax;
	uniforms.FogHeight = fogHeight;
	uniforms.AmbientColor = ambientColor;
	uniforms.ShadowStrength = shadowStrength;
}

Environment::Environment()
{
	calc(DAY_LENGTH_SECONDS * 0.5);
}
