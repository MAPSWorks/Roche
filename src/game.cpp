#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

#include "renderer.hpp"
#include "renderer_gl.hpp"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"

#include <glm/ext.hpp>

using namespace glm;

std::string generateScreenshotName();

Game::Game()
{
	renderer.reset(new RendererGL());
}

Game::~Game()
{
	renderer->destroy();

	glfwTerminate();
}

std::string readFile(const std::string &filename)
{
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
	if (!in) throw std::runtime_error("Can't open" + filename);
	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	return contents;
}

void Game::loadSettingsFile()
{
	using namespace shaun;
	try 
	{
		parser p{};
		const std::string fileContent = readFile("config/settings.sn");
		object obj = p.parse(fileContent.c_str());
		sweeper swp(&obj);

		sweeper video(swp("video"));
		auto fs = video("fullscreen");
		fullscreen = (fs.is_null())?true:(bool)fs.value<boolean>();

		if (!fullscreen)
		{
			width = video("width").value<number>();
			height = video("height").value<number>();
		}

		sweeper graphics(swp("graphics"));
		maxTexSize = graphics("maxTexSize").value<number>();
		msaaSamples = graphics("msaaSamples").value<number>();
		syncTexLoading = graphics("syncTexLoading").value<boolean>();

		sweeper controls(swp("controls"));
		sensitivity = controls("sensitivity").value<number>();
	} 
	catch (parse_error &e)
	{
		std::cout << e << std::endl;
	}
}

std::function<void(int)> uglyScrollWorkaround;

void Game::init()
{
	loadSettingsFile();
	loadPlanetFiles();

	viewPolar.z = planetParams[focusedPlanetId].getBody().getRadius()*4;

	// Window & context creation
	if (!glfwInit())
		throw std::runtime_error("Can't init GLFW");

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
	renderer->windowHints();

	if (fullscreen)
	{
		width = mode->width;
		height = mode->height;
	}
	win = glfwCreateWindow(width, height, "Roche", 
		fullscreen?monitor:nullptr, 
		nullptr);

	uglyScrollWorkaround = [this](int offset){
		if (switchPhase == SwitchPhase::IDLE)
		{
			// FOV zoom/unzoom when alt key held
			if (glfwGetKey(win, GLFW_KEY_LEFT_ALT))
			{
				viewFovy = clamp(viewFovy*pow(0.5f, 
					(float)offset*sensitivity*100),
					radians(0.1f), radians(40.f));
			}
			// Exposure +/-
			else if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL))
			{
				exposure = clamp(exposure+0.1f*offset, -4.f, 4.f);
			}
			// Distance zoom/unzoom
			else
			{
				viewSpeed.z -= 40*offset*sensitivity;
			}
		}
	};

	glfwSetScrollCallback(win, [](GLFWwindow* win, double, double yoffset){
		uglyScrollWorkaround(yoffset);
	});

	if (!win)
	{
		glfwTerminate();
		throw std::runtime_error("Can't open window");
	}
	glfwMakeContextCurrent(win);

	glewExperimental = true;
	const GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		throw std::runtime_error("Can't initialize GLEW : " + std::string((const char*)glewGetErrorString(err)));
	}

	// Renderer init
	renderer->init({
		planetParams, 
		starMapFilename, 
		starMapIntensity, 
		msaaSamples, 
		maxTexSize, 
		syncTexLoading, 
		width, height});
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
vec3 get(shaun::sweeper swp)
{
	vec3 ret;
	if (swp.is_null()) return ret;
	for (int i=0;i<3;++i)
		ret[i] = swp[i].value<shaun::number>();
	return ret;
}

template<>
vec4 get(shaun::sweeper swp)
{
	vec4 ret;
	if (swp.is_null()) return ret;
	for (int i=0;i<4;++i)
		ret[i] = swp[i].value<shaun::number>();
	return ret;
}

vec3 axis(const float rightAscension, const float declination)
{
	return vec3(
		-sin(rightAscension)*cos(declination),
		 cos(rightAscension)*cos(declination),
		 sin(declination));
}
	
