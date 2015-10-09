#version 330
layout(location = 0)in vec4 in_position;
layout(location = 1)in vec2 in_uv;

uniform vec2 pos;
uniform float size;
uniform float ratio;

out vec2 pass_uv;

void main(void)
{
	pass_uv = in_uv;
	gl_Position = vec4(pos+in_position.xy*size*vec2(1.0,ratio),0.0,1.0);
}