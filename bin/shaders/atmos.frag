#version 330

in vec3 pass_position;

uniform vec3 view_pos;
uniform vec3 light_dir;

uniform float planet_radius;
uniform float atmos_height;
uniform float scale_height;

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

out vec4 out_color;

float getNear(vec3 ray_origin, vec3 ray, float far)
{
  return 2*dot(ray,-view_pos) - far;
}

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
  return 0.75 + (1.0 + cc);
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

float density(vec3 point)
{
  return exp(-(length(point) - planet_radius)*SCALE_H);
}

float optic(vec3 a, vec3 b)
{
  vec3 step = (b-a)/float(OUT_SAMPLES);
  vec3 v = a+step*0.5;

  float sum = 0.0;
  for (int i=0;i<OUT_SAMPLES;++i)
  {
    sum += density(v);
    v += step;
  }
  return sum * length(step) * SCALE_L;
}

vec4 in_scattering(vec3 viewer, vec3 frag_pos, vec3 light_dir)
{
  vec3 view_dir = frag_pos-viewer;
  float far = length(view_dir);
  view_dir /= far;

  float near = getNear(viewer, view_dir, far);

  float len = (far-near)/float(IN_SAMPLES);
  vec3 step = view_dir*len;

  vec3 p = viewer+view_dir*near;
  vec3 v = p+step*0.5;

  vec3 sum = vec3(0.0);
  for (int i=0;i<IN_SAMPLES;++i)
  {
    float t = ray_sphere_far(v,light_dir,planet_radius+atmos_height);
    vec3 u = v+light_dir*t;
    float n = (optic(p,v)+optic(v,u))*(PI * 4.0);
    float dens = density(v);
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
  out_color = in_scattering(view_pos,pass_position,light_dir);
}