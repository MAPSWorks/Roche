#include "planet.hpp"

#include <fstream>
#include <string>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "thirdparty/shaun/sweeper.hpp"
#include <glm/ext.hpp>

void RingParameters::loadFile(
	const std::string filename,
	std::vector<float> &pixelData) const
{
	std::ifstream in(filename);

	if (!in)
	{
		throw std::runtime_error("Can't open ring file " + filename);
	}

	// Clear values
	pixelData.clear();
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
}

glm::dvec3 OrbitalParameters::computePosition(const double epoch, const double parentGM)
{
	if (parentGM <= 0)
	{
		return glm::dvec3(0,0,0); // No parent body
	}
	else
	{
		// Mean Anomaly compute
		const double orbital_period = 2*glm::pi<float>()*sqrt((sma*sma*sma)/parentGM);
		const double mean_motion = 2*glm::pi<float>()/orbital_period;
		const double meanAnomaly = fmod(epoch*mean_motion + m0, 2*glm::pi<float>());
		// Newton to find eccentric anomaly (En)
		const int it = 20; // Number of iterations
		double En = (ecc<0.8)?meanAnomaly:glm::pi<float>(); // Starting value of En
		for (int i=0;i<it;++i)
			En -= (En - ecc*sin(En)-meanAnomaly)/(1-ecc*cos(En));
		// Eccentric anomaly to True anomaly
		const double trueAnomaly = 2*atan2(sqrt(1+ecc)*sin(En/2), sqrt(1-ecc)*cos(En/2));
		// Distance from parent body
		const double dist = sma*((1-ecc*ecc)/(1+ecc*cos(trueAnomaly)));
		// Plane changes
		const glm::dvec3 posInPlane = glm::dvec3(-sin(trueAnomaly)*dist,cos(trueAnomaly)*dist,0.0);
		const glm::dquat q =  glm::rotate(glm::dquat(), lan, glm::dvec3(0,0,1))
												* glm::rotate(glm::dquat(), inc, glm::dvec3(0,1,0))
												* glm::rotate(glm::dquat(), arg, glm::dvec3(0,0,1));
		return q*posInPlane;
	}
}

float scatDensity(const float p, const float scaleHeight)
{
	return glm::exp(-std::max(0.f, p)/scaleHeight);
}

float scatDensity(const glm::vec2 p, const float radius, const float scaleHeight)
{
	return scatDensity(glm::length(p) - radius, scaleHeight);
}

float scatOptic(const glm::vec2 a, const glm::vec2 b, 
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

float intersectsSphereNear(glm::vec3 ori, glm::vec3 dir, float radius)
{
	const float b = glm::dot(ori,dir);
	const float c = glm::dot(ori,ori)-radius*radius;
	return -b-sqrt(b*b-c);
}

float intersectsSphereFar(glm::vec3 ori, glm::vec3 dir, float radius)
{
	const float b = glm::dot(ori,dir);
	const float c = glm::dot(ori,ori)-radius*radius;
	return -b+sqrt(b*b-c);
}

void AtmosphericParameters::generateLookupTable(
	std::vector<float> &table,
	const size_t size,
	const float radius) const
{
	/*  2 channel lookup table :
	 *  y-axis for altitude (0.0 for sl, 1.0 for maxHeight)
	 *  x-axis for cosine of angle of ray
	 *  First channel for air density
	 *  Second channel for out scattering factor
	 */
	table.resize(size*size*2);

	size_t index = 0;
	for (int i=0;i<size;++i)
	{
		const float altitude = (float)i/(float)size * maxHeight;
		const float density = glm::exp(-altitude/scaleHeight);
		for (int j=0;j<size;++j)
		{
			const float angle = acos(2*(float)j/(float)(size-1)-1);
			const glm::vec2 rayDir = glm::vec2(sin(angle), cos(angle));
			const glm::vec2 rayOri = glm::vec2(0, radius + altitude);
			// Test against planet surface
			const float b = glm::dot(rayOri, rayDir);
			const float r1 = radius;
			const float c1 = glm::dot(rayOri, rayOri)-r1*r1;
			const float r2 = radius+maxHeight;
			const float c2 = glm::dot(rayOri,rayOri)-r2*r2;
			const float t = -b+sqrt(b*b-c2);
			const glm::vec2 u = rayOri + rayDir*t;
			const float depth = scatOptic(rayOri, u, radius, scaleHeight, maxHeight, 50);
			table[index+0] = density;
			table[index+1] = depth;
			index += 2;
		}
	}
}