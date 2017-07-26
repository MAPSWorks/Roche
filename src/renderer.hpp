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

	struct InitInfo
	{
		std::vector<Planet> planetParams;
		int msaa;
		int maxTexSize;
		unsigned windowWidth;
		unsigned windowHeight;
	};

	struct RenderInfo
	{
		glm::dvec3 viewPos;
		float fovy;
		glm::dvec3 viewCenter;
		glm::vec3 viewUp;
		float exposure;
		float ambientColor;
		bool wireframe;
		bool bloom;
		std::vector<PlanetState> planetStates;
		std::vector<size_t> focusedPlanetsId;
	};

	virtual void init(const InitInfo &info) {}
	virtual void render(const RenderInfo &info) {}
	virtual void takeScreenshot(const std::string &filename) {}
	virtual void destroy() {}
	
	virtual std::vector<std::pair<std::string,uint64_t>> getProfilerTimes() { return {}; }
};