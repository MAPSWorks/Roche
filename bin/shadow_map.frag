#version 330

in float pass_depth;
in vec2 pass_uv;

uniform sampler2D tex;
uniform float minDist;

out vec2 out_depthalpha;

void main(void)
{
	out_depthalpha.r = pass_depth;
	float dist = length(pass_uv);
	out_depthalpha.g = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;
}

