#include "planet.h"
#include "opengl.h"

#include <stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"

#include <iostream>

Texture Planet::no_night;
Texture Planet::no_clouds;

glm::mat4 computeLightMatrix(glm::vec3 light_dir, glm::vec3 light_up, float planet_size, float ring_outer)
{
  glm::mat4 light_mat;
  light_up = glm::normalize(light_up);
  light_dir = - glm::normalize(light_dir);
  glm::vec3 light_right = glm::normalize(glm::cross(light_dir, light_up));
  light_dir *= ring_outer;
  light_up = glm::normalize(glm::cross(light_dir, light_right)) * planet_size;
  light_right *= planet_size;
  int i;
  for (i=0;i<3;++i)
  {
    light_mat[i][0] = light_right[i];
    light_mat[i][1] = light_up[i];
    light_mat[i][2] = -light_dir[i];
  }
  return light_mat;
}

void computeRingMatrix(glm::vec3 toward_view, glm::vec3 rings_up, float size, glm::mat4 *near_mat, glm::mat4 *far_mat)
{
  glm::mat4 near_mat_temp = glm::mat4(1.0);
  glm::mat4 far_mat_temp = glm::mat4(1.0);
  rings_up = glm::normalize(rings_up);
  toward_view = glm::normalize(toward_view);

  glm::vec3 rings_right = glm::normalize(glm::cross(rings_up, toward_view));
  glm::vec3 rings_x = glm::normalize(glm::cross(rings_up, rings_right));
  int i;
  for (i=0;i<3;++i)
  {
    near_mat_temp[0][i] = rings_x[i]*size;
    near_mat_temp[1][i] = rings_right[i]*size;
    near_mat_temp[2][i] = rings_up[i]*size;
    far_mat_temp[0][i] = -rings_x[i]*size;
    far_mat_temp[1][i] = -rings_right[i]*size;
    far_mat_temp[2][i] = -rings_up[i]*size;
  }
  *near_mat *= near_mat_temp;
  *far_mat *= far_mat_temp;
}

#define RING_ITERATIONS 100

void generate_rings(unsigned char *buffer, int size, int seed)
{
  // Starting fill
  int i,j;
  const int ref_size = 4096;
  float *ref_buffer = new float[ref_size];
  for (i=0;i<ref_size;++i)
  {
    ref_buffer[i] = 1.0;
  }
  srand(seed);

  // gap generation
  const int max_gapsize = ref_size/20;
  for (i=0;i<RING_ITERATIONS;++i)
  {
    int gapsize = rand()%(max_gapsize);
    int gap = rand()%(ref_size-gapsize+1);
    float gap_opacity = rand()%RAND_MAX/(float)RAND_MAX;
    if (gap_opacity < 0.4) gap_opacity = 0.4;
    for (j=gap;j<gap+gapsize;++j)
    {
      ref_buffer[j] *= gap_opacity;
    }
  }
  // brightness equalization
  float mean = 0;
  for (i=0;i<ref_size;++i)
  {
    mean += ref_buffer[i];
  }
  mean /= ref_size;
  float mul = 1.0/mean;
  for (i=0;i<ref_size;++i)
  {
    ref_buffer[i] *= mul;
  }

  // fading
  const int fade = ref_size/10;
  for (i=0;i<fade;++i)
  {
    ref_buffer[ref_size-i-1] *= i/(float)fade; 
    ref_buffer[i] *= i/(float)fade;
  }
  float scale = ref_size/(float)size;
  for (i=0;i<size;++i)
  {
    float mean = 0.0;
    for (j=i*scale;j<(i+1)*scale;++j)
    {
      mean += ref_buffer[j];
    }
    mean /= scale;
    buffer[i] = (unsigned char)(mean*255);
  }
  free(ref_buffer);
}

Planet::Planet()
{
  name = "undefined";
  pos = glm::vec3(0,0,0);
  rot_axis = glm::vec3(0,0,1);
  rot_rate = 0;
  rot_angle = 0;
  radius = 1000;

  ring_inner = 2000;
  ring_outer = 4000;
  ring_upvector = glm::vec3(0,0,1);
  ring_seed = 0;
  ring_color = glm::vec4(0.6,0.6,0.6,1.0);
  has_rings = false;

  atmos_color = glm::vec3(0.0,0.0,0.0);
  cloud_epoch = 0;

  ecc = 0;
  sma = 10000;
  inc = 0;
  lan = 0;
  arg = 0;
  m0 = 0;
  GM = 100000;

  parent = NULL;
  has_clouds_tex = false;
  has_night_tex = false;

  is_sun = false;

  loaded = false;
}

void Planet::load()
{
  if (!loaded)
  {
    if (has_rings)
    {
      const int ringsize = 2048;
      unsigned char *rings = new unsigned char[ringsize];
      generate_rings(rings, ringsize, ring_seed);

      ring.create();
      ring.update(TexMipmapData(false, &ring, 0, GL_DEPTH_COMPONENT, ringsize, 1, GL_UNSIGNED_BYTE, rings));
      ring.genMipmaps();
    }
    loaded = true;
  }
}

void Planet::unload()
{
  if (loaded)
  {
    ring.destroy();
    day.destroy();
    night.destroy();
    clouds.destroy();
    loaded = false;
  }
}

