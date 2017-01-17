#include "planet.hpp"

#include <cstdlib>
#include <random>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "thirdparty/shaun/sweeper.hpp"
#include <glm/ext.hpp>

#define PI 3.14159265358979323846264338327950288 

void RingParameters::generateRings(std::vector<uint8_t> &pixelBuffer, const int seed)
{
	// Create larger buffer for better anti-aliasing
	const int upscale = 4;
	std::vector<float> refBuffer(pixelBuffer.size()*upscale, 1.0);

	// Random init
	std::mt19937 rng(seed);
	std::uniform_real_distribution<> dis(0, 1);

	// Populate gaps
	const int maxGapsize = refBuffer.size()/20;
	for (int i=0;i<100;++i)
	{
		// multiply generated range by opacity
		const int gapSize = dis(rng)*maxGapsize;
		const int gapOffset = dis(rng)*(refBuffer.size()-gapSize+1);
		const float gapOpacity = std::max((float)dis(rng),0.4f);
		for (int j=gapOffset;j<gapOffset+gapSize;++j)
			refBuffer[j] *= gapOpacity;
	}

	// brightness equalization
	float mean = 0.f;
	for (float v : refBuffer)
		mean += v;
	mean /= refBuffer.size();
	const float mul = 1.0/mean;
	for (auto &v : refBuffer)
		v *= mul;

	// fading on edges
	const int fade = refBuffer.size()/10;
	for (int i=0;i<fade;++i)
	{
		refBuffer[refBuffer.size()-i-1] *= i/(float)fade;
		refBuffer[i] *= i/(float)fade;
	}

	// Downscaling
	for (int i=0;i<pixelBuffer.size();++i)
	{
		float mean = 0.f;
		for (int j=i*upscale;j<(i+1)*upscale;++j)
			mean += refBuffer[j];
		mean /= upscale;
		pixelBuffer[i] = (unsigned char)(mean*255);
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
		const double orbital_period = 2*PI*sqrt((sma*sma*sma)/parentGM);
		const double mean_motion = 2*PI/orbital_period;
		const double meanAnomaly = fmod(epoch*mean_motion + m0, 2*PI);
		// Newton to find eccentric anomaly (En)
		const int it = 20; // Number of iterations
		double En = (ecc<0.8)?meanAnomaly:PI; // Starting value of En
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
	glm::vec2 v = a+step*0.5;

	float sum = 0.f;
	for (int i=0;i<samples;++i)
	{
		sum += scatDensity(v, radius, scaleHeight);
		v += step;
	}
	return sum * glm::length(step) / maxHeight;
}

float raySphereFar(const glm::vec2 origin, const glm::vec2 ray, const float radius)
{
	const float b = glm::dot(origin, ray);
	const float c = glm::dot(origin, origin) - radius*radius;
	return -b+sqrt(b*b-c);
}

void AtmosphericParameters::generateLookupTable(std::vector<float> &table, const size_t size, const float radius)
{
	/*  2 channel lookup table :
	 *  x-axis for altitude (0.0 for sl, 1.0 for maxHeight)
	 *  y-axis for cosine of angle of ray /2
	 *  First channel for air density
	 *  Second channel for out scattering factor
	 */
	table.resize(size*size*2);

	for (int i=0;i<size;++i)
	{
		const float altitude = (float)i/(float)size * maxHeight;
		const float density = glm::exp(-altitude/scaleHeight);
		for (int j=0;j<size;++j)
		{
			const size_t index = (i+j*size)*2;
			const float angle = (float)j*PI/(float)size;
			const glm::vec2 rayDir = glm::vec2(sin(angle), cos(angle));
			const glm::vec2 rayOri = glm::vec2(0, radius + altitude);
			const float t = raySphereFar(rayOri, rayDir, radius+maxHeight);
			const glm::vec2 u = rayOri + rayDir*t;
			table[index+0] = density;
			table[index+1] = scatOptic(rayOri, u, radius, scaleHeight, maxHeight, 50)*(4*PI);
		}
	}
}