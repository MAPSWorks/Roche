#version 450

layout (location = 0) in vec4 inPosition;

layout (binding = 0, std140) uniform dynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	mat4 modelMat;
	vec4 viewPos;
	vec4 lightDir;
};

layout (location = 0) out vec4 passNormal;

void main(void)
{
	passNormal = modelMat*vec4(inPosition.xyz, 0.0);
	gl_Position = projMat*viewMat*modelMat*inPosition;
}