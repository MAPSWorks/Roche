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
		const std::vector<Planet> &planetParams, 
		int msaa,
		int maxTexSize,
		int windowWidth,
		int windowHeight) {}
	virtual void render(
		const glm::dvec3 &viewPos, 
		float fovy,
		const glm::dvec3 &viewCenter,
		const glm::vec3 &viewUp,
		float exposure,
		float ambientColor,
		bool wireframe,
		const std::vector<PlanetState> &planetStates) {}
	virtual void takeScreenshot(const std::string &filename) {}
	virtual void destroy() {}
	
	virtual std::vector<std::pair<std::string,uint64_t>> getProfilerTimes() { return {}; }
};