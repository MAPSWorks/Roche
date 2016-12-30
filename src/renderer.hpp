#pragma once

#include "planet.h"
#include <glm/glm.hpp>

class Renderer
{
public:
	virtual void windowHints() = 0;
	virtual void init(
		std::vector<PlanetParameters> planetParams, 
		SkyboxParameters skyboxParam,
		int msaa,
		int windowWidth,
		int windowHeight) = 0;
	virtual void render(
		glm::dvec3 viewPos, 
		float fovy,
		glm::dvec3 viewCenter,
		glm::vec3 viewUp,
		float gamma,
		std::vector<PlanetState> planetStates) = 0;
	
	virtual void destroy() = 0;
};