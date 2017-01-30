#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "planet.hpp"
#include "renderer.hpp"
#include <glm/glm.hpp>

#include <bitset>
#include <vector>
#include <atomic>
#include <thread>

#include <nanogui/screen.h>

#define PI        3.14159265358979323846264338327950288 

class Game
{
public:
	Game();
	~Game();
	void init();
	void update(double dt);
	bool isRunning();

private:
	void initGUI();
	bool isPressedOnce(int key);

	void loadPlanetFiles();
	void loadSettingsFile();

	void createSettingsWindow();
	void createProfilerWindow();

	// GUI
	nanogui::Screen *guiScreen;

	std::unique_ptr<Renderer> renderer;
	float gamma;
	float exposure;
	float ambientColor;
	
	// Main planet collection
	uint32_t planetCount;
	std::vector<PlanetParameters> planetParams; // Immutable parameters
	std::vector<PlanetState> planetStates; // Mutable state
	std::vector<int> planetParents;

	size_t focusedPlanetId; // Index of planet the view follows
	double epoch; // Seconds since January 1st 1950 00:00
	size_t timeWarpIndex;
	std::vector<double> timeWarpValues;

	// THREADING RELATED STUFF
	std::thread textureLoadThread; // Texture loading Thread
	std::thread screenshotThread;
	std::atomic<bool> save; // Indicates if the screenshot thread has to save the framebuffer to a file now
	std::atomic<bool> quit; // boolean for killing threads

	std::vector<uint8_t> screenshotBuffer; // pixel array

	// INTERACTION RELATED STUFF
	double preMousePosX, preMousePosY; // previous cursor position
	bool dragging; // Indicates if we are currently trying to drag the view
	bool canDrag; // Indicate if we can drag the view (not over gui,...)
	glm::vec3 viewSpeed;
	float maxViewSpeed, viewSmoothness;

	bool isSwitching; // Indicates if the view is switching from a planet to another
	int switchFrames; // Number of frames for a switch
	int switchFrameCurrent; // Current frame of switching
	float switchPreviousDist; // Zoom transition amount
	int switchPreviousPlanet; // index of previous planet

	float sensitivity; // Mouse sensitivity

	// camera
	glm::vec3 cameraPolar; // polar coordinates (theta, phi, distance)
	glm::dvec3 cameraCenter; // where the camera is looking at
	glm::dvec3 cameraPos; // actual cartesian coordinates

	GLFWwindow *win;
	std::bitset<512> keysHeld;
	uint32_t width, height;
	bool fullscreen;
	int msaaSamples;
	float ssaa;
};