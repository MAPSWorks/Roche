#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
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
layout (location = 2) out float passLinearDepth;

void main(void)
{
	passUv = inUv;
	passNormal = normalize(viewMat*modelMat*vec4(inPosition.xyz, 0.0));
	vec4 viewSpace = viewMat*modelMat*inPosition;
	passLinearDepth = viewSpace.z/viewSpace.w;
	gl_Position = projMat*viewSpace;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e8 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;
}