#version 330
// PIXEL ATTRIBUTES
in vec2 pass_uv;
in vec3 pass_normal;
in vec4 pass_position;
in vec3 pass_lpos;
out vec4 out_color;

// TEXTURES
uniform sampler2D day_tex;

// GLOBAL VARIABLES
uniform vec3 view_pos; // Position of the camera

void main(void)
{
	// TEXTURE LOOKUPS
	vec3 color = texture(day_tex,pass_uv).rgb;

	// ATMOSPHERE RENDERING
	vec3 to_viewer = normalize(view_pos-pass_position.xyz);
	float angle = pow(1.0 - dot(pass_normal, to_viewer), 3);
	float rim = angle*angle*angle;

	out_color = vec4(color*(1.0+angle),1.0-rim);
}