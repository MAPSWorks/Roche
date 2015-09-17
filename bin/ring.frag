#version 330
in vec2 pass_uv;
in vec4 pass_position;

uniform sampler2D tex;
uniform vec4 ring_color;
uniform float minDist;

out vec4 out_color;

void main(void)
{
    float dist = length(pass_uv);
    if (dist < minDist || dist > 1.0) discard;
    float matter = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;
	out_color = matter*ring_color;
}