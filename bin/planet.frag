#version 330
in vec2 pass_uv;
in vec3 pass_normal;
in vec4 pass_position;
in mat4 pass_tbn;

uniform sampler2D day_tex;
uniform sampler2D clouds_tex;
uniform sampler2D night_tex;
uniform sampler2DShadow shadow_map;
uniform float cloud_disp;

uniform vec3 light_dir;
uniform vec3 view_dir;

uniform mat4 lightMat;

vec3 sky_color = vec3(0.6,0.8,1.0);

out vec4 out_color;

#define CLOUD_ALT_RATIO 0.997
#define CLOUD_SHADOW 0.8
#define ATMO_RATIO 0.984
#define AMBIENT_LIGHT 0.04
#define RING_SHADOW_AMBIENT 0.4

#define M_PI 3.1415926535897932384626433832795

float max_angle = acos(CLOUD_ALT_RATIO)/(2*M_PI);

vec2 poissonDisk[5] = vec2[](
  vec2( 0.0, 0.0),
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);

#define BIAS 0.004
#define PCF_SIZE 4096.0

void main(void)
{
	float rawlight = dot(-light_dir, pass_normal);
	float light = clamp(rawlight,AMBIENT_LIGHT,1.0);
	vec3 day = texture(day_tex,pass_uv).rgb;
	vec2 cloud_offset = pass_uv + vec2(cloud_disp,0.0);

	vec4 to_light = vec4(normalize(light_dir-pass_position.xyz),1.0);
	vec3 lightnorm = (pass_tbn*to_light).xyz;
	vec2 shadow_tex_offset = lightnorm.xy*max_angle*vec2(1.0,0.5);

	vec4 to_viewer = vec4(normalize(view_dir-pass_position.xyz),1.0);
	vec3 viewnorm = (pass_tbn*to_viewer).xyz;
	vec2 cloud_tex_offset = viewnorm.xy*max_angle*vec2(1.0,0.5);
	float cloud_shadow = max(0.0,texture(clouds_tex, cloud_offset + shadow_tex_offset).r);
	day *= max(0.0,1.0 - cloud_shadow*CLOUD_SHADOW);

	float angle = 1.0 - dot(pass_normal, to_viewer.xyz);
	angle = pow(angle, 3);

	float cloud = texture(clouds_tex, cloud_offset + cloud_tex_offset).r;
	float nightlights = clamp(-rawlight*12.0+1.0,0.0,1.0);
	vec3 color = mix(day*light  + nightlights*texture(night_tex, pass_uv).rgb, vec3(light), cloud);
	float rim = (angle*angle*angle);

	color = mix(color, sky_color*light, angle);
	out_color = vec4(mix(color, sky_color*2, rim*light), 1.0-(rim*rim*rim));

	vec4 lightpos = (lightMat*pass_position);
	if ((mat3(lightMat)*pass_normal).z < 0.0) {
		vec3 shadow_coords = (lightpos.xyz/lightpos.w)*vec3(0.5) + vec3(0.5);
		shadow_coords.z -= BIAS;
		float shadow=0.0;
		for (int i=0;i<5;++i)
			shadow += texture(shadow_map,shadow_coords + vec3(poissonDisk[i]/PCF_SIZE, 0.0)) + RING_SHADOW_AMBIENT;
		shadow *= 0.20;
		shadow = clamp(shadow, AMBIENT_LIGHT, 1.0);
		out_color *= vec4(vec3(shadow),1.0);
	}
}