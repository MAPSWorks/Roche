#pragma once

#include <string>
#include <vector>
#include <limits>
#include <utility>

#include <glm/glm.hpp>

// Immutable state
class Planet
{
public:
	class Orbit
	{
	public:
		Orbit() = default;
		Orbit(double ecc, double sma, double inc, double lan, double arg, double m0);
		glm::dvec3 computePosition(double epoch, double parentGM) const;
	private:
		// Kepler orbital parameters (Meters & radians)
		double _ecc = 0.0;
		double _sma = 0.0;
		double _inc = 0.0;
		double _lan = 0.0;
		double _arg = 0.0;
		double _m0 = 0.0; 
	};

	class Atmo
	{
	public:
		Atmo() = default;
		Atmo(glm::vec4 K, float density, float maxHeight, float scaleHeight);
		std::vector<float> generateLookupTable(size_t size, float radius) const;

		glm::vec4 getScatteringConstant() const;
		float getDensity() const;
		float getMaxHeight() const;
		float getScaleHeight() const;
	private:
		glm::vec4 _K = glm::vec4(0.0);
		float _density = 0.0;
		float _maxHeight = 0.0; /// Max atmospheric height
		float _scaleHeight = 0.0; /// Scale height
	};

	class Ring
	{
	public:
		Ring() = default;
		Ring(float innerDistance, float outerDistance, glm::vec3 normal,
			const std::string &backscatFilename,
			const std::string &forwardscatFilename,
			const std::string &unlitFilename,
			const std::string &transparencyFilename,
			const std::string &colorFilename);
		std::vector<float> loadFile(const std::string &filename) const;

		float getInnerDistance() const;
		float getOuterDistance() const;
		glm::vec3 getNormal() const;
		std::string getBackscatFilename() const;
		std::string getForwardscatFilename() const;
		std::string getUnlitFilename() const;
		std::string getTransparencyFilename() const;
		std::string getColorFilename() const;
	private:
		float _innerDistance = 0.0; /// distance to planet of inner ring
		float _outerDistance = 0.0; /// distance to planet of outer ring
		glm::vec3 _normal = glm::vec3(0.0); /// Plane normal (normalized)

		// Assets
		std::string _backscatFilename;
		std::string _forwardscatFilename;
		std::string _unlitFilename;
		std::string _transparencyFilename;
		std::string _colorFilename;
	};

	class Body
	{
	public:
		Body() = default;
		Body(
			float radius,
			double GM,
			glm::vec3 rotAxis,
			float rotPeriod,
			glm::vec3 meanColor,
			const std::string &diffuseFilename);
		glm::vec3 getRotationAxis() const;
		float getRotationPeriod() const;
		glm::vec3 getMeanColor() const;
		float getRadius() const;
		double getGM() const;
		std::string getDiffuseFilename() const;

	private:
		glm::vec3 _rotAxis = glm::vec3(0.0,0.0,1.0);
		// Seconds per revolution
		float _rotPeriod = std::numeric_limits<float>::infinity();
		glm::vec3 _meanColor = glm::vec3(0.0); // Color seen from far away
		float _radius = 0.0; // km
		double _GM = 0.0; // gravitational parameter
		// Asset paths
		std::string _diffuseFilename;
	};

	class Star
	{
	public:
		Star() = default;
		explicit Star(float brightness);
		float getBrightness() const;
	private:
		float _brightness;
	};

	class Clouds
	{
	public:
		Clouds() = default;
		explicit Clouds(const std::string &filename, float period=0);
		std::string getFilename() const;
		float getPeriod() const;
	private:
		std::string _filename;
		float _period;
	};

	class Night
	{
	public:
		Night() = default;
		explicit Night(const std::string &filename, float intensity=1);
		std::string getFilename() const;
		float getIntensity() const;
	private:
		std::string _filename;
		float _intensity;
	};

	class Specular
	{
	public:
		struct Mask
		{
			glm::vec3 color;
			float hardness;
		};
		Specular() = default;
		explicit Specular(const std::string &filename, Mask mask0, Mask mask1);
		Mask getMask0() const;
		Mask getMask1() const;
		std::string getFilename() const;
	private:
		std::string _filename;
		Mask _mask0;
		Mask _mask1;
	};

	Planet() = default;
	void setName(const std::string &name);
	void setParentName(const std::string &name);
	void setBody(const Body &body);
	void setOrbit(const Orbit &orbit);
	void setAtmo(const Atmo &atmo);
	void setRing(const Ring &ring);
	void setStar(const Star &star);
	void setClouds(const Clouds &clouds);
	void setNight(const Night &night);
	void setSpecular(const Specular &specular);

	bool hasOrbit() const;
	bool hasAtmo() const;
	bool hasRing() const;
	bool isStar() const;
	bool hasClouds() const;
	bool hasNight() const;
	bool hasSpecular() const;

	std::string getName() const;
	std::string getParentName() const;
	const Body &getBody() const;
	const Orbit &getOrbit() const;
	const Atmo &getAtmo() const;
	const Ring &getRing() const;
	const Star &getStar() const;
	const Clouds &getClouds() const;
	const Night &getNight() const;
	const Specular &getSpecular() const;

private:
	std::string _name = "Undefined";
	std::string _parentName = "";
	Body _body = Body();
	std::pair<bool, Orbit> _orbit = std::make_pair(false, Orbit());
	std::pair<bool, Atmo> _atmo = std::make_pair(false, Atmo());
	std::pair<bool, Ring> _ring = std::make_pair(false, Ring());
	std::pair<bool, Star> _star = std::make_pair(false, Star());
	std::pair<bool, Clouds> _clouds = std::make_pair(false, Clouds());
	std::pair<bool, Night> _night = std::make_pair(false, Night());
	std::pair<bool, Specular> _specular = std::make_pair(false, Specular());
};

// Mutable state
class PlanetState
{
public:
	PlanetState() = default;
	PlanetState(const glm::dvec3 &pos, float rotationAngle, float cloudDisp);
	glm::dvec3 getPosition() const;
	float getRotationAngle() const;
	float getCloudDisp() const;
private:
	glm::dvec3 _position = glm::dvec3(0.0);
	float _rotationAngle = 0.0;
	float _cloudDisp = 0.0;
};