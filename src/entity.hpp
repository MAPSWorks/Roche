#pragma once

#include <string>
#include <vector>
#include <limits>
#include <utility>
#include <map>

#include <glm/glm.hpp>

class Orbit
{
public:
	Orbit() = default;
	/**
	 * @param ecc Eccentricity
	 * @param sma Semi-Major Axis (meters)
	 * @param inc Inclination (radians)
	 * @param lan Longitude of ascending node (radians)
	 * @param arg Argument of periapsis (radians)
	 * @param period Period of orbit (seconds)
	 * @param m0 Mean anomaly at epoch (radians)
	 */
	Orbit(
		double ecc, double sma, double inc, 
		double lan, double arg, double period, double m0);
	/**
	 * Computes cartesian coordinates of entity around parent entity
	 * @param epoch epoch in seconds
	 * @return cartesian coordinates around parent entity
	 */
	glm::dvec3 computePosition(double epoch) const;
private:
	// Kepler orbital parameters (Meters & radians)
	/// Eccentricity
	double _ecc = 0.0;
	/// Semi-Major Axis (meters)
	double _sma = 0.0;
	/// Inclination (radians)
	double _inc = 0.0;
	/// Longitude of ascending node (radians)
	double _lan = 0.0;
	/// Argument of periapsis (radians)
	double _arg = 0.0;
	/// Period
	double _pr = 1.0;
	/// Mean anomaly at epoch (radians)
	double _m0 = 0.0; 
};

class Atmo
{
public:
	Atmo() = default;
	/**
	 * @param K scattering constants
	 * @param density density at sea level
	 * @param maxHeight Atmospheric ceiling (0 pressure above)
	 * @param scaleHeight Scale height of atmosphere
	 */
	Atmo(glm::vec4 K, float density, float maxHeight, float scaleHeight);
	/**
	 * Generate lookup texture for atmosphere rendering
	 * @param size width and height of texture
	 * @param radius radius of entity
	 */
	std::vector<float> generateLookupTable(size_t size, float radius) const;

	glm::vec4 getScatteringConstant() const;
	float getDensity() const;
	float getMaxHeight() const;
	float getScaleHeight() const;
private:
	/// Scattering constants
	glm::vec4 _K = glm::vec4(0.0);
	/// Density at sea level
	float _density = 0.0;
	/// Atmospheric ceiling
	float _maxHeight = 0.0;
	/// Atmospheric scale height
	float _scaleHeight = 0.0;
};

class Ring
{
public:
	Ring() = default;
	/**
	 * @param innerDistance distance of inner edge of rings from entity center
	 * @param outerDistance distance of outer edge of rings from entity center
	 * @param normal ring plane normal
	 * @param backscatFilename backscattering brightness amount
	 * @param forwardscatFilename forward scattering brightness amount
	 * @param unlitFilename unlit side brightness amount
	 * @param transparencyFilename transparency amount
	 * @param colorFilename ring color texture
	 */
	Ring(float innerDistance, float outerDistance, glm::vec3 normal,
		const std::string &backscatFilename,
		const std::string &forwardscatFilename,
		const std::string &unlitFilename,
		const std::string &transparencyFilename,
		const std::string &colorFilename);

	/** Load txt files for rings */
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
	/// distance from entity center to inner edge
	float _innerDistance = 0.0;
	/// distance from entity center to outer edge
	float _outerDistance = 0.0;
	/// Plane normal
	glm::vec3 _normal = glm::vec3(0.0); 

	// Assets
	std::string _backscatFilename;
	std::string _forwardscatFilename;
	std::string _unlitFilename;
	std::string _transparencyFilename;
	std::string _colorFilename;
};

