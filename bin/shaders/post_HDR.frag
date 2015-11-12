#version 330

in vec2 pass_uv;

uniform sampler2D tex;
uniform float exposure;

out vec4 out_color;

void main(void)
{
  vec3 color = texture(tex, pass_uv).rgb;
  out_color = vec4(vec3(1.0) - exp(-color*exposure), 1.0);

}