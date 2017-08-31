#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <ctime>

#include "renderer.hpp"
#include "renderer_gl.hpp"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"

#include <glm/ext.hpp>

using namespace glm;
using namespace std;

string generateScreenshotName();

Game::Game()
{
	_renderer.reset(new RendererGL());
}

Game::~Game()
{
	_renderer->destroy();

	glfwTerminate();
}

void Game::loadSettingsFile()
{
	try 
	{
		shaun::object obj = shaun::parse_file("config/settings.sn");
		shaun::sweeper swp(obj);

		shaun::sweeper video(swp("video"));
		auto fs = video("fullscreen");
		_fullscreen = (fs.is_null())?true:(bool)fs.value<shaun::boolean>();

		if (!_fullscreen)
		{
			_width = video("width").value<shaun::number>();
			_height = video("height").value<shaun::number>();
		}

		shaun::sweeper graphics(swp("graphics"));
		_maxTexSize = graphics("maxTexSize").value<shaun::number>();
		_msaaSamples = graphics("msaaSamples").value<shaun::number>();
		_syncTexLoading = graphics("syncTexLoading").value<shaun::boolean>();

		shaun::sweeper controls(swp("controls"));
		_sensitivity = controls("sensitivity").value<shaun::number>();
	} 
	catch (const shaun::exception &e)
	{
		throw runtime_error("Error when parsing settings file :\n" + e.to_string());
	}
}

void Game::scrollFun(int offset)
{
	if (_switchPhase == SwitchPhase::IDLE)
	{
		// FOV zoom/unzoom when alt key held
		if (glfwGetKey(_win, GLFW_KEY_LEFT_ALT))
		{
			_viewFovy = clamp(_viewFovy*pow(0.5f, 
				(float)offset*_sensitivity*100),
				radians(0.1f), radians(40.f));
		}
		// Exposure +/-
		else if (glfwGetKey(_win, GLFW_KEY_LEFT_CONTROL))
		{
			_exposure = clamp(_exposure+0.1f*offset, -4.f, 4.f);
		}
		// Distance zoom/unzoom
		else
		{
			_viewSpeed.z -= 40*offset*_sensitivity;
		}
	}
}

void Game::init()
{
	loadSettingsFile();
	loadEntityFiles();

	_viewPolar.z = getFocusedBody().getParam().getModel().getRadius()*4;

	// Window & context creation
	if (!glfwInit())
		throw runtime_error("Can't init GLFW");

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
	_renderer->windowHints();

	if (_fullscreen)
	{
		_width = mode->width;
		_height = mode->height;
	}
	_win = glfwCreateWindow(_width, _height, "Roche", 
		_fullscreen?monitor:nullptr, 
		nullptr);

	glfwSetWindowUserPointer(_win, this);

	glfwSetScrollCallback(_win, [](GLFWwindow* win, double, double yoffset){
		((Game*)glfwGetWindowUserPointer(win))->scrollFun(yoffset);
	});

	if (!_win)
	{
		glfwTerminate();
		throw runtime_error("Can't open window");
	}
	glfwMakeContextCurrent(_win);

	glewExperimental = true;
	const GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		throw runtime_error("Can't initialize GLEW : " + string((const char*)glewGetErrorString(err)));
	}

	// Set _epoch as current time (get time since 1970 + adjust for 2017)
	_epoch = (long)time(NULL) - 1483228800;

	// Renderer init
	_renderer->init({
		&_entityCollection, 
		_starMapFilename, 
		_starMapIntensity, 
		_msaaSamples, 
		_maxTexSize, 
		_syncTexLoading, 
		_width, _height});
}

template<class T>
T get(shaun::sweeper swp);

template <>
double get(shaun::sweeper swp)
{
	if (swp.is_null()) return 0.0; else return swp.value<shaun::number>();
}