void Game::loadPlanetFiles()
{
	using namespace shaun;
	try
	{
		parser p;
		std::string fileContent = readFile("config/planets.sn");
		object obj = p.parse(fileContent.c_str());
		sweeper swp(&obj);

		ambientColor = (float)get<double>(swp("ambientColor"));
		std::string startingPlanet = std::string(swp("startingPlanet").value<string>());

		sweeper starMap(swp("starMap"));
		starMapFilename = get<std::string>(starMap("diffuse"));
		starMapIntensity = (float)get<double>(starMap("intensity"));

		sweeper planetsSweeper(swp("planets"));
		planetCount = planetsSweeper.value<list>().elements().size();
		planetParams.resize(planetCount);
		planetStates.resize(planetCount);
		planetParents.resize(planetCount, -1);

		for (uint32_t i=0;i<planetCount;++i)
		{
			sweeper pl(planetsSweeper[i]);
			std::string name = std::string(pl("name").value<string>());
			// Set focus on starting planet
			if (name == startingPlanet) focusedPlanetId = i;
			// Create planet
			Planet planet;
			planet.setName(name);
			planet.setParentName(get<std::string>(pl("parent")));

			sweeper orbitsw(pl("orbit"));
			if (!orbitsw.is_null())
			{
				const Planet::Orbit orbit(
					get<double>(orbitsw("ecc")),
					get<double>(orbitsw("sma")),
					radians(get<double>(orbitsw("inc"))),
					radians(get<double>(orbitsw("lan"))),
					radians(get<double>(orbitsw("arg"))),
					radians(get<double>(orbitsw("m0"))));
				planet.setOrbit(orbit);
			}
			sweeper bodysw(pl("body"));
			if (!bodysw.is_null())
			{
				const Planet::Body body(
					get<double>(bodysw("radius")),
					get<double>(bodysw("GM")),
					axis(
						radians(get<double>(bodysw("rightAscension"))),
						radians(get<double>(bodysw("declination")))),
					get<double>(bodysw("rotPeriod")),
					get<vec3>(bodysw("meanColor"))*
					(float)get<double>(bodysw("albedo")),
					get<std::string>(bodysw("diffuse")));
				planet.setBody(body);
			}
			sweeper atmosw(pl("atmo"));
			if (!atmosw.is_null())
			{
				Planet::Atmo atmo(
					get<vec4>(atmosw("K")),
					get<double>(atmosw("density")),
					get<double>(atmosw("maxHeight")),
					get<double>(atmosw("scaleHeight")));
				planet.setAtmo(atmo);
			}

			sweeper ringsw(pl("ring"));
			if (!ringsw.is_null())
			{
				Planet::Ring ring(
					get<double>(ringsw("inner")),
					get<double>(ringsw("outer")),
					axis(
						radians(get<double>(ringsw("rightAscension"))),
						radians(get<double>(ringsw("declination")))),
					get<std::string>(ringsw("backscat")),
					get<std::string>(ringsw("forwardscat")),
					get<std::string>(ringsw("unlit")),
					get<std::string>(ringsw("transparency")),
					get<std::string>(ringsw("color")));
				planet.setRing(ring);
			}

			sweeper starsw(pl("star"));
			if (!starsw.is_null())
			{
				Planet::Star star(
					get<double>(starsw("brightness")),
					get<double>(starsw("flareFadeInStart")),
					get<double>(starsw("flareFadeInEnd")),
					get<double>(starsw("flareAttenuation")),
					get<double>(starsw("flareMinSize")),
					get<double>(starsw("flareMaxSize")));
				planet.setStar(star);
			}

			sweeper cloudssw(pl("clouds"));
			if (!cloudssw.is_null())
			{
				Planet::Clouds clouds(
					get<std::string>(cloudssw("filename")),
					get<double>(cloudssw("period")));
				planet.setClouds(clouds);
			}

			sweeper nightsw(pl("night"));
			if (!nightsw.is_null())
			{
				Planet::Night night(
					get<std::string>(nightsw("filename")),
					get<double>(nightsw("intensity")));
				planet.setNight(night);
			}

			sweeper specsw(pl("specular"));
			if (!specsw.is_null())
			{
				sweeper mask0(specsw("mask0"));
				sweeper mask1(specsw("mask1"));
				Planet::Specular spec(
					get<std::string>(specsw("filename")),
					{get<vec3>(mask0("color")), 
					 (float)get<double>(mask0("hardness"))},
					{get<vec3>(mask1("color")),
					 (float)get<double>(mask1("hardness"))});
				planet.setSpecular(spec);
			}

			planetParams[i] = planet;
		}
		// Assign planet parents
		for (uint32_t i=0;i<planetCount;++i)
		{
			const std::string parent = planetParams[i].getParentName();
			if (parent != "")
			{
				for (uint32_t j=0;j<planetCount;++j)
				{
					if (i==j) continue;
					if (planetParams[j].getName() == parent)
					{
						planetParents[i] = j;
						break;
					}
				}
			}
		}
	} 
	catch (parse_error &e)
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

vec3 polarToCartesian(const vec2 &p)
{
	return vec3(
		cos(p.x)*cos(p.y), 
		sin(p.x)*cos(p.y), 
		sin(p.y));
}

void Game::update(const double dt)
{
	epoch += timeWarpValues[timeWarpIndex]*dt;

	std::vector<dvec3> relativePositions(planetCount);
	// Planet state update
	for (uint32_t i=0;i<planetCount;++i)
	{
		// Relative position update
		relativePositions[i] = 
			(getParent(i) == -1 || !planetParams[i].hasOrbit())?
			dvec3(0.0):
			planetParams[i].getOrbit().computePosition(
				epoch, planetParams[getParent(i)].getBody().getGM());
	}

	// Planet absolute position update
	for (uint32_t i=0;i<planetCount;++i)
	{
		dvec3 absPosition = relativePositions[i];
		int parent = getParent(i);
		while (parent != -1)
		{
			absPosition += relativePositions[parent];
			parent = getParent(parent);
		}

		// Planet Angle
		const float rotationAngle = 
			(2.0*pi<float>())*
			fmod(epoch/planetParams[i].getBody().getRotationPeriod(),1.f)
			+ pi<float>();

		// Cloud Displacement
		const float cloudDisp = [&]{
			if (planetParams[i].hasClouds()) return 0.0;
			const float period = planetParams[i].getClouds().getPeriod();
			return (period)?fmod(-epoch/period, 1.f):0.f;
		}();

		planetStates[i] = PlanetState(absPosition, rotationAngle, cloudDisp);
	}
	
	// Wireframe on/off
	if (isPressedOnce(GLFW_KEY_W))
	{
		wireframe = !wireframe;
	}

	// Bloom on/off
	if (isPressedOnce(GLFW_KEY_B))
	{
		bloom = !bloom;
	}

	// Mouse move
	double posX, posY;
	glfwGetCursorPos(win, &posX, &posY);

	if (switchPhase == SwitchPhase::IDLE)
	{
		updateIdle(dt, posX, posY);
	}
	else if (switchPhase == SwitchPhase::TRACK)
	{
		updateTrack(dt);
	}
	else if (switchPhase == SwitchPhase::MOVE)
	{
		updateMove(dt);
	}

	// Mouse reset
	preMousePosX = posX;
	preMousePosY = posY;

	// Screenshot
	if (isPressedOnce(GLFW_KEY_F12))
	{
		renderer->takeScreenshot(generateScreenshotName());
	}

	// Focused planets
	const std::vector<size_t> visiblePlanetsId = getFocusedPlanets(focusedPlanetId);
		
	// Scene rendering
	renderer->render({
		viewPos, viewFovy, viewDir,
		exposure, ambientColor, wireframe, bloom, 
		planetStates, visiblePlanetsId});

	auto a = renderer->getProfilerTimes();

	updateProfiling(a);

	// Display profiler in console
	if (isPressedOnce(GLFW_KEY_F5) && !a.empty())
	{
		std::cout << "Current Frame: " << std::endl;
		displayProfiling(a);
		auto b = computeAverage(fullTimes, numFrames);
		std::cout << "Average: " << std::endl;
		displayProfiling(b);
		std::cout << "Max: " << std::endl;
		displayProfiling(maxTimes);
	}

	glfwSwapBuffers(win);
	glfwPollEvents();
}

bool Game::isRunning()
{
	return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}

void Game::updateIdle(float dt, int posX, int posY)
{
	const vec2 move = {-posX+preMousePosX, posY-preMousePosY};

	const bool mouseButton1 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_1);
	const bool mouseButton2 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2);

	if ((mouseButton1 || mouseButton2) && !dragging)
	{
		dragging = true;
	}
	else if (dragging && !(mouseButton1 || mouseButton2))
	{
		dragging = false;
	}

	// Drag view around
	if (dragging)
	{
		if (mouseButton1)
		{	
			viewSpeed.x += move.x*sensitivity;
			viewSpeed.y += move.y*sensitivity;
			for (int i=0;i<2;++i)
			{
				if (viewSpeed[i] > maxViewSpeed) viewSpeed[i] = maxViewSpeed;
				if (viewSpeed[i] < -maxViewSpeed) viewSpeed[i] = -maxViewSpeed;
			}
		}
		else if (mouseButton2)
		{
			panPolar += move*sensitivity*viewFovy;
		}
	}

	const float radius = planetParams[focusedPlanetId].getBody().getRadius();

	viewPolar.x += viewSpeed.x;
	viewPolar.y += viewSpeed.y;
	viewPolar.z += viewSpeed.z*(viewPolar.z-radius);

	viewSpeed *= viewSmoothness;

	const float maxVerticalAngle = pi<float>()/2 - 0.001;

	if (viewPolar.y > maxVerticalAngle)
	{
		viewPolar.y = maxVerticalAngle;
		viewSpeed.y = 0;
	}
	if (viewPolar.y < -maxVerticalAngle)
	{
		viewPolar.y = -maxVerticalAngle;
		viewSpeed.y = 0;
	}
	if (viewPolar.z < radius) viewPolar.z = radius;

	if (viewPolar.y + panPolar.y > maxVerticalAngle)
	{
		panPolar.y = maxVerticalAngle - viewPolar.y;
	}
	if (viewPolar.y + panPolar.y < -maxVerticalAngle)
	{
		panPolar.y = -maxVerticalAngle - viewPolar.y;
	}

	// Position around center
	const vec3 relViewPos = polarToCartesian(vec2(viewPolar))*
		viewPolar.z;

	viewPos = dvec3(relViewPos) + 
		planetStates[focusedPlanetId].getPosition();

	const vec3 direction = -polarToCartesian(vec2(viewPolar)+panPolar);

	viewDir = mat3(lookAt(vec3(0), direction, vec3(0,0,1)));

	// Time warping
	if (isPressedOnce(GLFW_KEY_K))
	{
		if (timeWarpIndex > 0) timeWarpIndex--;
	}
	if (isPressedOnce(GLFW_KEY_L))
	{
		if (timeWarpIndex < timeWarpValues.size()-1) timeWarpIndex++;
	}

	// Switching
	if (isPressedOnce(GLFW_KEY_TAB))
	{
		switchPhase = SwitchPhase::TRACK;
		// Save previous planet
		switchPreviousPlanet = focusedPlanetId;
		// Choose next planet
		const int nextPlanet = focusedPlanetId+
		(glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)?-1:1);
		focusedPlanetId = (nextPlanet>=(int)planetCount)?0:
			((nextPlanet<0?(planetCount-1):nextPlanet));
		// Kill timewarp
		timeWarpIndex = 0;
		// Save previous orientation
		switchPreviousViewDir = viewDir;
		// Ray test
		switchNewViewPolar = viewPolar;
		// Get direction from view to target body
		const dvec3 target = planetStates[focusedPlanetId].getPosition() - 
				planetStates[switchPreviousPlanet].getPosition();
		const vec3 targetDir = normalize(target-dvec3(relViewPos));
		// Get t as origin+dir*t = closest point to body
		const float b = dot(relViewPos, targetDir);
		// Dont care if behind view
		if (b < 0)
		{
			// Get closest point coordinates
			const vec3 closestPoint = relViewPos-b*targetDir;
			// Get compare closest distance with radius of body
			const float closestDist = length(closestPoint);
			const float closestMinDist = radius*1.1;
			if (closestDist < closestMinDist)
			{
				// Vector to shift view to not have obstructed target body
				const vec3 tangent = normalize(closestPoint);
				// Thales to get amount to shift
				const double totalDist = length(target-dvec3(relViewPos));
				const double targetClosestDist = length(target-dvec3(tangent*closestMinDist));
				const double tangentCoef = totalDist*(closestMinDist-closestDist)/targetClosestDist;
				// New cartesian position
				const glm::vec3 newRelPos = polarToCartesian(vec2(viewPolar))*viewPolar.z + 
					vec3(tangentCoef*tangent);
				// Convert to polar coordinates & set as interpolation target
				const float newDist = length(newRelPos);
				const glm::vec3 newRelDir = - normalize(newRelPos);
				switchNewViewPolar = vec3(
					atan2(-newRelDir.y, -newRelDir.x), asin(-newRelDir.z), newDist);
			}
		}
	}
}

