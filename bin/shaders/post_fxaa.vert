#version 330

layout(location = 0)in vec2 in_position;

uniform sampler2D tex;
uniform float FXAA_SUBPIX_SHIFT = 1.0/4.0;

uniform vec2 off;

out vec4 pass_uv;

void main(void)
{
  pass_uv.xy = in_position;
  pass_uv.zw = in_position - (off*(0.5+FXAA_SUBPIX_SHIFT));
  gl_Position = vec4(in_position*2-vec2(1),0.0,1.0);
}