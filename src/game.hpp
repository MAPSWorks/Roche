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

	/// Returns bodies that need to have their texture loaded when the focus is on 'focusedEntity'
	std::vector<EntityHandle> getTexLoadBodies(const EntityHandle &focusedEntity);

	void displayProfiling(const std::vector<std::pair<std::string, uint64_t>> &a);
	void updateProfiling(const std::vector<std::pair<std::string, uint64_t>> &a);
	std::vector<std::pair<std::string, uint64_t>> computeAverage(
		const std::vector<std::pair<std::string, uint64_t>> &a, int frames);

	void scrollFun(int offsetY);

	EntityHandle getFocusedBody();
	EntityHandle getDisplayedBody();
	EntityHandle getPreviousBody();
	int chooseNextBody(bool direction);

	// Main entity collection
	EntityCollection _entityCollection;

	/// Index in the  the view follows
	int _focusedBodyId = 0; 
	/// Seconds since January 1st 2017 00:00:00 UTC
	double _epoch = 0.0;
	/// Index in the timeWarpValues collection which indicates the current timewarp factor
	int _timeWarpIndex = 0;
	/// Timewarp factors
	std::vector<double> _timeWarpValues 
		= {1, 60, 60*10, 3600, 3600*3, 3600*12, 3600*24, 
			3600*24*7, 3600*24*28, 3600*24*365.25, 3600*24*365.25*8};

	/// Entity name display
	int _bodyNameId = _focusedBodyId;
	/// Entity name display in/out
	float _bodyNameFade = 1.f;

	/// Renderer
	std::unique_ptr<Renderer> _renderer;
	/// Exposure coefficient
	float _exposure = 0.0;
	/// Ambient light coefficient
	float _ambientColor = 0.0;
	/// MSAA samples per pixel
	int _msaaSamples = 1;
	/// Maximum texture width/height
	int _maxTexSize = -1;
	/// Render lines instead of faces
	bool _wireframe = false;
	/// Render with bloom or not
	bool _bloom = true;
	/// Wait for whole texture to load before displaying (no pop-ins)
	bool _syncTexLoading = false;

	std::string _starMapFilename = "";
	float _starMapIntensity = 1.0;

	// PROFILING
	/// Total times
	std::vector<std::pair<std::string, uint64_t>> _fullTimes;
	/// Max times
	std::vector<std::pair<std::string, uint64_t>> _maxTimes;
	int _numFrames = 0;

	// VIEW CONTROL
	/// Mouse position of previous update cycle
	double _preMousePosX = 0.0;
	/// Mouse position of previous update cycle
	double _preMousePosY = 0.0;
	/// Indicates if we are currently dragging the view
	bool _dragging = false;
	/// View speed (yaw, pitch, zoom)
	glm::vec3 _viewSpeed = glm::vec3(0,0,0);
	/// Max view speed allowed
	float _maxViewSpeed = 0.2;
	/// View speed damping for smooth effect
	float _viewSmoothness = 0.85;
	/// View position
	glm::dvec3 _viewPos;
	/// View matrix
	glm::mat3 _viewDir;

	// SWITCHING PLANETS
	/// Indicates if the view is switching from a entity to another
	SwitchPhase _switchPhase = SwitchPhase::IDLE;
	/// Time of switching
	float _switchTime = 0.0;
	/// Index in main collection of entity switching from
	int _switchPreviousBodyId;
	/// View dir when switching started 
	glm::mat3 _switchPreviousViewDir;
	/// When view is obstructed when switching, interpolate to this new position
	glm::vec3 _switchNewViewPolar;

	/// Mouse sensitivity
	float _sensitivity = 0.0004;

	// VIEW COORDINATES
	/// Polar coordinates (theta, phi, distance)
	glm::vec3 _viewPolar;
	/// View panning polar coordinates (theta, phi)
	glm::vec2 _panPolar;
	/// Vertical Field of view in radians
	float _viewFovy = glm::radians(40.f);

	/// GLFW Window pointer
	GLFWwindow *_win = nullptr;
	/// Key currently held array
	std::bitset<512> _keysHeld;
	/// Window width in pixels
	uint32_t _width = 0;
	/// Window height in pixels
	uint32_t _height = 0;
	/// Whether window is fullscreen or not
	bool _fullscreen = false;
};