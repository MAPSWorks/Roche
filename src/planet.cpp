#include "planet.hpp"

#include <fstream>
#include <string>
#include <algorithm>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "thirdparty/shaun/sweeper.hpp"
#include <glm/ext.hpp>

Planet::Orbit::Orbit(
	const double ecc,
	const double sma,
	const double inc,
	const double lan,
	const double arg,
	const double m0) :
	_ecc{ecc},
	_sma{sma},
	_inc{inc},
	_lan{lan},
	_arg{arg},
	_m0{m0}
{

}

static double meanToEccentric(const double mean, const double ecc)
{
	// Newton to find eccentric anomaly (En)
	double En = (ecc<0.8)?mean:glm::pi<float>(); // Starting value of En
	const int it = 20; // Number of iterations
	for (int i=0;i<it;++i)
		En -= (En - ecc*sin(En)-mean)/(1-ecc*cos(En));
	return En;
}

glm::dvec3 Planet::Orbit::computePosition(
	const double epoch, 
	const double parentGM) const
{
	// Mean Anomaly compute
	const double orbitalPeriod = 2*glm::pi<float>()*sqrt(
		(_sma*_sma*_sma)/parentGM);
	const double meanMotion = 2*glm::pi<float>()/orbitalPeriod;
	const double meanAnomaly = fmod(epoch*meanMotion + _m0, 2*glm::pi<float>());
	// Mean anomaly to Eccentric
	const double En = meanToEccentric(meanAnomaly, _ecc);
	// Eccentric anomaly to True anomaly
	const double trueAnomaly = 2*atan2(sqrt(1+_ecc)*sin(En/2), sqrt(1-_ecc)*cos(En/2));
	// Distance from parent body
	const double dist = _sma*((1-_ecc*_ecc)/(1+_ecc*cos(trueAnomaly)));
	// Plane changes
	const glm::dvec3 posInPlane = glm::dvec3(
		-sin(trueAnomaly)*dist,
		cos(trueAnomaly)*dist,
		0.0);
	const glm::dquat q =
		  glm::rotate(glm::dquat(), _lan, glm::dvec3(0,0,1))
		* glm::rotate(glm::dquat(), _inc, glm::dvec3(0,1,0))
		* glm::rotate(glm::dquat(), _arg, glm::dvec3(0,0,1));
	return q*posInPlane;
}

Planet::Atmo::Atmo(
	const glm::vec4 K,
	const float density,
	const float maxHeight,
	const float scaleHeight) :
	_K{K},
	_density{density},
	_maxHeight{maxHeight},
	_scaleHeight{scaleHeight}
{

}

static float scatDensity(const float p, const float scaleHeight)
{
	return glm::exp(-std::max(0.f, p)/scaleHeight);
}

static float scatDensity(const glm::vec2 p, const float radius, const float scaleHeight)
{
	return scatDensity(glm::length(p) - radius, scaleHeight);
}

static float scatOptic(const glm::vec2 a, const glm::vec2 b, 
	const float radius, const float scaleHeight, const float maxHeight, const int samples)
{
	const glm::vec2 step = (b-a)/(float)samples;
	glm::vec2 v = a+step*0.5f;

	float sum = 0.f;
	for (int i=0;i<samples;++i)
	{
		sum += scatDensity(v, radius, scaleHeight);
		v += step;
	}
	return sum * glm::length(step) / maxHeight;
}

static glm::vec2 intersectsSphere(
	const glm::vec2 ori, 
	const glm::vec2 dir, 
	const float radius)
{
	const float b = glm::dot(ori,dir);
	const float c = glm::dot(ori,ori)-radius*radius;
	const float d = b*b-c;
	if (d < 0) return glm::vec2(
		+std::numeric_limits<float>::infinity(), 
		-std::numeric_limits<float>::infinity());
	const float e = sqrt(d);
	return glm::vec2(-b-e,-b+e);
}

