#version 330

in vec4 pass_position;

uniform vec3 color;

out vec4 out_color;

void main(void)
{
	out_color = vec4(color,0.4);
}