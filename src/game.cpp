#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <algorithm>

#include "renderer_gl.hpp"
#include "util.h"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"
#include "thirdparty/lodepng.h"

#include <glm/ext.hpp>

const int MIN_FLARE_DIST = 100.0;
const int MAX_PLANET_DIST = 200.0;

const float CAMERA_FOVY = 40.0;

Game::Game()
{
	sensitivity = 0.0004;
	viewSpeed = glm::vec3(0,0,0);
	maxViewSpeed = 0.2;
	viewSmoothness = 0.85;
	switchPreviousPlanet = -1;
	save = false;

	timeWarpValues = {1, 60, 60*10, 3600, 3600*3, 3600*12, 3600*24, 3600*24*10, 3600*24*365.2499};
	timeWarpIndex = 0;

	switchFrames = 100;
	switchFrameCurrent = 0;
	isSwitching = false;
	focusedPlanetId = 0;
	epoch = 0.0;
	quit = false;

	msaaSamples = 1;
	gamma = 2.2;

	renderer.reset(new RendererGL());
}

Game::~Game()
{
	renderer->destroy();

	glfwTerminate();

	quit = true;
	screenshotThread.join();
}

void ssThread(
	const std::atomic<bool> &quit, std::vector<uint8_t> &buffer,
	std::atomic<bool> &save, const int width, const int height)
{
	while (!quit)
	{
		if (save)
		{
			time_t t = time(0);
			struct tm *now = localtime(&t);
			std::stringstream filename;
			filename << 
				"./screenshots/screenshot_" << 
				(now->tm_year+1900) << "-" << 
				(now->tm_mon+1) << "-" << 
				(now->tm_mday) << "_" << 
				(now->tm_hour) << "-" << 
				(now->tm_min) << "-" << 
				(now->tm_sec) << ".png";

			unsigned int error = lodepng::encode(filename.str(), buffer.data(), width, height);
			if (error)
			{
				std::cout << "Can't save screenshot: " << lodepng_error_text(error) << std::endl;
			}
			save = false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void Game::loadSettingsFile()
{
	try 
	{
		shaun::parser p;
		shaun::object obj = p.parse(read_file("config/settings.sn").c_str());
		shaun::sweeper swp(&obj);

		shaun::sweeper video(swp("video"));
		auto fs = video("fullscreen");
		fullscreen = (fs.is_null())?true:(bool)fs.value<shaun::boolean>();

		if (!fullscreen)
		{
			width = video("width").value<shaun::number>();
			height = video("height").value<shaun::number>();
		}

		shaun::sweeper graphics(swp("graphics"));
		DDSLoader::setSkipMipmap(graphics("skipMipmap").value<shaun::number>());
		msaaSamples = graphics("msaaSamples").value<shaun::number>();
		gamma = graphics("gamma").value<shaun::number>();

		shaun::sweeper controls(swp("controls"));
		sensitivity = controls("sensitivity").value<shaun::number>();
	} 
	catch (shaun::parse_error e)
	{
		std::cout << e << std::endl;
	}
}

void Game::init()
{
	loadSettingsFile();
	loadPlanetFiles();

	cameraPolar.z = planetParams[focusedPlanetId].bodyParam.radius*4;

	// Window & context creation
	if (!glfwInit())
		exit(-1);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	renderer->windowHints();

	if (fullscreen)
	{
		width = mode->width;
		height = mode->height;
	}
	win = glfwCreateWindow(width, height, "Roche", fullscreen?monitor:nullptr, nullptr);

	if (!win)
	{
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(win);

	const GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		std::cout << "Some shit happened: " << glewGetErrorString(err) << std::endl;
	}

	// Screenshot
	screenshotBuffer.resize(width*height*4);
	screenshotThread = std::thread(
		ssThread, std::ref(quit), std::ref(screenshotBuffer), std::ref(save), width, height);

	renderer->init(planetParams, skybox, msaaSamples, width, height, gamma);
}

template<class T>
T get(shaun::sweeper swp);

template <>
double get(shaun::sweeper swp)
{
	if (swp.is_null()) return 0.0; else return swp.value<shaun::number>();
}

template <>
std::string get(shaun::sweeper swp)
{
	if (swp.is_null()) return ""; else return std::string(swp.value<shaun::string>());
}

template <>
bool get(shaun::sweeper swp)
{
	if (swp.is_null()) return false; else return swp.value<shaun::boolean>();
}

template<>
glm::vec3 get(shaun::sweeper swp)
{
	glm::vec3 ret;
	if (swp.is_null()) return ret;
	for (int i=0;i<3;++i)
		ret[i] = swp[i].value<shaun::number>();
	return ret;
}

template<>
glm::vec4 get(shaun::sweeper swp)
{
	glm::vec4 ret;
	if (swp.is_null()) return ret;
	for (int i=0;i<4;++i)
		ret[i] = swp[i].value<shaun::number>();
	return ret;
}

void Game::loadPlanetFiles()
{
	try 
	{
		shaun::parser p;
		shaun::object obj = p.parse(read_file("config/planets.sn").c_str());
		shaun::sweeper swp(&obj);

		shaun::sweeper sky(swp("skybox"));
		skybox.inclination = glm::radians(get<double>(sky("inclination")));
		skybox.textureFilename = get<std::string>(sky("texture"));
		skybox.intensity = get<double>(sky("intensity"));

		shaun::sweeper planetsSweeper(swp("planets"));
		planetCount = planetsSweeper.value<shaun::list>().elements().size();
		planetParams.resize(planetCount);
		planetStates.resize(planetCount);
		planetParents.resize(planetCount, -1);
		for (uint32_t i=0;i<planetCount;++i)
		{
			PlanetParameters planet;
			shaun::sweeper pl(planetsSweeper[i]);
			planet.name = std::string(pl("name").value<shaun::string>());
			planet.parentName = get<std::string>(pl("parent"));

			shaun::sweeper orbit(pl("orbit"));
			if (!orbit.is_null())
			{
				planet.orbitParam.ecc = get<double>(orbit("ecc"));
				planet.orbitParam.sma = get<double>(orbit("sma"));
				planet.orbitParam.inc = glm::radians(get<double>(orbit("inc")));
				planet.orbitParam.lan = glm::radians(get<double>(orbit("lan")));
				planet.orbitParam.arg = glm::radians(get<double>(orbit("arg")));
				planet.orbitParam.m0  = glm::radians(get<double>(orbit("m0" )));
			}
			shaun::sweeper body(pl("body"));
			if (!body.is_null())
			{
				planet.bodyParam.radius = get<double>(body("radius"));
				float rightAscension = glm::radians(get<double>(body("rightAscension")));
				float declination = glm::radians(get<double>(body("declination")));
				planet.bodyParam.rotationAxis = glm::vec3(
					-sin(rightAscension)*cos(declination),
					 cos(rightAscension)*cos(declination),
					 sin(declination));
				planet.bodyParam.rotationPeriod = get<double>(body("rotPeriod"));
				planet.bodyParam.meanColor = get<glm::vec3>(body("meanColor"));
				planet.bodyParam.albedo = get<double>(body("albedo"));
				planet.bodyParam.GM = get<double>(body("GM"));
				planet.bodyParam.isStar = get<bool>(  body("isStar"));
				planet.assetPaths.diffuseFilename = get<std::string>(body("diffuse"));
				planet.assetPaths.nightFilename = get<std::string>(body("night"));
				planet.assetPaths.cloudFilename = get<std::string>(body("cloud"));
				planet.assetPaths.modelFilename = get<std::string>(body("model"));
				planet.bodyParam.cloudDispPeriod = get<double>(body("cloudDispPeriod"));
				planet.bodyParam.nightTexIntensity = get<double>(body("nightTexIntensity"));
			}
			shaun::sweeper atmo(pl("atmosphere"));
			if (!atmo.is_null())
			{
				planet.atmoParam.maxHeight = get<double>(atmo("maxAltitude"));
				planet.atmoParam.K_R = get<double>(atmo("K_R"));
				planet.atmoParam.K_M = get<double>(atmo("K_M"));
				planet.atmoParam.E   = get<double>(atmo("E"));
				planet.atmoParam.C_R = get<glm::vec3>(atmo("C_R"));
				planet.atmoParam.G_M = get<double>(atmo("G_M"));
				planet.atmoParam.scaleHeight = get<double>(atmo("scaleHeight"));
			}

			shaun::sweeper ring(pl("ring"));
			if (!ring.is_null())
			{
				planet.ringParam.hasRings = true;
				planet.ringParam.innerDistance = get<double>(ring("inner"));
				planet.ringParam.outerDistance = get<double>(ring("outer"));
				float rightAscension = glm::radians(get<double>(ring("rightAscension")));
				float declination = glm::radians(get<double>(ring("declination")));
				planet.ringParam.normal = glm::vec3(
					-sin(rightAscension)*cos(declination),
					 cos(rightAscension)*cos(declination),
					 sin(declination));
				planet.ringParam.seed = (int)get<double>(ring("seed"));
				planet.ringParam.color = get<glm::vec4>(ring("color"));
			}
			planetParams[i] = planet;
		}
		// Assign planet parents
		for (uint32_t i=0;i<planetCount;++i)
		{
			const std::string parent = planetParams[i].parentName;
			if (parent != "")
			{
				for (uint32_t j=0;j<planetCount;++j)
				{
					if (i==j) continue;
					if (planetParams[j].name == parent)
					{
						planetParents[i] = j;
						break;
					}
				}
			}
		}	
	} 
	catch (shaun::parse_error e)
	{
		std::cout << e << std::endl;
	}
}

bool Game::isPressedOnce(const int key)
{
	if (glfwGetKey(win, key))
	{
		if (keysHeld[key]) return false;
		else return (keysHeld[key] = true);
	}
	else
	{
		return (keysHeld[key] = false);
	}
}

void Game::update(const double dt)
{
	epoch += timeWarpValues[timeWarpIndex]*dt;

	std::vector<glm::dvec3> relativePositions(planetCount);
	// Planet state update
	for (uint32_t i=0;i<planetCount;++i)
	{
		// Relative position update
		if (planetParents[i] == -1)
			planetStates[i].position = glm::dvec3(0,0,0);
		else
		{
			relativePositions[i] = 
				planetParams[i].orbitParam.computePosition(
					epoch, planetParams[planetParents[i]].bodyParam.GM);
		}
		// Rotation
		planetStates[i].rotationAngle = (2.0*PI*epoch)/planetParams[i].bodyParam.rotationPeriod + PI;
		// Cloud rotation
		const float period = planetParams[i].bodyParam.cloudDispPeriod;
		planetStates[i].cloudDisp = (period)?
			std::fmod(-epoch/period, 1.f):
			0.f;
	}

	// Planet absolute position update
	for (uint32_t i=0;i<planetCount;++i)
	{
		planetStates[i].position = relativePositions[i];
		int parent = planetParents[i];
		while (parent != -1)
		{
			planetStates[i].position += relativePositions[parent];
			parent = planetParents[parent];
		}
	}

	// Time warping
	if (isPressedOnce(GLFW_KEY_K))
	{
		if (timeWarpIndex > 0) timeWarpIndex--;
	}
	if (isPressedOnce(GLFW_KEY_L))
	{
		if (timeWarpIndex < (int)timeWarpValues.size()-1) timeWarpIndex++;
	}

	// Switching
	if (isPressedOnce(GLFW_KEY_TAB))
	{
		if (isSwitching)
		{
			isSwitching = false;
			cameraPolar.z = planetParams[focusedPlanetId].bodyParam.radius*4;
		}
		else
		{
			switchPreviousPlanet = focusedPlanetId;
			focusedPlanetId = (focusedPlanetId+1)%planetCount;
			switchPreviousDist = cameraPolar.z;
			isSwitching = true;
		} 
	}

	if (isSwitching)
	{
		const float t = switchFrameCurrent/(float)switchFrames;
		const double f = 6*t*t*t*t*t-15*t*t*t*t+10*t*t*t;
		const glm::dvec3 previousPlanetPos = planetStates[switchPreviousPlanet].position;
		cameraCenter = (planetStates[focusedPlanetId].position - previousPlanetPos)*f + previousPlanetPos;
		const float targetDist = planetParams[focusedPlanetId].bodyParam.radius*4;
		cameraPolar.z = (targetDist - switchPreviousDist)*f + switchPreviousDist;

		++switchFrameCurrent;
	}
	else
	{
		cameraCenter = planetStates[focusedPlanetId].position;
	}

	if (switchFrameCurrent > switchFrames)
	{
		isSwitching = false;
		switchFrameCurrent = 0;
	}

	// Mouse move
	double posX, posY;
	glfwGetCursorPos(win, &posX, &posY);
	const glm::vec2 move = {-posX+preMousePosX, posY-preMousePosY};

	if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_1))
	{
		viewSpeed.x += move.x*sensitivity;
		viewSpeed.y += move.y*sensitivity;
		for (int i=0;i<2;++i)
		{
			if (viewSpeed[i] > maxViewSpeed) viewSpeed[i] = maxViewSpeed;
			if (viewSpeed[i] < -maxViewSpeed) viewSpeed[i] = -maxViewSpeed;
		}
	}
	else if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2))
	{
		viewSpeed.z += (move.y*sensitivity);
	}

	cameraPolar.x += viewSpeed.x;
	cameraPolar.y += viewSpeed.y;
	cameraPolar.z *= 1.0+viewSpeed.z;

	viewSpeed *= viewSmoothness;

	if (cameraPolar.y > PI/2 - 0.001)
	{
		cameraPolar.y = PI/2 - 0.001;
		viewSpeed.y = 0;
	}
	if (cameraPolar.y < -PI/2 + 0.001)
	{
		cameraPolar.y = -PI/2 + 0.001;
		viewSpeed.y = 0;
	}
	const float radius = planetParams[focusedPlanetId].bodyParam.radius;
	if (cameraPolar.z < radius) cameraPolar.z = radius;

	// Mouse reset
	preMousePosX = posX;
	preMousePosY = posY;

	if (isPressedOnce(GLFW_KEY_F12) && !save)
	{
		int width,height;
		glfwGetWindowSize(win, &width, &height);
		glReadPixels(0,0,width,height, GL_RGBA, GL_UNSIGNED_BYTE, screenshotBuffer.data());
		save = true;
	}

	// Shift scene so view is at (0,0,0)
	cameraPos = glm::dvec3(
		cos(cameraPolar.x)*cos(cameraPolar.y), 
		sin(cameraPolar.x)*cos(cameraPolar.y), 
		sin(cameraPolar.y))*(double)cameraPolar.z +
		cameraCenter;
		
	renderer->render(
		cameraPos, glm::radians(CAMERA_FOVY), cameraCenter, glm::vec3(0,0,1), 
		planetStates);

	glfwSwapBuffers(win);
	glfwPollEvents();
}

bool Game::isRunning()
{
	return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}