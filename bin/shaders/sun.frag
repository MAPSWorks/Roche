#version 330
// PIXEL ATTRIBUTES
in vec2 pass_uv;
in vec3 pass_normal;
in vec4 pass_position;
in vec3 pass_lpos;
out vec4 out_color;

// TEXTURES
uniform sampler2D day_tex;
uniform vec3 rel_viewpos;

// GLOBAL VARIABLES
uniform vec3 view_pos; // Position of the camera

void main(void)
{
	// TEXTURE LOOKUPS
	vec3 color = texture(day_tex,pass_uv).rgb;

	out_color = vec4(color,1.0);
}