#include "planet.h"
#include "opengl.h"

#include <stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

#define PLANET_STRIDE 24

void render_planet()
{
  glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)0);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)16);
}

void render_rings()
{
  glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
}

Planet::Planet()
{
  loaded = false;
}

void Planet::load()
{
  if (!loaded)
  {
    const int ringsize = 2048;
    unsigned char *rings = new unsigned char[ringsize];
    generate_rings(rings, ringsize, ring_seed);

    ring.create();
    ring.image(1, ringsize, 1, (void*)rings);
    delete [] rings;
    
    day.load_from_file(day_filename.c_str(), 3);
    clouds.load_from_file(clouds_filename.c_str(), 3);
    night.load_from_file(night_filename.c_str(), 3);
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

void Planet::render(glm::mat4 proj_mat, glm::mat4 view_mat, glm::vec3 view_pos, glm::vec3 light_pos, Shader &planet_shader, Shader &ring_shader, Renderable &planet_obj, Renderable &ring_obj)
{
  glm::mat4 planet_mat = glm::translate(glm::mat4(), pos);

  glm::quat q = glm::rotate(glm::quat(), rot_epoch, rot_axis);
  planet_mat *= mat4_cast(q);
  planet_mat = glm::scale(planet_mat, glm::vec3(radius));

  glm::vec3 light_dir = glm::normalize(pos - light_pos);
  
  glm::mat4 light_mat = computeLightMatrix(light_dir, glm::vec3(0,0,1), radius, ring_outer);
  
  glm::mat4 far_ring_mat, near_ring_mat;
  far_ring_mat = glm::translate(far_ring_mat, pos);
  near_ring_mat = glm::translate(near_ring_mat, pos);
  computeRingMatrix(pos - view_pos, ring_upvector, ring_outer, &near_ring_mat, &far_ring_mat);

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
    ring_obj.render(render_rings);
  }

  // PLANET RENDER
  planet_shader.use();
  planet_shader.uniform( "projMat", glm::value_ptr(proj_mat));
  planet_shader.uniform( "viewMat", glm::value_ptr(view_mat));
  planet_shader.uniform( "modelMat", glm::value_ptr(planet_mat));
  planet_shader.uniform( "ring_vec", glm::value_ptr(ring_upvector));
  planet_shader.uniform( "light_dir", glm::value_ptr(light_dir));
  planet_shader.uniform( "cloud_disp", cloud_epoch);
  planet_shader.uniform( "view_pos", glm::value_ptr(view_pos));
  planet_shader.uniform( "sky_color", glm::value_ptr(atmos_color));
  planet_shader.uniform( "ring_inner", ring_inner);
  planet_shader.uniform( "ring_outer", ring_outer);
  planet_shader.uniform( "day_tex", 0);
  planet_shader.uniform( "clouds_tex", 1);
  planet_shader.uniform( "night_tex", 2);
  planet_shader.uniform( "ring_tex", 3);
  day.use(0);
  clouds.use(1);
  night.use(2);
  ring.use(3);
  planet_obj.render(render_planet);

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
    ring_obj.render(render_rings);
  }
}

void Skybox::load()
{
  tex.load_from_file(tex_filename.c_str(), 3);
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
  o.render(render_planet); 
}
