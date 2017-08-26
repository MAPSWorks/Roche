#pragma once

#include "entity.hpp"
#include <glm/glm.hpp>
#include <map>
#include <string>

/**
 * Renderer Interface
 */
class Renderer
{
public:
	virtual ~Renderer() {}
	/// Window initialization necessary for the API to work
	virtual void windowHints() {}

	struct InitInfo
	{
		/// All of the entities fixed parameters
		std::vector<Entity> entityParams;
		/// Star map texture filename
		std::string starMapFilename;
		/// Star map brightness
		float starMapIntensity;
		/// MSAA samples per pixel
		int msaa;
		/// Maximum texture width/height
		int maxTexSize;
		/// Wait for whole texture to load before displaying
		int syncTexLoading;
		/// Window width in pixels
		unsigned windowWidth;
		/// Window height in pixels
		unsigned windowHeight;
	};

	struct RenderInfo
	{
		/// View position in world space
		glm::dvec3 viewPos;
		/// Vertical field of view in radians
		float fovy;
		/// View direction matrix
		glm::mat3 viewDir;
		/// Exposure factor
		float exposure;
		/// Ambient light coefficient
		float ambientColor;
		/// Whether to render geometry as lines or faces
		bool wireframe;
		/// Whether to activate bloom or not
		bool bloom;
		/// All of the entities dynamic state
		std::vector<EntityState> entityStates;
		/// Ids of entities currently in focus
		std::vector<size_t> focusedEntitiesId;
		/// Name of focused body
		std::string focusedEntityName;
		/// Fade in/out of focused body name
		float entityNameFade;
		/// Formatted time
		std::string currentTime;
	};

	/** Initializes the renderer
	 * @param info Initialization info
	 */
	virtual void init(const InitInfo &info) {}

	/** Renders one frame with the given info
	 * @param info Rendering info
	 */
	virtual void render(const RenderInfo &info) {}

	/** Sets a screenshot to be taken and saved at the given location
	 * @param filename filename to save screenshot image to
	 */
	virtual void takeScreenshot(const std::string &filename) {}

	/** 
	 * Deletes resources
	 */
	virtual void destroy() {}
	
	/** Returns profiler times in nanoseconds associated with their label
	 * @return a vector of pairs of strings (label of time range) and uint64_t (time range in ns)
	 */
	virtual std::vector<std::pair<std::string,uint64_t>> getProfilerTimes() { return {}; }
};