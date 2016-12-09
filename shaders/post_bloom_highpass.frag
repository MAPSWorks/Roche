#version 330

in vec2 pass_uv;

uniform sampler2D tex;
uniform float threshold;

out vec4 out_color;

void main(void)
{
  vec3 color = texture(tex, pass_uv).rgb;
  vec3 brightness = dot(color, vec3(0.299, 0.587, 0.114));
  if (brightness > threshold)
  out_color = vec4(color, 1.0);
}