float ease(float t)
{
	return 6*t*t*t*t*t-15*t*t*t*t+10*t*t*t;
}

float ease2(float t, float alpha)
{
	float a = pow(t, alpha);
	return a/(a+pow(1-t, alpha));
}

void Game::updateTrack(float dt)
{
	const float totalTime = 1.0;
	const float t = min(1.f, switchTime/totalTime);
	const float f = ease(t);

	// Interpolate positions
	float posDeltaTheta = switchNewViewPolar.x-viewPolar.x;
	if (posDeltaTheta < -pi<float>()) posDeltaTheta += 2*pi<float>();
	else if (posDeltaTheta > pi<float>()) posDeltaTheta -= 2*pi<float>();

	const vec3 interpPolar = (1-f)*viewPolar+f*
		vec3(viewPolar.x+posDeltaTheta, switchNewViewPolar.y, switchNewViewPolar.z);

	viewPos = planetStates[switchPreviousPlanet].getPosition()+
		dvec3(polarToCartesian(vec2(interpPolar))*interpPolar.z);

	// Aim at next body
	const vec3 targetDir = 
		normalize(planetStates[focusedPlanetId].getPosition() - viewPos);
	// Find the angles
	const float targetPhi = asin(targetDir.z);
	const float targetTheta = atan2(targetDir.y, targetDir.x);

	// Find the angles of original direction
	const vec3 sourceDir = -(transpose(switchPreviousViewDir)[2]);
	const float sourcePhi = asin(sourceDir.z);
	const float sourceTheta = atan2(sourceDir.y, sourceDir.x);

	// Wrap around theta
	float deltaTheta = targetTheta-sourceTheta;
	if (deltaTheta < -pi<float>()+0.001) deltaTheta += 2*pi<float>();
	else if (deltaTheta > pi<float>()-0.001) deltaTheta -= 2*pi<float>();

	// Interpolate angles
	const float phi = f*targetPhi+(1-f)*sourcePhi;
	const float theta = f*(sourceTheta+deltaTheta)+(1-f)*sourceTheta;

	// Reconstruct direction from angles
	const vec3 dir = polarToCartesian(vec2(theta, phi));
	viewDir = lookAt(vec3(0), dir, vec3(0,0,1));

	switchTime += dt;
	if (switchTime > totalTime)
	{
		switchPhase = SwitchPhase::MOVE;
		switchTime = 0.f;
		viewPolar = interpPolar;
	}
}

