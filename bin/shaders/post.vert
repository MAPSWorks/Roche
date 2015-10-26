#version 330

layout(location = 0)in vec2 in_position;

out vec2 pass_uv;

void main(void)
{
	pass_uv = in_position;
	gl_Position = vec4(in_position*2-vec2(1),0.0,1.0);
}