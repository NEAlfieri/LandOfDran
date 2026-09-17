#pragma once

#include "../LandOfDran.h"
#include "ShaderSpecification.h"
#include "DayCycle.h"

/*
	Client only: turns the server's world clock into sun position, lighting, sky, and fog colors
	These reach shaders through EnvironmentUniforms, see ShaderSpecification.h
*/
class Environment
{
	public:

	//Colors for each part of the day and the fog distances, as last sent by the server
	DayCycle cycle;

	//0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
	float dayFraction = 0.5;

	//Where the sun actually is, below the horizon at night
	glm::vec3 sunDirection = glm::vec3(0, 1, 0);

	//The sun during the day and the moon (opposite the sun) at night, used for lighting and shadows
	glm::vec3 lightDirection = glm::vec3(0, 1, 0);
	glm::vec3 lightColor = glm::vec3(1, 1, 1);

	//Light from the sky itself, doesn't fade out when the sun or moon crosses the horizon
	glm::vec3 ambientColor = glm::vec3(0, 0, 0);

	//Fades to 0 as the sun or moon reaches the horizon, along with lightColor
	float shadowStrength = 1;

	//Sky color straight up, fades to fogColor at the horizon
	glm::vec3 skyColor = glm::vec3(0, 0, 1);
	glm::vec3 fogColor = glm::vec3(1, 1, 1);

	//Copied from cycle by calc. Grass and water stretch out a little past fogDistanceMax so fog always hides their edges
	float fogDistanceMin = 150;
	float fogDistanceMax = 290;

	//Copied from cycle by calc, world height the fog reaches up to, see DayCycle::fogHeight and sky.frag's skyFogAmount
	float fogHeight = 40;

	//0 shows the day skybox and 1 the night one, they cross fade while the sun is near the horizon, see Skybox
	float skyboxBlend = 0;

	void calc(double worldTimeSeconds);

	//Copies the results of calc into the environment UBO struct, does not upload it
	void passUniforms(std::shared_ptr<ShaderManager> shaders) const;

	Environment();
};
