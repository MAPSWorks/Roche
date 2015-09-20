#version 330
in vec2 pass_uv;
in vec4 pass_position;

uniform sampler2D tex;
uniform vec4 ring_color;
uniform float minDist;

uniform mat4 lightMat;

#define RING_SHARPNESS 200

out vec4 out_color;

void main(void)
{
  float dist = length(pass_uv);
  if (dist < minDist || dist > 1.0) discard;
  float matter = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;

  vec4 lightpos = (transpose(inverse(lightMat))*pass_position);
  vec3 shadowcoords = lightpos.xyz/lightpos.w;
  float shadow = 1.0;
  if (shadowcoords.z > 0.0)
    shadow = 1.0-clamp((1.0-length(shadowcoords.xy))*RING_SHARPNESS,0.0,1.0);

	out_color = matter*ring_color*vec4(vec3(shadow),1.0);
}