#include "renderer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <random>
#include <algorithm>

#define PI 3.14159265358979323846264338327950288

// Flare radial components
// r is the radius in degrees
float f0(float r)
{
	float a = r/0.02;
	float a2 = a*a;
	return 2.61e6*glm::exp(-a2);
}

float f1(float r)
{
	float a = r+0.02;
	float a3 = a*a*a;
	return 20.91/a3;
}

float f2(float r)
{
	float a = r+0.02;
	float a2 = a*a;
	return 72.37/a2;
}

// L is the wavelength in nanometers
float f3(float r, float L)
{
	float a = r-3*(L/568.0);
	float a2 = a*a;
	return 436.9*(568.0/L)*exp(-19.75*a2);
}

// pupil diameter in mm, luminance in cd/m2
float lumToPupilDiameter(float L)
{
	return 4.9-3*glm::tanh(0.4*glm::log(L+1.0));
}

float lerp(float a, float b, float x)
{
	return a*(1-x)+b*x;
}

void lerpArray(const float a[4], const float b[4], float x, float res[4])
{
	for (int i=0;i<4;++i) res[i] = lerp(a[i],b[i],x);
}

glm::vec3 wavelengthToRGB(float L)
{
	if (L>=380 && L<440) return glm::vec3(-(L-440)/(440-380),0.0,1.0);
	if (L>=440 && L<490) return glm::vec3(0.0,(L-440)/(490-440),1.0);
	if (L>=490 && L<510) return glm::vec3(0.0,1.0,-(L-510)/(510-490));
	if (L>=510 && L<580) return glm::vec3((L-510)/(580-510),1.0,0.0);
	if (L>=580 && L<645) return glm::vec3(1.0,-(L-645)/(645-580),0.0);
	if (L>=645 && L<781) return glm::vec3(1.0,0.0,0.0);
	return glm::vec3(0.0,0.0,0.0);
}

std::mt19937 rng;
std::uniform_real_distribution<> dis(0, 1);


void Renderer::generateFlareIntensityTex(int dimensions, std::vector<uint16_t> &pixelData)
{
	pixelData.resize(dimensions);
	const float SIZE_DEGREES = 60.0;
	for (int i=0;i<dimensions;++i)
	{
		float r = (SIZE_DEGREES*i)/((float)dimensions-1);
		float intensity = 0.282*f0(r)+0.478*f1(r)+0.207*f2(r);
		pixelData[i] = glm::packHalf1x16(std::min(1000.f,intensity));
	}
}

void Renderer::generateFlareLinesTex(int dimensions, std::vector<uint8_t> &pixelData)
{
	// Generate 1D array first
	int size = 60;
	std::vector<float> lines(size);
	for (int i=0;i<size;++i)
	{
		lines[i] = dis(rng);
	}

	pixelData.resize(dimensions*dimensions*4);
	for (int i=0;i<dimensions;++i)
	{
		for (int j=0;j<dimensions;++j)
		{
			// Get all corners of pixels
			float xs[4] = {i-0.5f,i-0.5f,i+0.5f,i+0.5f};
			float ys[4] = {j-0.5f,j+0.5f,j-0.5f,j+0.5f};
			// Find lowest and higher angle
			float minAngle = 1000.0;
			float maxAngle = -1000.0;
			for (int k=0;k<4;++k)
			{
				float x = xs[k]/(float)(dimensions-1) - 0.5;
				float y = ys[k]/(float)(dimensions-1) - 0.5;
				float angle = (atan2(y,x)+PI)/2*PI;
				minAngle = std::min(angle, minAngle);
				maxAngle = std::max(angle, maxAngle);
			}
			int minAccess = floor(minAngle*size);
			int maxAccess = ceil(maxAngle*size);
			float minSub = fmod(minAngle*size,1.0);
			float maxSub = fmod(maxAngle*size,1.0);
			
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
}

void Renderer::generateFlareHaloTex(int dimensions, std::vector<uint16_t> &pixelData)
{
	pixelData.resize(dimensions*4);
	// Color integration
	int colorSteps = 50;
	glm::vec3 totalSum(0.0);
	for (int i=0;i<colorSteps;++i)
	{
		float v = 400.0+(i/(float)colorSteps)*300.0;
		totalSum += wavelengthToRGB(v);
	}
	const glm::vec3 colorInv = glm::vec3(1.0)/totalSum;

	for (int i=0;i<dimensions;++i)
	{
		const float r = (2.422f*i)/((float)dimensions-1)+1.647f;
		glm::vec3 sum(0.0);
		for (int j=0;j<colorSteps;++j)
		{
			const float L = 400.0+(j/(float)colorSteps)*300.0;
			const glm::vec3 color = wavelengthToRGB(L);
			sum += color*f3(r,L);
		}
		const glm::vec4 finalColor = glm::vec4(sum*colorInv*0.033f*0.1f,1.f);
		for (int j=0;j<4;++j)
		{
			pixelData[i*4+j] = glm::packHalf1x16(finalColor[j]);
		}
	}
}