class Model
{
public:
	Model() = default;
	/**
	 * @param radius radius of sphere (km)
	 * @param GM gravitational parameter
	 * @param rotAxis Axis of rotation
	 * @param rotPeriod length of sidereal day (seconds)
	 * @param meanColor flare color
	 * @param diffuseFilename diffuse texture filename
	 */
	Model(
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
	/// Entity rotation axis
	glm::vec3 _rotAxis = glm::vec3(0.0,0.0,1.0);
	/// Seconds per revolution
	float _rotPeriod = std::numeric_limits<float>::infinity();
	/// Color seen from far away
	glm::vec3 _meanColor = glm::vec3(0.0);
	/// Radius in km
	float _radius = 0.0;
	/// Gravitational parameter
	double _GM = 0.0;
	/// Diffuse texture filename
	std::string _diffuseFilename;
};

class Star
{
public:
	Star() = default;
	Star(float brightness, 
		float flareFadeInStart, float flareFadeInEnd, float flareAttenuation,
		float flareMinSize, float flareMaxSize);
	float getBrightness() const;
	float getFlareFadeInStart() const;
	float getFlareFadeInEnd() const;
	float getFlareAttenuation() const;
	float getFlareMinSize() const;
	float getFlareMaxSize() const;
private:
	float _brightness;
	float _flareFadeInStart;
	float _flareFadeInEnd;
	float _flareAttenuation;
	float _flareMinSize;
	float _flareMaxSize;
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
		/// Specular highlight color
		glm::vec3 color;
		/// Specular highlight hardness (0-large; 255-small)
		float hardness;
	};
	Specular() = default;
	/**
	 * @param filename Specular mask image filename
	 * @param mask0 Specular properties on black areas of mask
	 * @param mask1 Specular properties on white areas of mask
	 */
	explicit Specular(const std::string &filename, Mask mask0, Mask mask1);
	Mask getMask0() const;
	Mask getMask1() const;
	std::string getFilename() const;
private:
	std::string _filename;
	Mask _mask0;
	Mask _mask1;
};

/**
 * Fixed state of an entity, unlikely to change
 */
class EntityParam
{
public:
	EntityParam() = default;
	/**
	 * Sets the name of the entity
	 * @param name unique name for the entity
	 */
	void setName(const std::string &name);
	/**
	 * Sets the fancy name of the entity
	 * @param name name to be displayed on screen
	 */
	void setDisplayName(const std::string &name);
	/**
	 * Sets the name of the parent entity
	 * @param name of parent entity
	 */
	void setParentName(const std::string &name);
	/**
	 * Sets the sphere properties
	 * @param sphere Model properties
	 */
	void setModel(const Model &model);
	/**
	 * Sets the orbit parameters of the entity
	 * @param orbit Orbit parameters
	 */
	void setOrbit(const Orbit &orbit);
	/**
	 * Sets the atmosphere properties of the entity
	 * @param atmo Atmosphere properties
	 */
	void setAtmo(const Atmo &atmo);
	/**
	 * Sets the ring properties of the entity
	 * @param ring Ring properties
	 */
	void setRing(const Ring &ring);
	/**
	 * Sets the entity to render as a star
	 * @param star Star properties
	 */
	void setStar(const Star &star);
	/**
	 * Sets the cloud properties of the entity
	 * @param cloud Cloud properties
	 */
	void setClouds(const Clouds &clouds);
	/**
	 * Sets the night texture properties of the entity
	 * @param night Night texture properties
	 */
	void setNight(const Night &night);
	/**
	 * Sets the specular highlight properties of the entity
	 * @param specular Specular highlight properties
	 */
	void setSpecular(const Specular &specular);

	/// Indicates whether the entity is fixed in place or orbits some other entity
	bool hasOrbit() const;
	/// Indicates if the entity is a celestial body
	bool isBody() const;
	/// Indicates whether the entity has an atmosphere
	bool hasAtmo() const;
	/// Indicates whether the entity has a set of rings
	bool hasRing() const;
	/// Indicates whether the entity is rendered as a star or not
	bool isStar() const;
	/// Indicates whether the entity has a layer of clouds
	bool hasClouds() const;
	/// Indicates whether the entity has an emissive night texture
	bool hasNight() const;
	/// Indicates whether the entity has a reflective surface
	bool hasSpecular() const;

