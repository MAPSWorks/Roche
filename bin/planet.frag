#version 330
in vec2 pass_uv;

uniform sampler1D tex;
uniform vec4 ring_color;

out vec4 out_color;

void main(void)
{
	float mindist = 0.4;
	float dist = length(pass_uv);
	if (dist < mindist || dist > 1.0) discard;
	float matter = texture(tex,((dist-mindist)/(1.0-mindist)));
	out_color = matter*ring_color;
}