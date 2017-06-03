#pragma once

#include "graphics_api.hpp"

#include "planet.hpp"
#include "renderer.hpp"
#include <glm/glm.hpp>

#include <bitset>
#include <vector>
#include <memory>

class Game
{
public:
	Game();
	~Game();
	void init();
	void update(double dt);
	bool isRunning();

private:
	bool isPressedOnce(int key);

	void loadPlanetFiles();
	void loadSettingsFile();

	std::unique_ptr<Renderer> renderer;
	float exposure = 0.0;
	float ambientColor = 0.0;
	int maxTexSize = -1;
	
	// Main planet collection
	uint32_t planetCount = 0;
	std::vector<PlanetParameters> planetParams; // Immutable parameters
	std::vector<PlanetState> planetStates; // Mutable state
	std::vector<int> planetParents;

	size_t focusedPlanetId = 0; // Index of planet the view follows
	double epoch = 0.0; // Seconds since January 1st 1950 00:00
	size_t timeWarpIndex = 0;
	std::vector<double> timeWarpValues 
		= {1, 60, 60*10, 3600, 3600*3, 3600*12, 3600*24, 3600*24*10, 3600*24*365.2499};;

	// INTERACTION RELATED STUFF
	double preMousePosX = 0.0;
	double preMousePosY = 0.0; // previous cursor position
	bool dragging = false; // Indicates if we are currently trying to drag the view
	glm::vec3 viewSpeed = glm::vec3(0,0,0); // Polar coordinate view speed (yaw, pitch, zoom)
	float maxViewSpeed = 0.2;
	float viewSmoothness = 0.85;

	bool isSwitching = false; // Indicates if the view is switching from a planet to another
	int switchFrames = 100; // Number of frames for a switch
	int switchFrameCurrent = 0; // Current frame of switching
	float switchPreviousDist = 0; // Zoom transition amount
	int switchPreviousPlanet = -1; // index of previous planet

	float sensitivity = 0.0004; // Mouse sensitivity

	// camera
	glm::vec3 cameraPolar; // polar coordinates (theta, phi, distance)
	glm::dvec3 cameraCenter; // where the camera is looking at
	glm::dvec3 cameraPos; // actual cartesian coordinates

	GLFWwindow *win = nullptr;
	std::bitset<512> keysHeld;
	uint32_t width = 0;
	uint32_t height = 0;
	bool fullscreen = false;
	int msaaSamples = 1;
};