void Game::updateMove(float dt)
{
	const float totalTime = 1.0;
	const float t = min(1.f, switchTime/totalTime);
	const double f = ease2(t, 4);

	// Old position to move from
	const dvec3 sourcePos = planetStates[switchPreviousPlanet].getPosition()+
		dvec3(polarToCartesian(vec2(viewPolar))*viewPolar.z);

	// Distance from planet at arrival
	const float targetDist = 4*planetParams[focusedPlanetId].getBody().getRadius();
	// Direction from old position to new planet
	const glm::vec3 direction = 
		normalize(planetStates[focusedPlanetId].getPosition()-sourcePos);
	// New position (subtract direction to not be inside planet)
	const dvec3 targetPos = planetStates[focusedPlanetId].getPosition()-
		dvec3(direction*targetDist);

	// Interpolate positions
	viewPos = f*targetPos+(1-f)*sourcePos;
	viewDir = lookAt(vec3(0), direction, vec3(0,0,1));

	switchTime += dt;
	if (switchTime > totalTime)
	{
		switchPhase = SwitchPhase::IDLE;
		switchTime = 0.f;
		// Reconstruct new polar angles from direction
		viewPolar = vec3(
			atan2(-direction.y, -direction.x), asin(-direction.z), targetDist);
		panPolar = vec2(0);
		viewSpeed = vec3(0);
	}
}

