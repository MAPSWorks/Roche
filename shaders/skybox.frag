#version 330
in vec2 pass_uv;

uniform sampler2D tex;

out vec4 out_color;

void main(void)
{
    out_color = vec4(texture(tex, pass_uv).rgb*0.2,1.0);
}