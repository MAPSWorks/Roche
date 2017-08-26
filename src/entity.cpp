#include "entity.hpp"

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

Orbit::Orbit(
	const double ecc,
	const double sma,
	const double inc,
	const double lan,
	const double arg,
	const double pr,
	const double m0) :
	_ecc{ecc},
	_sma{sma},
	_inc{inc},
	_lan{lan},
	_arg{arg},
	_pr{pr},
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

glm::dvec3 Orbit::computePosition(
	const double epoch) const
{
	// Mean Anomaly compute
	const double meanMotion = 2*glm::pi<float>()/_pr;
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

Atmo::Atmo(
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

std::vector<float> Atmo::generateLookupTable(
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

glm::vec4 Atmo::getScatteringConstant() const
{
	return _K;
}

float Atmo::getDensity() const
{
	return _density;
}

float Atmo::getMaxHeight() const
{
	return _maxHeight;
}

float Atmo::getScaleHeight() const
{
	return _scaleHeight;
}

Ring::Ring(
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

std::vector<float> Ring::loadFile(
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

float Ring::getInnerDistance() const
{
	return _innerDistance;
}

float Ring::getOuterDistance() const
{
	return _outerDistance;
}

glm::vec3 Ring::getNormal() const
{
	return _normal;
}

std::string Ring::getBackscatFilename() const
{
	return _backscatFilename;
}

std::string Ring::getForwardscatFilename() const
{
	return _forwardscatFilename;
}

std::string Ring::getUnlitFilename() const
{
	return _unlitFilename;
}

std::string Ring::getTransparencyFilename() const
{
	return _transparencyFilename;
}

std::string Ring::getColorFilename() const
{
	return _colorFilename;
}

Sphere::Sphere(
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

glm::vec3 Sphere::getRotationAxis() const
{
	return _rotAxis;
}

float Sphere::getRotationPeriod() const
{
	return _rotPeriod;
}

glm::vec3 Sphere::getMeanColor() const
{
	return _meanColor;
}

float Sphere::getRadius() const
{
	return _radius;
}

double Sphere::getGM() const
{
	return _GM;
}

std::string Sphere::getDiffuseFilename() const
{
	return _diffuseFilename;
}

Star::Star(const float brightness,
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

float Star::getBrightness() const
{
	return _brightness;
}

float Star::getFlareFadeInStart() const
{
	return _flareFadeInStart;
}

float Star::getFlareFadeInEnd() const
{
	return _flareFadeInEnd;
}

float Star::getFlareAttenuation() const
{
	return _flareAttenuation;
}

float Star::getFlareMinSize() const
{
	return _flareMinSize;
}

float Star::getFlareMaxSize() const
{
	return _flareMaxSize;
}

Clouds::Clouds(const std::string &filename, const float period) :
	_filename{filename},
	_period{period}
{

}

std::string Clouds::getFilename() const
{
	return _filename;
}

float Clouds::getPeriod() const
{
	return _period;
}

Night::Night(const std::string &filename,
	const float intensity) :
	_filename{filename},
	_intensity{intensity}
{

}

std::string Night::getFilename() const
{
	return _filename;
}

float Night::getIntensity() const
{
	return _intensity;
}

Specular::Specular(const std::string &filename,
	const Mask mask0, const Mask mask1) :
	_filename{filename},
	_mask0{mask0},
	_mask1{mask1}
{

}

Specular::Mask Specular::getMask0() const
{
	return _mask0;
}

Specular::Mask Specular::getMask1() const
{
	return _mask1;
}

std::string Specular::getFilename() const
{
	return _filename;
}

void Entity::setName(const std::string &name)
{
	_name = name;
}

void Entity::setDisplayName(const std::string &name)
{
	_displayName = name;
}

void Entity::setParentName(const std::string &name)
{
	_parentName = name;
}

void Entity::setSphere(const Sphere &sphere)
{
	_sphere = std::make_pair(true, sphere);
}

void Entity::setOrbit(const Orbit &orbit)
{
	_orbit = std::make_pair(true, orbit);
}

void Entity::setAtmo(const Atmo &atmo)
{
	_atmo = std::make_pair(true, atmo);
}

void Entity::setRing(const Ring &ring)
{
	_ring = std::make_pair(true, ring);
}

void Entity::setStar(const Star &star)
{
	_star = std::make_pair(true, star);
}

void Entity::setClouds(const Clouds &clouds)
{
	_clouds = std::make_pair(true, clouds);
}

void Entity::setNight(const Night &night)
{
	_night = std::make_pair(true, night);
}

void Entity::setSpecular(const Specular &specular)
{
	_specular = std::make_pair(true, specular);
}

bool Entity::isSphere() const
{
	return _sphere.first;
}

bool Entity::hasOrbit() const
{
	return _orbit.first;
}

bool Entity::hasAtmo() const
{
	return _atmo.first;
}

bool Entity::hasRing() const
{
	return _ring.first;
}

bool Entity::isStar() const
{
	return _star.first;
}

bool Entity::hasClouds() const
{
	return _clouds.first;
}

bool Entity::hasNight() const
{
	return _night.first;
}

bool Entity::hasSpecular() const
{
	return _specular.first;
}

std::string Entity::getName() const
{
	return _name;
}

std::string Entity::getDisplayName() const
{
	return _displayName;
}

std::string Entity::getParentName() const
{
	return _parentName;
}

const Sphere &Entity::getSphere() const
{
	return _sphere.second;
}

const Orbit &Entity::getOrbit() const
{
	return _orbit.second;
}

const Atmo &Entity::getAtmo() const
{
	return _atmo.second;
}

const Ring &Entity::getRing() const
{
	return _ring.second;
}

const Star &Entity::getStar() const
{
	return _star.second;
}

const Clouds &Entity::getClouds() const
{
	return _clouds.second;
}

const Night &Entity::getNight() const
{
	return _night.second;
}

const Specular &Entity::getSpecular() const
{
	return _specular.second;
}

EntityState::EntityState(
	const glm::dvec3 &pos, float rotationAngle, float cloudDisp) :
	_position{pos},
	_rotationAngle{rotationAngle},
	_cloudDisp{cloudDisp}
{

}

glm::dvec3 EntityState::getPosition() const
{
	return _position;
}

float EntityState::getRotationAngle() const
{
	return _rotationAngle;
}

float EntityState::getCloudDisp() const
{
	return _cloudDisp;
}