std::vector<float> Planet::Atmo::generateLookupTable(
	const size_t size,
	const float radius) const
{
	/* 2 channel lookup table :
	 * y-axis for altitude (0.0 for sea level, 1.0 for maxHeight)
	 * x-axis for cosine of angle of ray
	 * First channel for air density
	 * Second channel for out scattering factor
	 */
	std::vector<float> table(size*size*2);

	size_t index = 0;
	for (size_t i=0;i<size;++i)
	{
		const float altitude = (float)i/(float)size * _maxHeight;
		const float density = glm::exp(-altitude/_scaleHeight);
		for (size_t j=0;j<size;++j)
		{
			const float angle = acos(2*(float)j/(float)(size-1)-1);
			const glm::vec2 rayDir = glm::vec2(sin(angle), cos(angle));
			const glm::vec2 rayOri = glm::vec2(0, radius + altitude);
			const float t = intersectsSphere(rayOri, rayDir, radius+_maxHeight).y;
			const glm::vec2 u = rayOri + rayDir*t;
			const float depth = scatOptic(rayOri, u, radius, _scaleHeight, _maxHeight, 50);
			table[index+0] = density;
			table[index+1] = depth;
			index += 2;
		}
	}
	return table;
}

glm::vec4 Planet::Atmo::getScatteringConstant() const
{
	return _K;
}

float Planet::Atmo::getDensity() const
{
	return _density;
}

float Planet::Atmo::getMaxHeight() const
{
	return _maxHeight;
}

float Planet::Atmo::getScaleHeight() const
{
	return _scaleHeight;
}

Planet::Ring::Ring(
	const float innerDistance,
	const float outerDistance,
	const glm::vec3 normal,
	const std::string &backscatFilename,
	const std::string &forwardscatFilename,
	const std::string &unlitFilename,
	const std::string &transparencyFilename,
	const std::string &colorFilename) :
	_innerDistance{innerDistance},
	_outerDistance{outerDistance},
	_normal{glm::normalize(normal)},
	_backscatFilename{backscatFilename},
	_forwardscatFilename{forwardscatFilename},
	_unlitFilename{unlitFilename},
	_transparencyFilename{transparencyFilename},
	_colorFilename{colorFilename}
{

}

std::vector<float> Planet::Ring::loadFile(
	const std::string &filename) const
{
	std::ifstream in(filename);

	if (!in)
	{
		throw std::runtime_error("Can't open ring file " + filename);
	}

	// Clear values
	std::vector<float> pixelData;
	in.seekg(0, std::ios::beg);

	std::string number = "";

	while (!in.eof())
	{
		char c;
		in.read(&c, 1);
		if (c == ' ' || c == '\t' || c == '\n')
		{
			if (number.size() > 0)
			{
				pixelData.push_back(std::stof(number.c_str()));
				number = "";
			}
		}
		else
		{
			number += c;
		}
	}
	return pixelData;
}

float Planet::Ring::getInnerDistance() const
{
	return _innerDistance;
}

float Planet::Ring::getOuterDistance() const
{
	return _outerDistance;
}

glm::vec3 Planet::Ring::getNormal() const
{
	return _normal;
}

std::string Planet::Ring::getBackscatFilename() const
{
	return _backscatFilename;
}

std::string Planet::Ring::getForwardscatFilename() const
{
	return _forwardscatFilename;
}

std::string Planet::Ring::getUnlitFilename() const
{
	return _unlitFilename;
}

std::string Planet::Ring::getTransparencyFilename() const
{
	return _transparencyFilename;
}

std::string Planet::Ring::getColorFilename() const
{
	return _colorFilename;
}

Planet::Body::Body(
	const float radius,
	const double GM,
	const glm::vec3 rotAxis,
	const float rotPeriod,
	const glm::vec3 meanColor,
	const std::string &diffuseFilename) :
	_rotAxis{glm::normalize(rotAxis)},
	_rotPeriod{rotPeriod},
	_meanColor{meanColor},
	_radius{radius},
	_GM{GM},
	_diffuseFilename{diffuseFilename}
{

}

glm::vec3 Planet::Body::getRotationAxis() const
{
	return _rotAxis;
}

float Planet::Body::getRotationPeriod() const
{
	return _rotPeriod;
}

glm::vec3 Planet::Body::getMeanColor() const
{
	return _meanColor;
}

float Planet::Body::getRadius() const
{
	return _radius;
}

double Planet::Body::getGM() const
{
	return _GM;
}

std::string Planet::Body::getDiffuseFilename() const
{
	return _diffuseFilename;
}

Planet::Star::Star(const float brightness,
	const float flareFadeInStart, const float flareFadeInEnd,
	const float flareAttenuation, const float flareMinSize,
	const float flareMaxSize) : 
	_brightness{brightness},
	_flareFadeInStart{flareFadeInStart},
	_flareFadeInEnd{flareFadeInEnd},
	_flareAttenuation{flareAttenuation},
	_flareMinSize{flareMinSize},
	_flareMaxSize{flareMaxSize}
{

}

