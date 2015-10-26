#version 330

in vec2 pass_uv;

uniform sampler2D tex;

out vec4 out_color;

void main(void)
{
  out_color = texture(tex, pass_uv);
}