	/// Returns the name of the entity
	std::string getName() const;
	/// Returns the fancy name of the entity
	std::string getDisplayName() const;
	/// Returns the name of the parent entity
	std::string getParentName() const;
	/// Returns the sphere properties
	const Model &getModel() const;
	/// Returns the orbital parameters
	const Orbit &getOrbit() const;
	/// Returns the atmosphere properties
	const Atmo &getAtmo() const;
	/// Returns the ring properties
	const Ring &getRing() const;
	/// Returns the star properties
	const Star &getStar() const;
	/// Returns the cloud properties
	const Clouds &getClouds() const;
	/// Returns the night texture properties
	const Night &getNight() const;
	/// Returns the specular highlight properties
	const Specular &getSpecular() const;

private:
	std::string _name = "Undefined";
	std::string _displayName = "Undefined";
	std::string _parentName = "";
	std::pair<bool, Model> _model = std::make_pair(false, Model());
	std::pair<bool, Orbit> _orbit = std::make_pair(false, Orbit());
	std::pair<bool, Atmo> _atmo = std::make_pair(false, Atmo());
	std::pair<bool, Ring> _ring = std::make_pair(false, Ring());
	std::pair<bool, Star> _star = std::make_pair(false, Star());
	std::pair<bool, Clouds> _clouds = std::make_pair(false, Clouds());
	std::pair<bool, Night> _night = std::make_pair(false, Night());
	std::pair<bool, Specular> _specular = std::make_pair(false, Specular());
};

/**
 * Changing state of an entity, changing at every update
 */
class EntityState
{
public:
	EntityState() = default;
	/**
	 * @param pos world space position of center of entity
	 * @param rotationAngle rotation angle around Body::getRotationAxis()
	 * @param cloudDisp amount of displacement of the cloud layer
	 */
	explicit EntityState(const glm::dvec3 &pos, float rotationAngle=0.0, float cloudDisp=0.0);
	/// Returns the world space position of center of entity
	glm::dvec3 getPosition() const;
	/// Returns the rotation angle around Body::getRotationAxis()
	float getRotationAngle() const;
	/// Returns the amount of displacement of the cloud layer
	float getCloudDisp() const;
private:
	glm::dvec3 _position = glm::dvec3(0.0);
	float _rotationAngle = 0.0;
	float _cloudDisp = 0.0;
};

class EntityCollection;

class EntityHandle
{
	int _id = -1;
	const EntityCollection * _collec = nullptr;
	EntityHandle(const EntityCollection* collec, int id);
public:
	EntityHandle() = default;
	bool exists() const;
	const EntityParam &getParam() const;
	const EntityState &getState() const;
	EntityHandle getParent() const;
	std::vector<EntityHandle> getAllParents() const;
	std::vector<EntityHandle> getChildren() const;
	std::vector<EntityHandle> getAllChildren() const;
	bool operator<(const EntityHandle &h) const;
	bool operator==(const EntityHandle &h) const;
	friend class EntityCollection;
};

class EntityCollection
{
public:
	EntityCollection() = default;
	void init(const std::vector<EntityParam> &param);
	void setState(const std::map<EntityHandle, EntityState> &state);
	const std::vector<EntityHandle> &getAll() const;
	const std::vector<EntityHandle> &getBodies() const;
	friend class EntityHandle;
private:
	EntityHandle createHandle(int id) const;
	const EntityParam &getParam(const EntityHandle &handle) const;
	const EntityState &getState(const EntityHandle &handle) const;

	std::vector<EntityParam> _param;
	std::vector<EntityState> _state;

	std::vector<EntityHandle> _all;
	std::vector<EntityHandle> _bodies;

	std::vector<int> _parents;
};