#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct OrbitalParameters
{
	// Kepler orbital parameters
	double ecc, sma, inc, lan, arg, m0; // Meters, radians
	glm::dvec3 computePosition(double epoch, double parentGM);
};

struct AtmosphericParameters
{
	float K_R; /// Rayleigh scattering constant
	float K_M; /// Mie scattering constant
	float E;   /// Sunlight intensity;
	float G_M; /// Mie G constant
	glm::vec3 C_R; /// 1 / sunlight wavelength^4
	float maxHeight; /// Max atmospheric height
	float scaleHeight; /// Scale height
	bool hasAtmosphere;

	void generateLookupTable(std::vector<float> &table, size_t size, float radius);
};

struct RingParameters
{
	bool hasRings;
	float innerDistance; /// distance to planet of inner ring
	float outerDistance; /// distance to planet of outer ring
	int seed; /// seed for ring generation
	glm::vec3 normal; /// Plane normal (normalized)
	glm::vec4 color;

	void generateRings(std::vector<uint8_t> &pixelBuf, int seed);
};

struct BodyParameters
{
	glm::vec3 rotationAxis;
	float rotationPeriod; // radians per second
	glm::vec3 meanColor; // Color seen from far away
	float radius; // km
	double GM; // gravitational parameter
	bool isStar; // for lighting computation
	float albedo; // light intensity from far away
	float cloudDispRate;
};

struct AssetPaths
{
	std::string diffuseFilename;
	std::string cloudFilename;
	std::string nightFilename;
	std::string modelFilename;
};

// Immutable state
class PlanetParameters
{
public:
	std::string name;
	std::string parentName;
	OrbitalParameters orbitParam;
	AtmosphericParameters atmoParam;
	RingParameters ringParam;
	BodyParameters bodyParam;
	AssetPaths assetPaths;
};

// Mutable state
class PlanetState
{
public:
	glm::dvec3 position;
	float rotationAngle;
	float cloudDisp;
};

class SkyboxParameters
{
public:
	float inclination;
	std::string textureFilename;
};