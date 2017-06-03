#pragma once

#include "planet.hpp"
#include <glm/glm.hpp>
#include <map>
#include <string>

class Renderer
{
public:
	virtual ~Renderer() {}
	virtual void windowHints() {}
	virtual void init(
		std::vector<PlanetParameters> planetParams, 
		int msaa,
		int maxTexSize,
		int windowWidth,
		int windowHeight) {}
	virtual void render(
		glm::dvec3 viewPos, 
		float fovy,
		glm::dvec3 viewCenter,
		glm::vec3 viewUp,
		float exposure,
		float ambientColor,
		std::vector<PlanetState> planetStates) {}
	virtual void takeScreenshot(const std::string &filename) {}
	virtual void destroy() {}
	
	virtual std::vector<std::pair<std::string,uint64_t>> getProfilerTimes() {}
};