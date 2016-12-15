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
};

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;
layout (location = 2) out vec4 passPosition;
layout (location = 3) out vec4 passLpos;

void main(void)
{
	passUv = inUv;
	passLpos = modelMat*vec4(inPosition.xyz, 0.0);
	passNormal = normalize(passLpos);
	passPosition = modelMat*inPosition;
	gl_Position = projMat*viewMat*passPosition;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e8 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;
}