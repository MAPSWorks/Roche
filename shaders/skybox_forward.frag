#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
	float exposure;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	vec4 lightDir;
	float albedo;
	float cloudDisp;
	float nightIntensity;
};

layout (binding = 2) uniform sampler2D diffuse;

layout (location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(texture(diffuse, passUv.st).rgb*albedo*exposure, 1.0);
}