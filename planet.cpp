#include "planet.h"
#include "game.h"
#include "opengl.h"
#include "util.h"

#include <stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#define PI 3.14159265358979323846264338327950288 

Texture PhysicalProperties::no_night;
Texture PhysicalProperties::no_clouds;
Texture RingProperties::no_rings;
bool PhysicalProperties::no_tex_init = false;
bool RingProperties::no_rings_init = false;

/// RENDER HELPER FUNCTIONS

glm::mat4 PhysicalProperties::computeLightMatrix(const glm::vec3 &light_dir,const glm::vec3 &light_up, float planet_size, float ring_outer)
{
  glm::mat4 light_mat;
  glm::vec3 nlight_up = glm::normalize(light_up);
  glm::vec3 nlight_dir = - glm::normalize(light_dir);
  glm::vec3 light_right = glm::normalize(glm::cross(nlight_dir, nlight_up));
  nlight_dir *= ring_outer;
  nlight_up = glm::normalize(glm::cross(nlight_dir, light_right)) * planet_size;
  light_right *= planet_size;
  int i;
  for (i=0;i<3;++i)
  {
    light_mat[i][0] = light_right[i];
    light_mat[i][1] = nlight_up[i];
    light_mat[i][2] = -nlight_dir[i];
  }
  return light_mat;
}

void PhysicalProperties::computeRingMatrix(glm::vec3 toward_view, glm::vec3 rings_up, float size, glm::mat4 &near_mat, glm::mat4 &far_mat)
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
  near_mat *= near_mat_temp;
  far_mat *= far_mat_temp;
}

/// RING GENERATION HELPER FUNCTION

#define RING_ITERATIONS 100

void RingProperties::generateRings(unsigned char *buffer, int size, int seed)
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

RenderContext::RenderContext(Shader &ps, Shader &ss, Shader &rs,
      Renderable &po, Renderable &ro) :
        planet_shader(ps),
        sun_shader(ss),
        ring_shader(rs),
        planet_obj(po),
        ring_obj(ro)
{
  
}

void OrbitalParameters::setParameters(const std::string &parent_body, double ecc, double sma, double inc, double lan, double arg, double m0)
{
  this->ecc = ecc;
  this->sma = sma;
  this->inc = inc;
  this->lan = lan;
  this->arg = arg;
  this->m0 = m0;
  this->parent_body = parent_body;
  this->parent =  NULL;
  this->position = glm::vec3(0,0,0);
}
void OrbitalParameters::computePosition(double epoch)
{
  if (parent)
  { 
    double meanAnomaly = sqrt(parent->getPhysicalProperties().getGM() / (sma*sma*sma))*epoch + m0;
    double En = (ecc<0.8)?meanAnomaly:PI;
    const int it = 10;
    for (int i=0;i<it;++i)
      En -= (En - ecc*sin(En)-meanAnomaly)/(1-ecc*cos(En));
    double trueAnomaly = atan2(sqrt(1+ecc)*sin(En/2), sqrt(1-ecc)*cos(En/2));
    double dist = sma*((1-ecc*ecc)/(1+ecc*cos(trueAnomaly)));
    glm::dvec3 posInPlane = glm::vec3(sin(trueAnomaly)*dist,cos(trueAnomaly)*dist,0.0);
    glm::dquat q = glm::rotate(glm::rotate(glm::rotate(glm::dquat(), arg*PI/180.0,glm::dvec3(0,0,1)),inc*PI/180, glm::dvec3(1,0,0)),lan*PI/180, glm::dvec3(1,0,0));
    position = q*posInPlane;
  }
  else
  {
    position = glm::vec3(0,0,0);
  }
}

const glm::vec3 &OrbitalParameters::getPosition() const
{
  return position;
}

void OrbitalParameters::setParentFromName(const std::deque<Planet> &planets)
{
  for (auto it: planets)
  {
    if (it.getName() == parent_body)
    {
      parent = &it;
      return;
    }
  }
  std::cout << "Can't find parent body " << parent_body << std::endl;
}

