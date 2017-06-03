#include "flare.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <random>
#include <algorithm>

using namespace glm;

// Flare radial components
// r is the radius in degrees
float f0(const float r)
{
	const float a = r/0.02f;
	const float a2 = a*a;
	return 2.61e6*glm::exp(-a2);
}

float f1(const float r)
{
	const float a = r+0.02f;
	const float a3 = a*a*a;
	return 20.91f/a3;
}

float f2(const float r)
{
	const float a = r+0.02f;
	const float a2 = a*a;
	return 72.37f/a2;
}

// L is the wavelength in nanometers
float f3(const float r, const float L)
{
	const float a = r-3.f*(L/568.f);
	const float a2 = a*a;
	return 436.9f*(568.f/L)*exp(-19.75f*a2);
}

// pupil diameter in mm, luminance in cd/m2
float lumToPupilDiameter(const float L)
{
	return 4.9f-3.f*glm::tanh(0.4f*glm::log(L+1.f));
}

glm::vec3 wavelengthToRGB(const float L)
{
	if (L>=380 && L<440) return vec3(-(L-440)/(440-380),0.0,1.0);
	if (L>=440 && L<490) return vec3(0.0,(L-440)/(490-440),1.0);
	if (L>=490 && L<510) return vec3(0.0,1.0,-(L-510)/(510-490));
	if (L>=510 && L<580) return vec3((L-510)/(580-510),1.0,0.0);
	if (L>=580 && L<645) return vec3(1.0,-(L-645)/(645-580),0.0);
	if (L>=645 && L<781) return vec3(1.0,0.0,0.0);
	return vec3(0.0,0.0,0.0);
}


std::vector<uint16_t> generateFlareIntensityTex(const int dimensions)
{
	std::vector<uint16_t> pixelData(dimensions);
	const float SIZE_DEGREES = 60.0;
	for (int i=0;i<dimensions;++i)
	{
		float r = (SIZE_DEGREES*i)/((float)dimensions-1);
		float intensity = 0.282f*f0(r)+0.478f*f1(r)+0.207f*f2(r);
		pixelData[i] = glm::packHalf1x16(std::min(1000.f,intensity));
	}
	return pixelData;
}

std::vector<uint8_t> generateFlareLinesTex(int dimensions)
{
	// Generate 1D array first
	std::mt19937 rng;
	std::uniform_real_distribution<> dis(0, 1);

	int size = 60;
	std::vector<float> lines(size);
	for (int i=0;i<size;++i)
	{
		lines[i] = dis(rng);
	}

	std::vector<uint8_t> pixelData(dimensions*dimensions*4);
	for (int i=0;i<dimensions;++i)
	{
		for (int j=0;j<dimensions;++j)
		{
			// Get all corners of pixels
			const float xs[4] = {i-0.5f,i-0.5f,i+0.5f,i+0.5f};
			const float ys[4] = {j-0.5f,j+0.5f,j-0.5f,j+0.5f};
			// Find lowest and higher angle
			float minAngle = +1000.f;
			float maxAngle = -1000.f;
			for (int k=0;k<4;++k)
			{
				const float x = xs[k]/(float)(dimensions-1) - 0.5f;
				const float y = ys[k]/(float)(dimensions-1) - 0.5f;
				const float angle = (float)(atan2(y,x)+glm::pi<float>())/2*glm::pi<float>();
				minAngle = std::min(angle, minAngle);
				maxAngle = std::max(angle, maxAngle);
			}
			const int minAccess = floor(minAngle*size);
			const int maxAccess = ceil(maxAngle*size);
			const float minSub = fmod(minAngle*size,1.f);
			const float maxSub = fmod(maxAngle*size,1.f);
			
			float avg = 0.0;
			// Smooth border pixels
			avg += lines[minAccess%size]*(1.0-minSub);
			avg += lines[maxAccess%size]*maxSub;
			// Interpolate between all elements between min and max angles
			for (int k=minAccess+1;k<maxAccess;++k)
			{
				avg += lines[k%size];
			}
			// Conversion from float to uint8_t
			pixelData[i*dimensions+j] = (avg/(float)(maxAccess-minAccess+1))*255;
		}
	}
	return pixelData;
}

std::vector<uint16_t> generateFlareHaloTex(const int dimensions)
{
	std::vector<uint16_t> pixelData(dimensions*4);
	// Color integration
	const int colorSteps = 50;
	glm::vec3 totalSum(0.0);
	for (int i=0;i<colorSteps;++i)
	{
		const float v = 400.f+(i/(float)colorSteps)*300.f;
		totalSum += wavelengthToRGB(v);
	}
	const glm::vec3 colorInv = glm::vec3(1.0)/totalSum;

	for (int i=0;i<dimensions;++i)
	{
		const float r = (2.422f*i)/((float)dimensions-1)+1.647f;
		glm::vec3 sum(0.0);
		for (int j=0;j<colorSteps;++j)
		{
			const float L = 400.f+(j/(float)colorSteps)*300.f;
			const glm::vec3 color = wavelengthToRGB(L);
			sum += color*f3(r,L);
		}
		const glm::vec4 finalColor = glm::vec4(sum*colorInv*0.033f*0.1f,1.f);
		for (int j=0;j<4;++j)
		{
			pixelData[i*4+j] = glm::packHalf1x16(finalColor[j]);
		}
	}
	return pixelData;
}
