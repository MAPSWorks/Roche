#version 330

layout(location = 0)in vec4 in_position;

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 modelMat;

out vec4 pass_position;

void main(void)
{
	pass_position = modelMat*in_position;
	gl_Position = projMat*viewMat*pass_position;
}