void PhysicalProperties::setProperties(
      float radius,
      const glm::vec3 rot_axis,
      float rotation_rate,
      glm::vec3 mean_color,
      glm::vec3 atmosphere_color,
      double GM,
      bool is_star,
      const std::string &diffuse_filename,
      const std::string &night_filename,
      const std::string &cloud_filename,
      float cloud_disp_rate)
{
  this->radius = radius;
  this->rotation_axis = rot_axis;
  this->rotation_rate = rotation_rate;
  this->mean_color = mean_color;
  this->atmosphere_color =  atmosphere_color;
  this->GM = GM;
  this->is_star = is_star;
  this->diffuse_filename = diffuse_filename;
  this->night_filename = night_filename;
  this->cloud_filename = cloud_filename;

  this->has_night_tex = night_filename!="";
  this->has_cloud_tex = cloud_filename!="";
  this->cloud_disp_rate = cloud_disp_rate;
  loaded = false;
}

void PhysicalProperties::load()
{
  if (!no_tex_init)
  {
    no_night.create();
    no_clouds.create();
    unsigned char *black = new unsigned char[4]{0,0,0,255};
    unsigned char *trans = new unsigned char[4]{255,255,255,0};
    TexMipmapData(false, no_night, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, black).updateTexture();
    TexMipmapData(false, no_clouds, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, trans).updateTexture();
    no_tex_init = true;
  }
  if (!loaded)
  {
    diffuse_tex.create();
    Game::loadTexture(diffuse_filename, diffuse_tex);
    if (has_night_tex) 
    {
      night_tex.create();
      Game::loadTexture(night_filename, night_tex);
    }
    if (has_cloud_tex)
    {
      cloud_tex.create();
      Game::loadTexture(cloud_filename, cloud_tex);
    }
    loaded = true;
  }
}
void PhysicalProperties::unload()
{
  if (loaded)
  {
    diffuse_tex.destroy();
    night_tex.destroy();
    cloud_tex.destroy();
    loaded = false;
  }
}

void PhysicalProperties::update(double epoch)
{
  rotation_angle = rotation_rate*epoch;
  cloud_disp = cloud_disp_rate*epoch;
}

void PhysicalProperties::render(const glm::vec3 &pos, const RenderContext &rc, const RingProperties &rings)
{
  Shader &pshad = is_star?rc.sun_shader:rc.planet_shader;

  glm::vec3 render_pos = pos-rc.view_center;

  glm::mat4 planet_mat = glm::translate(glm::mat4(), render_pos);

  glm::quat q = glm::rotate(glm::quat(), rotation_angle, rotation_axis);
  planet_mat *= mat4_cast(q);
  planet_mat = glm::scale(planet_mat, glm::vec3(radius));

  glm::vec3 light_dir = glm::normalize(pos - rc.light_pos);

  glm::mat4 light_mat = computeLightMatrix(light_dir, glm::vec3(0,0,1), radius, rings.getOuter());
  
  glm::mat4 far_ring_mat, near_ring_mat;
  far_ring_mat = glm::translate(far_ring_mat, render_pos);
  near_ring_mat = glm::translate(near_ring_mat, render_pos);
  computeRingMatrix(render_pos - rc.view_pos, rings.getNormal(), rings.getOuter(), near_ring_mat, far_ring_mat);

  rings.render(far_ring_mat, light_mat, rc);

  // PLANET RENDER
  pshad.use();
  pshad.uniform( "projMat", glm::value_ptr(rc.proj_mat));
  pshad.uniform( "viewMat", glm::value_ptr(rc.view_mat));
  pshad.uniform( "modelMat", glm::value_ptr(planet_mat));
  pshad.uniform( "ring_vec", glm::value_ptr(rings.getNormal()));
  pshad.uniform( "light_dir", glm::value_ptr(light_dir));
  pshad.uniform( "cloud_disp", cloud_disp);
  pshad.uniform( "view_pos", glm::value_ptr(rc.view_pos));
  pshad.uniform( "sky_color", glm::value_ptr(atmosphere_color));
  pshad.uniform( "ring_inner", rings.getInner());
  pshad.uniform( "ring_outer", rings.getOuter());
  pshad.uniform( "diffuse_tex", 0);
  pshad.uniform( "clouds_tex", 1);
  pshad.uniform( "night_tex", 2);
  pshad.uniform( "ring_tex", 3);
  diffuse_tex.use(0);
  if (has_cloud_tex) cloud_tex.use(1); else no_clouds.use(1);
  if (has_night_tex) night_tex.use(2); else no_night.use(2);
  rings.useTexture(3);
  rc.planet_obj.render();

  rings.render(near_ring_mat, light_mat, rc);
}

