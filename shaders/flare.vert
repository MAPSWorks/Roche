#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float ambientColor;
	float invGamma;
	float exposure;
};

layout (binding = 1, std140) uniform flareDynamicUBO
{
	mat4 modelMat;
	vec4 color;
	float brightness;
};

layout (location = 0) out vec4 passUv;

void main()
{
	passUv = inUv;
	gl_Position = modelMat*inPosition;
}