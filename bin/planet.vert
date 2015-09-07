#version 330
layout(location = 0)in vec4 in_position;
layout(location = 1)in vec2 in_uv;

out vec2 pass_uv;
void main(void)
{
	pass_uv = in_uv;
	gl_Position = in_position;
}