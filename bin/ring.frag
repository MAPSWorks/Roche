#version 330
in vec2 pass_uv;

uniform sampler2D tex;
uniform vec4 ring_color;

out vec4 out_color;

void main(void)
{
	float minDist = 0.4;
    float dist = length(pass_uv);
    if (dist < minDist || dist > 1.0) discard;
    float matter = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;
	out_color = ring_color * vec4(vec3(1),matter);
}