float Planet::Star::getBrightness() const
{
	return _brightness;
}

float Planet::Star::getFlareFadeInStart() const
{
	return _flareFadeInStart;
}

float Planet::Star::getFlareFadeInEnd() const
{
	return _flareFadeInEnd;
}

float Planet::Star::getFlareAttenuation() const
{
	return _flareAttenuation;
}

float Planet::Star::getFlareMinSize() const
{
	return _flareMinSize;
}

float Planet::Star::getFlareMaxSize() const
{
	return _flareMaxSize;
}

Planet::Clouds::Clouds(const std::string &filename, const float period) :
	_filename{filename},
	_period{period}
{

}

std::string Planet::Clouds::getFilename() const
{
	return _filename;
}

float Planet::Clouds::getPeriod() const
{
	return _period;
}

Planet::Night::Night(const std::string &filename,
	const float intensity) :
	_filename{filename},
	_intensity{intensity}
{

}

std::string Planet::Night::getFilename() const
{
	return _filename;
}

float Planet::Night::getIntensity() const
{
	return _intensity;
}

Planet::Specular::Specular(const std::string &filename,
	const Mask mask0, const Mask mask1) :
	_filename{filename},
	_mask0{mask0},
	_mask1{mask1}
{

}

Planet::Specular::Mask Planet::Specular::getMask0() const
{
	return _mask0;
}

Planet::Specular::Mask Planet::Specular::getMask1() const
{
	return _mask1;
}

std::string Planet::Specular::getFilename() const
{
	return _filename;
}

void Planet::setName(const std::string &name)
{
	_name = name;
}

void Planet::setParentName(const std::string &name)
{
	_parentName = name;
}

void Planet::setBody(const Body &body)
{
	_body = body;
}

void Planet::setOrbit(const Orbit &orbit)
{
	_orbit = std::make_pair(true, orbit);
}

void Planet::setAtmo(const Atmo &atmo)
{
	_atmo = std::make_pair(true, atmo);
}

void Planet::setRing(const Ring &ring)
{
	_ring = std::make_pair(true, ring);
}

void Planet::setStar(const Star &star)
{
	_star = std::make_pair(true, star);
}

void Planet::setClouds(const Clouds &clouds)
{
	_clouds = std::make_pair(true, clouds);
}

void Planet::setNight(const Night &night)
{
	_night = std::make_pair(true, night);
}

void Planet::setSpecular(const Specular &specular)
{
	_specular = std::make_pair(true, specular);
}

bool Planet::hasOrbit() const
{
	return _orbit.first;
}

bool Planet::hasAtmo() const
{
	return _atmo.first;
}

bool Planet::hasRing() const
{
	return _ring.first;
}

bool Planet::isStar() const
{
	return _star.first;
}

bool Planet::hasClouds() const
{
	return _clouds.first;
}

bool Planet::hasNight() const
{
	return _night.first;
}

bool Planet::hasSpecular() const
{
	return _specular.first;
}

std::string Planet::getName() const
{
	return _name;
}

std::string Planet::getParentName() const
{
	return _parentName;
}

const Planet::Body &Planet::getBody() const
{
	return _body;
}

const Planet::Orbit &Planet::getOrbit() const
{
	return _orbit.second;
}

const Planet::Atmo &Planet::getAtmo() const
{
	return _atmo.second;
}

const Planet::Ring &Planet::getRing() const
{
	return _ring.second;
}

const Planet::Star &Planet::getStar() const
{
	return _star.second;
}

const Planet::Clouds &Planet::getClouds() const
{
	return _clouds.second;
}

const Planet::Night &Planet::getNight() const
{
	return _night.second;
}

const Planet::Specular &Planet::getSpecular() const
{
	return _specular.second;
}

PlanetState::PlanetState(
	const glm::dvec3 &pos, float rotationAngle, float cloudDisp) :
	_position{pos},
	_rotationAngle{rotationAngle},
	_cloudDisp{cloudDisp}
{

}

glm::dvec3 PlanetState::getPosition() const
{
	return _position;
}

float PlanetState::getRotationAngle() const
{
	return _rotationAngle;
}

float PlanetState::getCloudDisp() const
{
	return _cloudDisp;
}
