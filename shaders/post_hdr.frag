#version 330

in vec2 pass_uv;

uniform sampler2D tex;
uniform float exposure;

out vec4 out_color;

const float gamma = 2.2;

void main(void)
{
  vec3 color = texture(tex, pass_uv).rgb;
  color = vec3(1.0) - exp(-color*exposure);
  out_color = vec4(color, 1.0);
}