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
	vec4 planetPos;
	vec4 lightDir;
	vec4 K;
	float albedo;
	float cloudDisp;
	float nightIntensity;
	float atmoDensity;
	float radius;
	float atmoHeight;
};

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;
layout (location = 2) out vec4 passPosition;

void main(void)
{
	passUv = inUv;
	passNormal = normalize(viewMat*modelMat*inNormal);
	passPosition = viewMat*modelMat*inPosition;
	gl_Position = projMat*passPosition;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e10 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;
}