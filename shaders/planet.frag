#version 330
// PIXEL ATTRIBUTES
in vec2 pass_uv;
in vec3 pass_normal;
in vec4 pass_position;
in vec3 pass_lpos;
out vec4 out_color;

// TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D clouds_tex;
uniform sampler2D night_tex;
uniform sampler2D ring_tex;

// GLOBAL VARIABLES
uniform float cloud_disp; // Amount of displacement of cloud on x axis
uniform float ring_inner; // Ring's inner radius
uniform float ring_outer; // Ring's outer radius
uniform vec3 light_dir; // Towards the light source
uniform vec3 view_pos; // Position of the camera
uniform vec3 ring_vec; // Normal vector of the ring plane

uniform vec3 rel_viewpos;

// GAUSS KERNEL /W LERP
uniform float offset[3] = float[]( 0.0, 1.3846153846, 3.2307692308 );
uniform float weight[3] = float[]( 0.2270270270, 0.3162162162, 0.0702702703 );
// Ring texture size for gaussian offsets
float ringtex_size = 1.0/float(textureSize(ring_tex,0).x);

// This should be uniform variables
const float AMBIENT_LIGHT = 0.04;
const float RING_AMBIENT = 0.1;
const float CITY_LIGHT_INTENSITY = 0.6;

uniform float planet_radius;
uniform float atmos_height;
uniform float scale_height;

uniform sampler2D lookup;

float SCALE_H = 1.0/scale_height;
float SCALE_L = 1.0/(atmos_height);

#define OUT_SAMPLES 5
#define IN_SAMPLES 5

#define PI 3.14159265359

uniform float K_R; // Rayleigh scattering constant
uniform float K_M; // Mie scattering constant
uniform float E; // Sunlight intensity
uniform vec3 C_R; // 1 / sunlight wavelength ^4
uniform float G_M; // Mie g constant

float ray_sphere_far(vec3 ori, vec3 ray, float radius)
{
  float b = dot(ori, ray);
  float c = dot(ori,ori) - radius*radius;
  return -b+sqrt(b*b-c);
}

float ray_sphere_near(vec3 ori, vec3 ray, float radius)
{
  float b = dot(ori, ray);
  float c = dot(ori,ori) - radius*radius;
  return -b-sqrt(b*b-c);
}

float rayleigh(float cc)
{
  return 0.75 * (1.0 + cc);
}

float mie(float g, float c, float cc)
{
  float gg = g*g;
  float a = (1.0-gg)*(1.0+cc);
  float b = 1.0 + gg - 2.0*g*c;
  b*= sqrt(b);
  b*= 2.0+gg;

  return 1.5*a/b;
}

vec4 in_scattering(vec3 viewer, vec3 frag_pos, vec3 light_dir)
{
  vec3 view_dir = frag_pos-viewer;
  float far = length(view_dir);
  view_dir /= far;

  float near = ray_sphere_near(viewer, view_dir, planet_radius+atmos_height);

  float len = (far-near)/float(IN_SAMPLES);
  vec3 step = view_dir*len;

  vec3 p = viewer+view_dir*near;
  vec3 v = p+step*0.5;

  vec3 sum = vec3(0.0);
  for (int i=0;i<IN_SAMPLES;++i)
  {
    float t = ray_sphere_far(v,light_dir,planet_radius+atmos_height);
    vec3 u = v+light_dir*t;

    float alt = (length(v)-planet_radius)/atmos_height;
    vec3 norm_v = normalize(v);

    float angle_view = acos(dot(norm_v, -view_dir))/PI;
    float angle_light = acos(dot(norm_v, light_dir))/PI;

    float n = texture(lookup, vec2(alt,angle_view)).g +
              texture(lookup, vec2(alt,angle_light)).g;
    float dens = texture(lookup,vec2(alt,0.0)).r;
    sum += dens * exp(-n*(K_R*C_R+K_M));
    v += step;
  }

  sum *= len * SCALE_L;

  float c = dot(view_dir,-light_dir);
  float cc = c*c;

  vec3 color = sum * (K_R*C_R*rayleigh(cc) + K_M*mie(G_M, c,cc))*E;

  return color.rgbb;
}

void main(void)
{
	// TEXTURE LOOKUPS
	vec3 day = texture(diffuse_tex,pass_uv).rgb;
	vec3 night = texture(night_tex, pass_uv).rgb;
	vec4 cloud = texture(clouds_tex, pass_uv + vec2(cloud_disp,0.0));

	// LIGHT CALCULATION
	float rawlight = dot(-light_dir, pass_normal);
	float light = clamp(rawlight,AMBIENT_LIGHT,1.0);

	// TEXTURE COMPOSITION
	float nightlights = clamp(-rawlight*12.0+1.0,0.0,1.0);
	vec3 color = mix(day*light + nightlights*night*CITY_LIGHT_INTENSITY, light*cloud.rgb, cloud.a);

	// SHADOW CALCULATION (RAYTRACING)
	float t = dot(pass_lpos.xyz, ring_vec)/dot(light_dir,ring_vec);
	vec4 projPos = vec4(pass_lpos.xyz-t*light_dir,1.0);
	float dist = length(projPos.xyz);
	float shadow = 0.0;
	float tex_offset = (dist-ring_inner)/(ring_outer-ring_inner);
	for (int i=0;i<3;++i) // Size 5 gauss filter
	{
		shadow += texture(ring_tex, vec2(tex_offset + offset[i]*ringtex_size,0.0)).r * weight[i];
		shadow += texture(ring_tex, vec2(tex_offset - offset[i]*ringtex_size,0.0)).r * weight[i];
	}

  shadow = mix(1.0,shadow*(1-RING_AMBIENT) + RING_AMBIENT,dist > ring_inner && dist < ring_outer && t>=0);
  color *= mix(shadow,1.0,nightlights);

  // Atmospheric scattering
	if (atmos_height >0)
	{
    vec3 tlpos = normalize(pass_lpos)*planet_radius;
		vec4 scat = in_scattering(rel_viewpos, tlpos, -light_dir);
    color *= 1.0-scat.w;
    color += scat.xyz;
	}
	out_color = vec4(color,1.0);
}