#version 330

layout(location = 0)in vec4 in_position;

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 modelMat;

out vec3 pass_position;

void main(void)
{
	pass_position = mat3(modelMat)*in_position.xyz;
	gl_Position = projMat*viewMat*modelMat*in_position;
}