template <>
string get(shaun::sweeper swp)
{
	if (swp.is_null()) return ""; else return swp.value<shaun::string>();
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

Orbit parseOrbit(shaun::sweeper &swp)
{
	return Orbit(
		get<double>(swp("ecc")),
		get<double>(swp("sma")),
		radians(get<double>(swp("inc"))),
		radians(get<double>(swp("lan"))),
		radians(get<double>(swp("arg"))),
		get<double>(swp("pr")),
		radians(get<double>(swp("m0"))));
}

Model parseModel(shaun::sweeper &modelsw, const mat3 &axialMat)
{
	return Model(
		get<double>(modelsw("radius")),
		get<double>(modelsw("GM")),
		axialMat*
		axis(
			radians(get<double>(modelsw("rightAscension"))),
			radians(get<double>(modelsw("declination")))),
		get<double>(modelsw("rotPeriod")),
		get<vec3>(modelsw("meanColor"))*
		(float)get<double>(modelsw("albedo")),
		get<string>(modelsw("diffuse")));
}

Atmo parseAtmo(shaun::sweeper &atmosw)
{
	return Atmo(
		get<vec4>(atmosw("K")),
		get<double>(atmosw("density")),
		get<double>(atmosw("maxHeight")),
		get<double>(atmosw("scaleHeight")));
}

Ring parseRing(shaun::sweeper &ringsw, const mat3 &axialMat)
{
	return Ring(
		get<double>(ringsw("inner")),
		get<double>(ringsw("outer")),
		axialMat*
		axis(
			radians(get<double>(ringsw("rightAscension"))),
			radians(get<double>(ringsw("declination")))),
		get<string>(ringsw("backscat")),
		get<string>(ringsw("forwardscat")),
		get<string>(ringsw("unlit")),
		get<string>(ringsw("transparency")),
		get<string>(ringsw("color")));
}

Star parseStar(shaun::sweeper &starsw)
{
	return Star(
		get<double>(starsw("brightness")),
		get<double>(starsw("flareFadeInStart")),
		get<double>(starsw("flareFadeInEnd")),
		get<double>(starsw("flareAttenuation")),
		get<double>(starsw("flareMinSize")),
		get<double>(starsw("flareMaxSize")));
}

Clouds parseClouds(shaun::sweeper &cloudssw)
{
	return Clouds(
		get<string>(cloudssw("filename")),
		get<double>(cloudssw("period")));
}

Night parseNight(shaun::sweeper &nightsw)
{
	return Night(
		get<string>(nightsw("filename")),
		get<double>(nightsw("intensity")));
}

Specular parseSpecular(shaun::sweeper &specsw)
{
	shaun::sweeper mask0(specsw("mask0"));
	shaun::sweeper mask1(specsw("mask1"));
	return Specular(
		get<string>(specsw("filename")),
		{get<vec3>(mask0("color")), 
		 (float)get<double>(mask0("hardness"))},
		{get<vec3>(mask1("color")),
		 (float)get<double>(mask1("hardness"))});
}

void Game::loadEntityFiles()
{
	try
	{
		shaun::object obj = shaun::parse_file("config/entities.sn");
		shaun::sweeper swp(obj);

		_ambientColor = (float)get<double>(swp("ambientColor"));
		string startingBody = swp("startingBody").value<shaun::string>();

		shaun::sweeper starMap(swp("starMap"));
		_starMapFilename = get<string>(starMap("diffuse"));
		_starMapIntensity = (float)get<double>(starMap("intensity"));

		const float axialTilt = radians(get<double>(swp("axialTilt")));
		const mat3 axialMat = mat3(rotate(mat4(), axialTilt, vec3(0,-1,0)));

		vector<EntityParam> entities;

		shaun::sweeper barycenterSw(swp("barycenters"));
		for (int i=0;i<(int)barycenterSw.size();++i)
		{
			shaun::sweeper bc(barycenterSw[i]);
			EntityParam entity;
			entity.setName(bc("name").value<shaun::string>());
			entity.setParentName(get<string>(bc("parent")));

			shaun::sweeper orbitsw(bc("orbit"));
			if (!orbitsw.is_null())
			{
				entity.setOrbit(parseOrbit(orbitsw));
			}
			entities.push_back(entity);
		}

		shaun::sweeper bodySweeper(swp("bodies"));

		for (int i=0;i<(int)bodySweeper.size();++i)
		{
			shaun::sweeper bd(bodySweeper[i]);
			string name = bd("name").value<shaun::string>();
			// Create entity
			EntityParam entity;
			entity.setName(name);
			const string displayName = get<string>(bd("displayName"));
			entity.setDisplayName(displayName==""?name:displayName);
			entity.setParentName(get<string>(bd("parent")));

			shaun::sweeper orbitsw(bd("orbit"));
			if (!orbitsw.is_null())
			{
				entity.setOrbit(parseOrbit(orbitsw));
			}
			shaun::sweeper modelsw(bd("model"));
			if (!modelsw.is_null())
			{
				entity.setModel(parseModel(modelsw, axialMat));
			}
			shaun::sweeper atmosw(bd("atmo"));
			if (!atmosw.is_null())
			{
				entity.setAtmo(parseAtmo(atmosw));
			}
			shaun::sweeper ringsw(bd("ring"));
			if (!ringsw.is_null())
			{
				entity.setRing(parseRing(ringsw, axialMat));
			}
			shaun::sweeper starsw(bd("star"));
			if (!starsw.is_null())
			{
				entity.setStar(parseStar(starsw));
			}
			shaun::sweeper cloudssw(bd("clouds"));
			if (!cloudssw.is_null())
			{
				entity.setClouds(parseClouds(cloudssw));
			}
			shaun::sweeper nightsw(bd("night"));
			if (!nightsw.is_null())
			{
				entity.setNight(parseNight(nightsw));
			}
			shaun::sweeper specsw(bd("specular"));
			if (!specsw.is_null())
			{
				entity.setSpecular(parseSpecular(specsw));
			}
			entities.push_back(entity);
		}
		_entityCollection.init(entities);

		// Set focused body
		for (int i=0;i<(int)_entityCollection.getBodies().size();++i)
		{
			if (_entityCollection.getBodies()[i].getParam().getName() == startingBody)
			{
				_focusedBodyId = i;
				break;
			}
		}
	}
	catch (const shaun::exception &e)
	{
		throw runtime_error("Error when parsing entity file :\n" + e.to_string());
	}
}

bool Game::isPressedOnce(const int key)
{
	if (glfwGetKey(_win, key))
	{
		if (_keysHeld[key]) return false;
		else return (_keysHeld[key] = true);
	}
	else
	{
		return (_keysHeld[key] = false);
	}
}

vec3 polarToCartesian(const vec2 &p)
{
	return vec3(
		cos(p.x)*cos(p.y), 
		sin(p.x)*cos(p.y), 
		sin(p.y));
}

string format(int value)
{
	return string(1, (char)('0'+(value/10))) +
		string(1, (char)('0'+(value%10)));
}

bool isLeapYear(int year)
{
	return ((year%4==0) && (year%100!=0)) || (year%400==0);
}

string getFormattedTime(long _epochInSeconds)
{
	const int seconds = _epochInSeconds%60;
	const int minutes = (_epochInSeconds/60)%60;
	const int hours = (_epochInSeconds/3600)%24;
	const int days = _epochInSeconds/86400;

	int year = 2017;
	int i = 0;
	while (true)
	{
		const int daysInYear = 365+((isLeapYear(year))?1:0);
		if (i+daysInYear <= days)
		{
			i += daysInYear;
			year += 1;
		}
		else break;
	}

	int remainingDays = days-i;

	vector<int> monthLength = {
		31,28+(isLeapYear(year)?1:0), 31,
		30, 31, 30,
		31, 31, 30,
		31, 30, 31};

	int j=0;
	int month = 0;
	while (true)
	{
		const int daysInMonth = monthLength[month];
		if (j+daysInMonth <= remainingDays)
		{
			j += daysInMonth;
			month += 1;
		}
		else break;
	}

	vector<string> monthNames = {
		"Jan", "Feb", "Mar", 
		"Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", 
		"Oct", "Nov", "Dec"};

	return monthNames[month] + ". " + 
		to_string(remainingDays-j+1) + " " + 
		to_string(year) + " " + 
		format(hours) + ':' + 
		format(minutes) + ':' +
		format(seconds) + " UTC";
}

void Game::update(const double dt)
{
	_epoch += _timeWarpValues[_timeWarpIndex]*dt;

	map<EntityHandle, dvec3> relativePositions;
	// Entity state update
	for (const auto &h : _entityCollection.getAll())
	{
		relativePositions[h] = (!h.getParent().exists() || !h.getParam().hasOrbit())?
			dvec3(0.0):
			h.getParam().getOrbit().computePosition(_epoch);
	}

	map<EntityHandle, EntityState> state;

	// Entity absolute position update
	for (auto h : _entityCollection.getAll())
	{
		dvec3 absPosition = relativePositions[h];
		auto parent = h.getParent();
		while (parent.exists())
		{
			absPosition += relativePositions[parent];
			parent = parent.getParent();
		}

		// Entity Angle
		const float rotationAngle = 
			(2.0*pi<float>())*
			fmod(_epoch/h.getParam().getModel().getRotationPeriod(),1.f);

		// Cloud Displacement
		const float cloudDisp = [&]{
			if (h.getParam().hasClouds()) return 0.0;
			const float period = h.getParam().getClouds().getPeriod();
			return (period)?fmod(-_epoch/period, 1.f):0.f;
		}();

		state[h] = EntityState(absPosition, rotationAngle, cloudDisp);
	}

	_entityCollection.setState(state);
	
	// Wireframe on/off
	if (isPressedOnce(GLFW_KEY_W))
	{
		_wireframe = !_wireframe;
	}

	// Bloom on/off
	if (isPressedOnce(GLFW_KEY_B))
	{
		_bloom = !_bloom;
	}

	// Mouse move
	double posX, posY;
	glfwGetCursorPos(_win, &posX, &posY);

	if (_switchPhase == SwitchPhase::IDLE)
	{
		updateIdle(dt, posX, posY);
	}
	else if (_switchPhase == SwitchPhase::TRACK)
	{
		updateTrack(dt);
	}
	else if (_switchPhase == SwitchPhase::MOVE)
	{
		updateMove(dt);
	}

	// Mouse reset
	_preMousePosX = posX;
	_preMousePosY = posY;

	// Screenshot
	if (isPressedOnce(GLFW_KEY_F12))
	{
		_renderer->takeScreenshot(generateScreenshotName());
	}

	// Focused entities
	const vector<EntityHandle> texLoadBodies = 
		getTexLoadBodies(getFocusedBody());

	// Time formatting
	const long _epochInSeconds = floor(_epoch);
	const string formattedTime = getFormattedTime(_epochInSeconds);
		
	// Scene rendering
	_renderer->render({
		_viewPos, _viewFovy, _viewDir,
		_exposure, _ambientColor, _wireframe, _bloom, texLoadBodies, 
		getDisplayedBody().getParam().getDisplayName(),
		_bodyNameFade, formattedTime});

	auto a = _renderer->getProfilerTimes();

	updateProfiling(a);

	// Display profiler in console
	if (isPressedOnce(GLFW_KEY_F5) && !a.empty())
	{
		cout << "Current Frame: " << endl;
		displayProfiling(a);
		auto b = computeAverage(_fullTimes, _numFrames);
		cout << "Average: " << endl;
		displayProfiling(b);
		cout << "Max: " << endl;
		displayProfiling(_maxTimes);
	}

	glfwSwapBuffers(_win);
	glfwPollEvents();
}

bool Game::isRunning()
{
	return !glfwGetKey(_win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(_win);
}

EntityHandle Game::getFocusedBody()
{
	return _entityCollection.getBodies()[_focusedBodyId];
}

EntityHandle Game::getDisplayedBody()
{
	return _entityCollection.getBodies()[_bodyNameId];
}

EntityHandle Game::getPreviousBody()
{
	return _entityCollection.getBodies()[_switchPreviousBodyId];
}

int Game::chooseNextBody(bool direction)
{
	int id = _focusedBodyId+(direction?1:-1);
	int size = _entityCollection.getBodies().size();
	if (id < 0) id += size;
	else if (id >= size) id -= size;
	return id;
}

void Game::updateIdle(float dt, double posX, double posY)
{
	const vec2 move = {-posX+_preMousePosX, posY-_preMousePosY};

	const bool mouseButton1 = glfwGetMouseButton(_win, GLFW_MOUSE_BUTTON_1);
	const bool mouseButton2 = glfwGetMouseButton(_win, GLFW_MOUSE_BUTTON_2);

	if ((mouseButton1 || mouseButton2) && !_dragging)
	{
		_dragging = true;
	}
	else if (_dragging && !(mouseButton1 || mouseButton2))
	{
		_dragging = false;
	}

	// Drag view around
	if (_dragging)
	{
		if (mouseButton1)
		{	
			_viewSpeed.x += move.x*_sensitivity;
			_viewSpeed.y += move.y*_sensitivity;
			for (int i=0;i<2;++i)
			{
				if (_viewSpeed[i] > _maxViewSpeed) _viewSpeed[i] = _maxViewSpeed;
				if (_viewSpeed[i] < -_maxViewSpeed) _viewSpeed[i] = -_maxViewSpeed;
			}
		}
		else if (mouseButton2)
		{
			_panPolar += move*_sensitivity*_viewFovy;
		}
	}

	const float radius = getFocusedBody().getParam().getModel().getRadius();

	_viewPolar.x += _viewSpeed.x;
	_viewPolar.y += _viewSpeed.y;
	_viewPolar.z += _viewSpeed.z*glm::max(0.01f, _viewPolar.z-radius);

	_viewSpeed *= _viewSmoothness;

	const float maxVerticalAngle = pi<float>()/2 - 0.001;

	if (_viewPolar.y > maxVerticalAngle)
	{
		_viewPolar.y = maxVerticalAngle;
		_viewSpeed.y = 0;
	}
	if (_viewPolar.y < -maxVerticalAngle)
	{
		_viewPolar.y = -maxVerticalAngle;
		_viewSpeed.y = 0;
	}
	if (_viewPolar.z < radius) _viewPolar.z = radius;

	if (_viewPolar.y + _panPolar.y > maxVerticalAngle)
	{
		_panPolar.y = maxVerticalAngle - _viewPolar.y;
	}
	if (_viewPolar.y + _panPolar.y < -maxVerticalAngle)
	{
		_panPolar.y = -maxVerticalAngle - _viewPolar.y;
	}

	// Position around center
	const vec3 relViewPos = polarToCartesian(vec2(_viewPolar))*
		_viewPolar.z;

	_viewPos = dvec3(relViewPos) + getFocusedBody().getState().getPosition();

	const vec3 direction = -polarToCartesian(vec2(_viewPolar)+_panPolar);

	_viewDir = mat3(lookAt(vec3(0), direction, vec3(0,0,1)));

	// Time warping
	if (isPressedOnce(GLFW_KEY_K))
	{
		if (_timeWarpIndex > 0) _timeWarpIndex--;
	}
	if (isPressedOnce(GLFW_KEY_L))
	{
		if (_timeWarpIndex < (int)_timeWarpValues.size()-1) _timeWarpIndex++;
	}

	// Entity name display
	_bodyNameId = _focusedBodyId;
	_bodyNameFade = 1.f;

	// Switching
	if (isPressedOnce(GLFW_KEY_TAB))
	{
		_switchPhase = SwitchPhase::TRACK;
		// Save previous entity
		_switchPreviousBodyId = _focusedBodyId;
		// Choose next entity
		const int direction = !glfwGetKey(_win, GLFW_KEY_LEFT_SHIFT);
		_focusedBodyId = chooseNextBody(direction);
		// Kill timewarp
		_timeWarpIndex = 0;
		// Save previous orientation
		_switchPreviousViewDir = _viewDir;
		// Ray test
		_switchNewViewPolar = _viewPolar;
		// Get direction from view to target entity
		const dvec3 target = getFocusedBody().getState().getPosition() - 
				getPreviousBody().getState().getPosition();
		const vec3 targetDir = normalize(target-dvec3(relViewPos));
		// Get t as origin+dir*t = closest point to entity
		const float b = dot(relViewPos, targetDir);
		// Dont care if behind view
		if (b < 0)
		{
			// Get closest point coordinates
			const vec3 closestPoint = relViewPos-b*targetDir;
			// Get compare closest distance with radius of entity
			const float closestDist = length(closestPoint);
			const float closestMinDist = radius*1.1;
			if (closestDist < closestMinDist)
			{
				// Vector to shift view to not have obstructed target entity
				const vec3 tangent = normalize(closestPoint);
				// Thales to get amount to shift
				const double totalDist = length(target-dvec3(relViewPos));
				const double targetClosestDist = length(target-dvec3(tangent*closestMinDist));
				const double tangentCoef = totalDist*(closestMinDist-closestDist)/targetClosestDist;
				// New cartesian position
				const vec3 newRelPos = polarToCartesian(vec2(_viewPolar))*_viewPolar.z + 
					vec3((float)tangentCoef*tangent);
				// Convert to polar coordinates & set as interpolation target
				const float newDist = length(newRelPos);
				const vec3 newRelDir = - normalize(newRelPos);
				_switchNewViewPolar = vec3(
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
	const float t = glm::min(1.f, _switchTime/totalTime);
	const float f = ease(t);

	// Entity name display
	_bodyNameId = _switchPreviousBodyId;
	_bodyNameFade = clamp(1.f-t*2.f, 0.f, 1.f);

	// Interpolate positions
	float posDeltaTheta = _switchNewViewPolar.x-_viewPolar.x;
	if (posDeltaTheta < -pi<float>()) posDeltaTheta += 2*pi<float>();
	else if (posDeltaTheta > pi<float>()) posDeltaTheta -= 2*pi<float>();

	const vec3 interpPolar = (1-f)*_viewPolar+f*
		vec3(_viewPolar.x+posDeltaTheta, _switchNewViewPolar.y, _switchNewViewPolar.z);

	_viewPos = getPreviousBody().getState().getPosition()+
		dvec3(polarToCartesian(vec2(interpPolar))*interpPolar.z);

	// Aim at next entity
	const vec3 targetDir = 
		normalize(getFocusedBody().getState().getPosition() - _viewPos);
	// Find the angles
	const float targetPhi = asin(targetDir.z);
	const float targetTheta = atan2(targetDir.y, targetDir.x);

	// Find the angles of original direction
	const vec3 sourceDir = -(transpose(_switchPreviousViewDir)[2]);
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
	_viewDir = lookAt(vec3(0), dir, vec3(0,0,1));

	_switchTime += dt;
	if (_switchTime > totalTime)
	{
		_switchPhase = SwitchPhase::MOVE;
		_switchTime = 0.f;
		_viewPolar = interpPolar;
	}
}

void Game::updateMove(float dt)
{
	const float totalTime = 1.0;
	const float t = glm::min(1.f, _switchTime/totalTime);
	const double f = ease2(t, 4);

	// Entity name fade
	_bodyNameId = _focusedBodyId;
	_bodyNameFade = clamp((t-0.5f)*2.f, 0.f, 1.f);

	// Old position to move from
	const dvec3 sourcePos = getPreviousBody().getState().getPosition()+
		dvec3(polarToCartesian(vec2(_viewPolar))*_viewPolar.z);

	// Distance from entity at arrival
	const float targetDist = glm::max(
		4*getFocusedBody().getParam().getModel().getRadius(), 1000.f);
	// Direction from old position to new entity
	const vec3 direction = 
		normalize(getFocusedBody().getState().getPosition()-sourcePos);
	// New position (subtract direction to not be inside entity)
	const dvec3 targetPos = getFocusedBody().getState().getPosition()-
		dvec3(direction*targetDist);

	// Interpolate positions
	_viewPos = f*targetPos+(1-f)*sourcePos;
	_viewDir = lookAt(vec3(0), direction, vec3(0,0,1));

	_switchTime += dt;
	if (_switchTime > totalTime)
	{
		_switchPhase = SwitchPhase::IDLE;
		_switchTime = 0.f;
		// Reconstruct new polar angles from direction
		_viewPolar = vec3(
			atan2(-direction.y, -direction.x), asin(-direction.z), targetDist);
		_panPolar = vec2(0);
		_viewSpeed = vec3(0);
	}
}

vector<EntityHandle> Game::getTexLoadBodies(const EntityHandle &focusedEntity)
{
	// Itself visible
	vector<EntityHandle> v = {focusedEntity};

	// All parents visible
	auto parents = focusedEntity.getAllParents();
	v.insert(v.end(), parents.begin(), parents.end());

	// All siblings visible
	auto siblings = focusedEntity.getParent().getAllChildren();
	v.insert(v.end(), siblings.begin(), siblings.end());

	// Only select bodies
	v.erase(remove_if(v.begin(), v.end(), [](const EntityHandle &h){
		return !h.getParam().isBody();
	}), v.end());

	return v;
}

string generateScreenshotName()
{
	time_t t = time(0);
	struct tm *now = localtime(&t);
	stringstream filenameBuilder;
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

void Game::displayProfiling(const vector<pair<string, uint64_t>> &a)
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
		cout.width(largestName);
		cout << left << p.first;
		uint64_t nano = p.second;
		double percent = 100*nano/(double)full;
		double fps = 1E9/(double)nano;
		double micro = nano/1E6;
		// If entry is full time, display fps instead of percentage of frame
		if (nano == full)
			cout << "  " << micro << "ms (" << fps << "FPS)" << endl;
		else
			cout << "  " << micro << "ms (" << percent << "%)" << endl;
	}
	cout << "-------------------------" << endl;
}

void Game::updateProfiling(const vector<pair<string, uint64_t>> &a)
{
	for (auto p : a)
	{
		// Full time update
		{
			auto it = find_if(_fullTimes.begin(), _fullTimes.end(), [&](pair<string, uint64_t> pa){
				return pa.first == p.first;
			});
			if (it == _fullTimes.end()) _fullTimes.push_back(p);
			else it->second += p.second;
		}

		// Max time update
		{
			auto it = find_if(_maxTimes.begin(), _maxTimes.end(), [&](pair<string, uint64_t> pa){
				return pa.first == p.first;
			});
			if (it == _maxTimes.end()) _maxTimes.push_back(p);
			else it->second = glm::max(it->second, p.second);
		}
	}
	_numFrames += 1;
}

vector<pair<string, uint64_t>> Game::computeAverage(
	const vector<pair<string, uint64_t>> &a, int frames)
{
	vector<pair<string, uint64_t>> result = a;
	for (auto &p : result)
	{
		p.second /= (float)frames;
	}
	return result;
}
