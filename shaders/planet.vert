#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;
layout(location = 2) in vec4 inNormal;
layout(location = 3) in vec4 inTangent;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float ambientColor;
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

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;

void main(void)
{
	passUv = inUv;
	passNormal = normalize(viewMat*modelMat*inNormal);
	gl_Position = projMat*viewMat*modelMat*inPosition;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e10 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;
}