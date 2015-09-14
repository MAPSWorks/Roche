#version 330
in vec2 pass_uv;

uniform sampler2D tex;

uniform float exposure;

out vec4 out_color;

void main(void)
{
    vec3 color = texture(tex, pass_uv).rgb;
	out_color = vec4((clamp(color, 1.0-exposure, 1.0) - 1.0 + exposure)/exposure,1.0);
}