void Planet::render(glm::mat4 proj_mat, glm::mat4 view_mat, glm::vec3 view_pos, glm::vec3 light_pos, glm::vec3 focused_planet_pos, Shader &planet_shader, Shader &sun_shader, Shader &ring_shader, Renderable &planet_obj, Renderable &ring_obj)
{
  Shader &pshad = is_sun?sun_shader:planet_shader;

  glm::vec3 render_pos = pos-focused_planet_pos;

  glm::mat4 planet_mat = glm::translate(glm::mat4(), render_pos);

  glm::quat q = glm::rotate(glm::quat(), rot_angle, rot_axis);
  planet_mat *= mat4_cast(q);
  planet_mat = glm::scale(planet_mat, glm::vec3(radius));

  glm::vec3 light_dir = glm::normalize(pos - light_pos);
  
  glm::mat4 light_mat = computeLightMatrix(light_dir, glm::vec3(0,0,1), radius, ring_outer);
  
  glm::mat4 far_ring_mat, near_ring_mat;
  far_ring_mat = glm::translate(far_ring_mat, render_pos);
  near_ring_mat = glm::translate(near_ring_mat, render_pos);
  computeRingMatrix(render_pos - view_pos, ring_upvector, ring_outer, &near_ring_mat, &far_ring_mat);

  if (has_rings)
  {
    // FAR RING RENDER
    ring_shader.use();
    ring_shader.uniform( "projMat", glm::value_ptr(proj_mat));
    ring_shader.uniform( "viewMat", glm::value_ptr(view_mat));
    ring_shader.uniform( "modelMat", glm::value_ptr(far_ring_mat));
    ring_shader.uniform( "lightMat", glm::value_ptr(light_mat));
    ring_shader.uniform( "ring_color", glm::value_ptr(ring_color));
    ring_shader.uniform( "tex", 0);
    ring_shader.uniform( "minDist", ring_inner/ring_outer);
    ring.use(0);
    ring_obj.render();
  }

  // PLANET RENDER
  pshad.use();
  pshad.uniform( "projMat", glm::value_ptr(proj_mat));
  pshad.uniform( "viewMat", glm::value_ptr(view_mat));
  pshad.uniform( "modelMat", glm::value_ptr(planet_mat));
  pshad.uniform( "ring_vec", glm::value_ptr(ring_upvector));
  pshad.uniform( "light_dir", glm::value_ptr(light_dir));
  pshad.uniform( "cloud_disp", cloud_epoch);
  pshad.uniform( "view_pos", glm::value_ptr(view_pos));
  pshad.uniform( "sky_color", glm::value_ptr(atmos_color));
  pshad.uniform( "ring_inner", ring_inner);
  pshad.uniform( "ring_outer", ring_outer);
  pshad.uniform( "day_tex", 0);
  pshad.uniform( "clouds_tex", 1);
  pshad.uniform( "night_tex", 2);
  pshad.uniform( "ring_tex", 3);
  day.use(0);
  if (has_clouds_tex) clouds.use(1); else no_clouds.use(1);
  if (has_night_tex) night.use(2); else no_night.use(2);
  if (has_rings) ring.use(3); else no_clouds.use(3);
  planet_obj.render();

  if (has_rings)
  {
    // NEAR RING RENDER
    ring_shader.use();
    ring_shader.uniform( "projMat", glm::value_ptr(proj_mat));
    ring_shader.uniform( "viewMat", glm::value_ptr(view_mat));
    ring_shader.uniform( "modelMat", glm::value_ptr(near_ring_mat));
    ring_shader.uniform( "lightMat", glm::value_ptr(light_mat));
    ring_shader.uniform( "ring_color", glm::value_ptr(ring_color));
    ring_shader.uniform( "tex", 0);
    ring_shader.uniform( "minDist", ring_inner/ring_outer);
    ring.use(0);
    ring_obj.render();
  }
}

void Skybox::load()
{

}

void Skybox::render(glm::mat4 proj_mat, glm::mat4 view_mat, Shader &skybox_shader, Renderable &o)
{
  glm::quat q = glm::rotate(glm::quat(), rot_angle, rot_axis);
  glm::mat4 skybox_mat = glm::mat4_cast(q);
  skybox_mat = glm::scale(skybox_mat, glm::vec3(size));
  
  // SKYBOX RENDER
  skybox_shader.use();
  skybox_shader.uniform("projMat", glm::value_ptr(proj_mat));
  skybox_shader.uniform("viewMat", glm::value_ptr(view_mat));
  skybox_shader.uniform("modelMat", glm::value_ptr(skybox_mat));
  skybox_shader.uniform("tex", 0);
  tex.use(0);
  o.render(); 
}

// MATH STUFF

#define PI 3.14159265358979323846264338327950288 

void Planet::update(double epoch)
{
  rot_angle = epoch*rot_rate;
  if (parent != NULL)
  {
    double meanAnomaly = sqrt(parent->GM / (sma*sma*sma))*epoch + m0;
    double En = (ecc<0.8)?meanAnomaly:PI;
    const int it = 10;
    for (int i=0;i<it;++i)
      En -= (En - ecc*sin(En)-meanAnomaly)/(1-ecc*cos(En));
    double trueAnomaly = atan2(sqrt(1+ecc)*sin(En/2), sqrt(1-ecc)*cos(En/2));
    double dist = sma*((1-ecc*ecc)/(1+ecc*cos(trueAnomaly)));
    glm::dvec3 posInPlane = glm::vec3(sin(trueAnomaly)*dist,cos(trueAnomaly)*dist,0.0);
    glm::dquat q = glm::rotate(glm::rotate(glm::rotate(glm::dquat(), arg*PI/180.0,glm::dvec3(0,0,1)),inc*PI/180, glm::dvec3(1,0,0)),lan*PI/180, glm::dvec3(1,0,0));
    pos = q*posInPlane;
  }
}