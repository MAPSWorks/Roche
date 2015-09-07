#version 330
in vec2 pass_uv;
in vec3 pass_normal;

out vec4 out_color;

void main(void)
{
	float light = dot(pass_normal, normalize(vec3(1.0,1.0,2)));
	out_color = vec4(vec3(0.6,0.87,1.0)*light,1.0);
}