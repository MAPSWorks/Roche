#pragma once

#include "planet.h"
#include <glm/glm.hpp>

class Renderer
{
public:
	void setGamma(float gamma) { this->gamma = gamma; }
	virtual void init(std::vector<PlanetParameters> planetParams, SkyboxParameters skyboxParam) = 0;
	virtual void render(
		glm::dvec3 viewPos, 
		glm::mat4 projMat, 
		glm::mat4 viewMat,
		std::vector<PlanetState> planetStates) = 0;
	
	virtual void destroy() = 0;

protected:
	float gamma;
};