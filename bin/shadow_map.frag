#version 330

in float pass_depth;
in vec2 pass_uv;

uniform sampler2D tex;
uniform float minDist;
uniform float maxDist;

out float out_depthalpha;

void main(void)
{
	float dist = length(pass_uv);
	if (dist < minDist || dist > maxDist) discard;
	if (texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r < 0.3) discard;
	out_depthalpha = pass_depth;
}

