#version 330
in vec2 pass_uv;
in vec4 pass_position;

uniform sampler2D tex;
uniform vec4 ring_color;
uniform float minDist;

uniform sampler2DShadow shadow_map;
uniform mat4 lightMat;

out vec4 out_color;

vec2 poissonDisk[5] = vec2[](
  vec2( 0.0, 0.0),
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);

#define BIAS 0.004
#define PCF_SIZE 2048.0

void main(void)
{
    float dist = length(pass_uv);
    if (dist < minDist || dist > 1.0) discard;
    float matter = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;
    vec4 lightpos = (lightMat*pass_position);
    vec3 shadow_coords = (lightpos.xyz/lightpos.w)*vec3(0.5) + vec3(0.5);
    shadow_coords.z -= BIAS;

    float shadow=0.0;
    for (int i=0;i<5;++i)
    	shadow += texture(shadow_map,shadow_coords + vec3(poissonDisk[i]/PCF_SIZE, 0.0));
	shadow *= 0.20;
	out_color = matter*ring_color*vec4(vec3(shadow),1.0);
}