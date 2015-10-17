#version 330
in vec2 pass_uv;

uniform sampler2D tex;

uniform float exposure;

out vec4 out_color;

void main(void)
{
    out_color = vec4(texture(tex, pass_uv).rgb,1.0);
}