int Game::getParent(size_t planetId)
{
	return planetParents[planetId];
}

std::vector<size_t> Game::getAllParents(size_t planetId)
{
	std::vector<size_t> parents = {};
	int temp = planetId;
	int tempParent = -1;
	while ((tempParent = getParent(temp)) != -1)
	{
		parents.push_back(tempParent);
		temp = tempParent;
	}
	return parents;
}

int Game::getLevel(size_t planetId)
{
	int level = 0;
	int temp = planetId;
	int tempParent = -1;
	while ((tempParent = getParent(temp)) != -1)
	{
		level += 1;
		temp = tempParent;
	}
	return level;
}

std::vector<size_t> Game::getChildren(size_t planetId)
{
	std::vector<size_t> children;
	for (size_t i=0;i<planetParents.size();++i)
	{
		if (getParent(i) == (int)planetId) children.push_back(i);
	}
	return children;
}

std::vector<size_t> Game::getAllChildren(size_t planetId)
{
	auto c = getChildren(planetId);
	std::vector<size_t> accum = {};
	for (auto i : c)
	{
		auto cc = getAllChildren(i);
		accum.insert(accum.end(), cc.begin(), cc.end());
	}
	c.insert(c.end(), accum.begin(), accum.end());
	return c;
}

std::vector<size_t> Game::getFocusedPlanets(size_t focusedPlanetId)
{
	int level = getLevel(focusedPlanetId);

	// Itself visible
	std::vector<size_t> v = {focusedPlanetId};
	// All children visible
	auto children = getAllChildren(focusedPlanetId);
	v.insert(v.end(), children.begin(), children.end());

	// All parents visible
	auto parents = getAllParents(focusedPlanetId);
	v.insert(v.end(), parents.begin(), parents.end());

	// If it is a moon, siblings are visible
	if (level >= 2)
	{
		auto siblings = getAllChildren(getParent(focusedPlanetId));
		v.insert(v.end(), siblings.begin(), siblings.end());
	}
	return v;
}

