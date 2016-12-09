#version 330
layout(location = 0)in vec4 in_position;
layout(location = 1)in vec2 in_uv;

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 modelMat;

out vec2 pass_uv;

void main(void)
{
	pass_uv = in_uv;
	gl_Position = projMat*mat4(mat3(viewMat))*modelMat*in_position;
}