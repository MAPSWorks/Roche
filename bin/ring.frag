#version 330
in vec2 pass_uv;
in float pass_dist;

uniform sampler1D tex;
uniform vec4 ring_color;

out vec4 out_color;

void main(void)
{
	float minDist = 0.4;
    float dist = length(pass_uv);
    if (dist < minDist || dist > 1.0) discard;
    float matter = textureLod(tex, (dist-minDist)/(1.0-minDist), log(pass_dist));
	out_color = matter*ring_color;
}