std::string generateScreenshotName()
{
	time_t t = time(0);
	struct tm *now = localtime(&t);
	std::stringstream filenameBuilder;
	filenameBuilder << 
		"./screenshots/screenshot_" << 
		(now->tm_year+1900) << "-" << 
		(now->tm_mon+1) << "-" << 
		(now->tm_mday) << "_" << 
		(now->tm_hour) << "-" << 
		(now->tm_min) << "-" << 
		(now->tm_sec) << ".png";
	return filenameBuilder.str();
}

void Game::displayProfiling(const std::vector<std::pair<std::string, uint64_t>> &a)
{
	// First entry is full time of frame
	uint64_t full = a[0].second;
	// Compute which label has the largest width
	size_t largestName = 0;
	for (auto p : a)
	{
		if (p.first.size() > largestName) largestName = p.first.size();
	}
	// Display each entry
	for (auto p : a)
	{
		std::cout.width(largestName);
		std::cout << std::left << p.first;
		uint64_t nano = p.second;
		double percent = 100*nano/(double)full;
		double fps = 1E9/(double)nano;
		double micro = nano/1E6;
		// If entry is full time, display fps instead of percentage of frame
		if (nano == full)
			std::cout << "  " << micro << "ms (" << fps << "FPS)" << std::endl;
		else
			std::cout << "  " << micro << "ms (" << percent << "%)" << std::endl;
	}
	std::cout << "-------------------------" << std::endl;
}

void Game::updateProfiling(const std::vector<std::pair<std::string, uint64_t>> &a)
{
	for (auto p : a)
	{
		// Full time update
		{
			auto it = std::find_if(fullTimes.begin(), fullTimes.end(), [&](std::pair<std::string, uint64_t> pa){
				return pa.first == p.first;
			});
			if (it == fullTimes.end()) fullTimes.push_back(p);
			else it->second += p.second;
		}

		// Max time update
		{
			auto it = std::find_if(maxTimes.begin(), maxTimes.end(), [&](std::pair<std::string, uint64_t> pa){
				return pa.first == p.first;
			});
			if (it == maxTimes.end()) maxTimes.push_back(p);
			else it->second = std::max(it->second, p.second);
		}
	}
	numFrames += 1;
}

std::vector<std::pair<std::string, uint64_t>> Game::computeAverage(
	const std::vector<std::pair<std::string, uint64_t>> &a, int frames)
{
	std::vector<std::pair<std::string, uint64_t>> result = a;
	for (auto &p : result)
	{
		p.second /= (float)frames;
	}
	return result;
}