double PhysicalProperties::getGM()
{
  return GM;
}

RingProperties::RingProperties()
{
  has_rings = false;
}
void RingProperties::setProperties(float inner, float outer, const glm::vec3 &normal, int seed, const glm::vec4 &color)
{
  this->inner = inner;
  this->outer = outer;
  this->up = normal;
  this->seed = seed;
  this->color = color;
}
void RingProperties::load()
{
  if (!no_rings_init)
  {
    no_rings.create();
    unsigned char *trans = new unsigned char[4]{255,255,255,0};
    TexMipmapData(false, no_rings, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, trans).updateTexture();
    no_rings_init = true;
  }
  if (!loaded && has_rings)
  {
    const int ringsize = 4096;
    unsigned char *rings = new unsigned char[ringsize];
    generateRings(rings, ringsize, seed);

    tex.create();
    TexMipmapData(false, tex, 0, GL_DEPTH_COMPONENT, ringsize, 1, GL_UNSIGNED_BYTE, rings).updateTexture();
    tex.genMipmaps();
    loaded = true;
  }
}
void RingProperties::unload()
{
  if (loaded)
  {
    tex.destroy();
    loaded = false;
  }
}

void RingProperties::render(const glm::mat4 &model_mat,  const glm::mat4 &light_mat, const RenderContext &rc) const
{
  if (has_rings)
  {
    // FAR RING RENDER
    rc.ring_shader.use();
    rc.ring_shader.uniform( "projMat", glm::value_ptr(rc.proj_mat));
    rc.ring_shader.uniform( "viewMat", glm::value_ptr(rc.view_mat));
    rc.ring_shader.uniform( "modelMat", glm::value_ptr(model_mat));
    rc.ring_shader.uniform( "lightMat", glm::value_ptr(light_mat));
    rc.ring_shader.uniform( "ring_color", glm::value_ptr(color));
    rc.ring_shader.uniform( "tex", 0);
    rc.ring_shader.uniform( "minDist", inner/outer);
    tex.use(0);
    rc.ring_obj.render();
  }
}

void RingProperties::useTexture(int unit) const
{
  if (has_rings) tex.use(unit); else no_rings.use(unit);
}

float RingProperties::getInner() const
{
  return inner;
}
float RingProperties::getOuter() const
{
  return outer;
}
const glm::vec3 &RingProperties::getNormal() const
{
  return up;
}

Planet::Planet()
{
  name = "undefined";
}

void Planet::load()
{
  phys.load();
  ring.load();
}

void Planet::unload()
{
  phys.unload();
  ring.unload();
}

void Planet::update(double epoch)
{
  orbit.computePosition(epoch);
  phys.update(epoch);
}

void Planet::setParentBody(const std::deque<Planet> &planets)
{
  orbit.setParentFromName(planets);
}

void Planet::render(const RenderContext &rc)
{
  phys.render(orbit.getPosition(), rc, ring);
}

OrbitalParameters &Planet::getOrbitalParameters()
{
  return orbit;
}
PhysicalProperties &Planet::getPhysicalProperties()
{
  return phys;
}
RingProperties &Planet::getRingProperties()
{
  return ring;
}

const glm::vec3 &Planet::getPosition()
{
  return orbit.getPosition();
}

void Skybox::load()
{

}

void Skybox::render(const glm::mat4 &proj_mat,const glm::mat4 &view_mat, Shader &skybox_shader, Renderable &o)
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