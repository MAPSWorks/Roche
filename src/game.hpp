#pragma once

#include "graphics_api.hpp"

#include "entity.hpp"
#include "renderer.hpp"
#include <glm/glm.hpp>

#include <bitset>
#include <vector>
#include <memory>

/**
 * All application logic
 */
class Game
{
public:
	Game();
	~Game();
	/**
	 * Loads configuration files
	 */
	void init();
	/**
	 * Updates one frame
	 * @dt delta time since last frame
	 */
	void update(double dt);
	/**
	 * Indicates whether the application has been requested to stop
	 */
	bool isRunning();

private:
	/**
	 * Returns true when the key is pressed, but false when it's held
	 * @param key GLFW key to check
	 * @param whether the key is pressed but not held
	 */
	bool isPressedOnce(int key);

	/// Loads entity configuration files
	void loadEntityFiles();
	/// Loads settings file
	void loadSettingsFile();

	enum class SwitchPhase
	{
		IDLE, TRACK, MOVE 
	};
	void updateIdle(float dt, double mousePosX, double mousePosY);
	void updateTrack(float dt);
	void updateMove(float dt);

	/// Returns the id of entity's parent (-1 if no parent)
	int getParent(size_t entityId);
	/// Returns all parents (recursive) until there is no parent
	std::vector<size_t> getAllParents(size_t entityId);
	/// Returns the level of entity inside the hierarchy
	int getLevel(size_t entityId);
	/// Returns direct children
	std::vector<size_t> getChildren(size_t entityId);
	/// Returns all children recursively
	std::vector<size_t> getAllChildren(size_t entityId);

	/// Returns entities in the vicinity of the given entity
	std::vector<size_t> getFocusedEntities(size_t focusedEntityId);

	void displayProfiling(const std::vector<std::pair<std::string, uint64_t>> &a);
	void updateProfiling(const std::vector<std::pair<std::string, uint64_t>> &a);
	std::vector<std::pair<std::string, uint64_t>> computeAverage(
		const std::vector<std::pair<std::string, uint64_t>> &a, int frames);

	void scrollFun(int offsetY);

	/// Renderer
	std::unique_ptr<Renderer> renderer;
	/// Exposure coefficient
	float exposure = 0.0;
	/// Ambient light coefficient
	float ambientColor = 0.0;
	/// MSAA samples per pixel
	int msaaSamples = 1;
	/// Maximum texture width/height
	int maxTexSize = -1;
	/// Render lines instead of faces
	bool wireframe = false;
	/// Render with bloom or not
	bool bloom = true;
	/// Wait for whole texture to load before displaying (no pop-ins)
	bool syncTexLoading = false;

	std::string starMapFilename = "";
	float starMapIntensity = 1.0;
	
	// Main entity collection
	/// Number of entities
	uint32_t entityCount = 0;
	/// Fixed entity parameters
	std::vector<Entity> entityParams;
	/// Dynamic entity state
	std::vector<EntityState> entityStates;
	/// Index in the main collection of parent entity 
	std::vector<int> entityParents;

	/// Index in the main collection of entity the view follows
	size_t focusedEntityId = 0; 
	/// Seconds since January 1st 2017 00:00:00 UTC
	double epoch = 0.0;
	/// Index in the timeWarpValues collection which indicates the current timewarp factor
	size_t timeWarpIndex = 0;
	/// Timewarp factors
	std::vector<double> timeWarpValues 
		= {1, 60, 60*10, 3600, 3600*3, 3600*12, 3600*24, 
			3600*24*10, 3600*24*28, 3600*24*365.2499, 3600*24*365.2499*8};

	/// Entity name display
	size_t entityNameId = focusedEntityId;
	/// Entity name display in/out
	float entityNameFade = 1.f;

	// PROFILING
	/// Total times
	std::vector<std::pair<std::string, uint64_t>> fullTimes;
	/// Max times
	std::vector<std::pair<std::string, uint64_t>> maxTimes;
	int numFrames = 0;

	// VIEW CONTROL
	/// Mouse position of previous update cycle
	double preMousePosX = 0.0;
	/// Mouse position of previous update cycle
	double preMousePosY = 0.0;
	/// Indicates if we are currently dragging the view
	bool dragging = false;
	/// View speed (yaw, pitch, zoom)
	glm::vec3 viewSpeed = glm::vec3(0,0,0);
	/// Max view speed allowed
	float maxViewSpeed = 0.2;
	/// View speed damping for smooth effect
	float viewSmoothness = 0.85;
	/// View position
	glm::dvec3 viewPos;
	/// View matrix
	glm::mat3 viewDir;

	// SWITCHING PLANETS
	/// Indicates if the view is switching from a entity to another
	SwitchPhase switchPhase = SwitchPhase::IDLE;
	/// Time of switching
	float switchTime = 0.0;
	/// Index in main collection of entity switching from
	int switchPreviousEntity = -1; 
	/// View dir when switching started 
	glm::mat3 switchPreviousViewDir;
	/// When view is obstructed when switching, interpolate to this new position
	glm::vec3 switchNewViewPolar;

	/// Mouse sensitivity
	float sensitivity = 0.0004;

	// VIEW COORDINATES
	/// Polar coordinates (theta, phi, distance)
	glm::vec3 viewPolar;
	/// View panning polar coordinates (theta, phi)
	glm::vec2 panPolar;
	/// Vertical Field of view in radians
	float viewFovy = glm::radians(40.f);

	/// GLFW Window pointer
	GLFWwindow *win = nullptr;
	/// Key currently held array
	std::bitset<512> keysHeld;
	/// Window width in pixels
	uint32_t width = 0;
	/// Window height in pixels
	uint32_t height = 0;
	/// Whether window is fullscreen or not
	bool fullscreen = false;
};