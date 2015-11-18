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

#include "shaun/sweeper.hpp"
#include <glm/ext.hpp>

#define PI 3.14159265358979323846264338327950288 

#define SCATTERING_SAMPLES (50)

/// RING GENERATION HELPER FUNCTION

#define RING_ITERATIONS 100

void Ring::generateRings(unsigned char *buffer, int size, int seed)
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

Orbit::Orbit()
{
  this->parent =  NULL;
  this->updated = false;
  this->parent_body = "";
  this->position = glm::vec3(0,0,0);
}

void Orbit::setParameters(const std::string &parent_body, double ecc, double sma, double inc, double lan, double arg, double m0)
{
  this->ecc = ecc;
  this->sma = sma;
  this->inc = glm::radians(inc);
  this->lan = glm::radians(lan);
  this->arg = glm::radians(arg);
  this->m0 = glm::radians(m0);
  this->parent_body = parent_body;
}

void Orbit::computePosition(double epoch)
{
  if (!updated)
  {
    if (parent)
    {
      double orbital_period = 2*PI*sqrt((sma*sma*sma)/parent->getBody().GM);
      double mean_motion = 2*PI/orbital_period;
      double meanAnomaly = fmod(epoch*mean_motion + m0, 2*PI);
      double En = (ecc<0.8)?meanAnomaly:PI;
      const int it = 20;
      for (int i=0;i<it;++i)
        En -= (En - ecc*sin(En)-meanAnomaly)/(1-ecc*cos(En));
      double trueAnomaly = 2*atan2(sqrt(1+ecc)*sin(En/2), sqrt(1-ecc)*cos(En/2));
      double dist = sma*((1-ecc*ecc)/(1+ecc*cos(trueAnomaly)));
      glm::dvec3 posInPlane = glm::vec3(-sin(trueAnomaly)*dist,cos(trueAnomaly)*dist,0.0);
      glm::dquat q = glm::dquat();
      q = glm::rotate(q, lan, glm::dvec3(0,0,1));
      q = glm::rotate(q, inc, glm::dvec3(0,1,0));
      q = glm::rotate(q, arg, glm::dvec3(0,0,1));
      position = q*posInPlane;

      parent->getOrbit().computePosition(epoch);

      position += parent->getPosition();
      updated = true;
    }
    else
    {
      position = glm::dvec3(0,0,0);
    }
  }
}

const glm::dvec3 &Orbit::getPosition() const
{
  return position;
}

void Orbit::setParentFromName(std::deque<Planet> &planets)
{
  if (parent_body != "")
  {
    for (Planet &it: planets)
    {
      if (it.getName() == parent_body)
      {
        parent = &it;
        return;
      }
    }
    std::cout << "Can't find parent body " << parent_body << std::endl;
  }
}

bool Orbit::isUpdated()
{
  return updated;
}

void Orbit::reset()
{
  updated = false;
}

Body::Body()
{

}

void Body::load()
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
}
void Body::unload()
{
  diffuse_tex.destroy();
  if (has_night_tex)
    night_tex.destroy();
  if (has_cloud_tex)
    cloud_tex.destroy();
}

void Body::update(double epoch)
{
  rotation_angle = (2.0*PI*epoch)/rotation_period + PI;
  cloud_disp = cloud_disp_rate*epoch;
}

Ring::Ring()
{
  has_rings = false;
}

void Ring::load()
{
  if (has_rings)
  {
    const int ringsize = 4096;
    unsigned char *rings = new unsigned char[ringsize];
    generateRings(rings, ringsize, seed);

    tex.create();
    TexMipmapData(false, tex, 0, GL_DEPTH_COMPONENT, ringsize, 1, GL_UNSIGNED_BYTE, rings).updateTexture();
    tex.genMipmaps();
  }
}
void Ring::unload()
{
  if (has_rings)
    tex.destroy();
}

Atmosphere::Atmosphere()
{
  max_height = -100;
}
int Planet::SCATTERING_RES = 256;

Planet::Planet()
{
  name = "undefined";
  loaded = false;
}

const std::string &Planet::getName() const
{
  return name;
}

float Planet::scat_density(const glm::vec2 &p)
{
  return scat_density(glm::length(p)-body.radius);
}

float Planet::scat_density(float p)
{
  return glm::exp(-std::max(0.0f,p)/atmos.scale_height);
}

float Planet::scat_optic(const glm::vec2 &a, const glm::vec2 &b)
{
  glm::vec2 step = (b-a)/(float)SCATTERING_SAMPLES;
  glm::vec2 v = a+step*0.5;

  float sum = 0.0;
  for (int i=0;i<SCATTERING_SAMPLES;++i)
  {
    sum += scat_density(v);
    v += step;
  }
  return sum * glm::length(step) / atmos.max_height;
}

float Planet::ray_sphere_far(glm::vec2 ori, glm::vec2 ray, float radius)
{
  float b = glm::dot(ori, ray);
  float c = glm::dot(ori,ori) - radius*radius;
  return -b+sqrt(b*b-c);
}

void Planet::load()
{
  if (!loaded)
  {
    body.load();
    ring.load();
    // Atmospheric scattering lookup table creation
    if (atmos.max_height >= 0)
    {
      // x-axis : altitude from 0.0 (sea level) to 1.0 (max_height)
      // y-axis : cosine of angle of ray / 2
      float *lookup_data = new float[SCATTERING_RES*SCATTERING_RES*4];
      float inv_scaleheight = 1.0/atmos.scale_height;
      for (int i=0;i<SCATTERING_RES;++i)
      {
        float alt = (float)i / (float)SCATTERING_RES * atmos.max_height;
        float density = glm::exp(-alt*inv_scaleheight);
        for (int j=0;j<SCATTERING_RES;++j)
        {
          lookup_data[(i+j*SCATTERING_RES)*4+0] = density;
          float angle = (float)j*PI/(float)SCATTERING_RES;
          glm::vec2 ray_dir = glm::vec2(sin(angle),cos(angle));
          glm::vec2 ray_ori = glm::vec2(0, body.radius + alt);
          float t = ray_sphere_far(ray_ori, ray_dir, body.radius+atmos.max_height);
          glm::vec2 u = ray_ori + ray_dir*t;
          lookup_data[(i+j*SCATTERING_RES)*4+1] = scat_optic(ray_ori,u)*(4*3.14159265359);
        }
      }
      glGenTextures(1,&atmos.lookup_table);
      glBindTexture(GL_TEXTURE_2D, atmos.lookup_table);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCATTERING_RES, SCATTERING_RES, 0, GL_RGBA,GL_FLOAT,lookup_data);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glBindTexture(GL_TEXTURE_2D, 0);
      delete [] lookup_data;
    }
    loaded = true;
  }
}

void Planet::unload()
{
  if (loaded)
  {
    body.unload();
    ring.unload();
    if (atmos.max_height >= 0)
      glDeleteTextures(1, &atmos.lookup_table);
    loaded = false;
  }
}

void Planet::update(double epoch)
{
  orbit.computePosition(epoch);
  body.update(epoch);
}

void Planet::setParentBody(std::deque<Planet> &planets)
{
  orbit.setParentFromName(planets);
}

Orbit &Planet::getOrbit()
{
  return orbit;
}
Body &Planet::getBody()
{
  return body;
}
Ring &Planet::getRing()
{
  return ring;
}
Atmosphere &Planet ::getAtmosphere()
{
  return atmos;
}

const glm::dvec3 &Planet::getPosition() const
{
  return orbit.getPosition();
}

template <>
glm::vec3 Planet::get(shaun::sweeper &swp, glm::vec3 def)
{
  glm::vec3 ret;
  if (swp.is_null())
    return def;
  else
  {
    for (int i=0;i<3;++i)
      ret[i] = swp[i].value<shaun::number>();
    return ret;
  }
}
template <>
glm::vec4 Planet::get(shaun::sweeper &swp, glm::vec4 def)
{
  glm::vec4 ret;
  if (swp.is_null())
    return def;
  else
  {
    for (int i=0;i<4;++i)
      ret[i] = swp[i].value<shaun::number>();
    return ret;
  }
}

template <>
float Planet::get(shaun::sweeper &swp, float def)
{
  if (swp.is_null()) return def; else return swp.value<shaun::number>();
}

template <>
double Planet::get(shaun::sweeper &swp, double def)
{
  if (swp.is_null()) return def; else return swp.value<shaun::number>();
}

template <>
int Planet::get(shaun::sweeper &swp, int def)
{
  if (swp.is_null()) return def; else return swp.value<shaun::number>();
}

template <>
std::string Planet::get(shaun::sweeper &swp, std::string def)
{
  if (swp.is_null()) return def; else return std::string(swp.value<shaun::string>());
}

template <>
bool Planet::get(shaun::sweeper &swp, bool def)
{
  if (swp.is_null()) return def; else return swp.value<shaun::boolean>();
}

void Planet::createFromFile(shaun::sweeper &swp1)
{
  shaun::sweeper swp(swp1);
  this->name = get<std::string>(swp("name"), "undefined");
  auto orbit(swp("orbit"));
  if (!orbit.is_null())
  {
    this->orbit.setParameters(
      get<std::string>(orbit("parent"), ""),
      get<double>(orbit("ecc"), 0.0),
      get<double>(orbit("sma"), 1000.0),
      get<double>(orbit("inc"), 0.0),
      get<double>(orbit("lan"), 0.0),
      get<double>(orbit("arg"), 0.0),
      get<double>(orbit("m0"), 0.0)
    );
  }

  auto phys(swp("body"));
  if (!phys.is_null())
  {
    this->body.radius = get<float>(phys("radius"), 1.0);
    float r_a = glm::radians(get<float>(phys("right_ascension"), 0.0));
    float dec = glm::radians(get<float>(phys("declination"), 90.0));
    this->body.rotation_axis = glm::vec3(-sin(r_a)*cos(dec),cos(r_a)*cos(dec), sin(dec));
    this->body.rotation_period = get<float>(phys("rot_period"), 10.0);
    this->body.mean_color = get<glm::vec3>(phys("mean_color"), glm::vec3(1.0));
    this->body.albedo = get<float>(phys("albedo"), 0.3);
    this->body.GM = get<double>(phys("GM"), 1000);
    this->body.is_star = get<bool>(phys("is_star"), false);
    this->body.diffuse_filename = get<std::string>(phys("diffuse"), "");
    this->body.night_filename = get<std::string>(phys("night"), "");
    this->body.cloud_filename = get<std::string>(phys("cloud"), "");
    this->body.cloud_disp_rate = get<float>(phys("cloud_disp_rate"), 0.0);
    this->body.has_night_tex = this->body.night_filename!="";
    this->body.has_cloud_tex = this->body.cloud_filename!="";
  }

  auto atmos(swp("atmosphere"));
  if (!atmos.is_null())
  {
      this->atmos.max_height = get<float>(atmos("max_altitude"), -100.0);
      this->atmos.K_R = get<float>(atmos("K_R"),0);
      this->atmos.K_M = get<float>(atmos("K_M"),0);
      this->atmos.E = get<float>(atmos("E"),0);
      this->atmos.C_R = get<glm::vec3>(atmos("C_R"),glm::vec3(0,0,0));
      this->atmos.G_M = get<float>(atmos("G_M"),-0.75);
      this->atmos.scale_height = get<float>(atmos("scale_height"), this->atmos.max_height/4);
  }

  auto ring(swp("ring"));
  if (!ring.is_null())
  {
    this->ring.has_rings = true;
    this->ring.inner = get<float>(ring("inner"), 2.0);
    this->ring.outer = get<float>(ring("outer"), 4.0);
    float r_a = glm::radians(get<float>(ring("right_ascension"), 0.0));
    float dec = glm::radians(get<float>(ring("declination"), 90.0));
    this->ring.normal = glm::vec3(-sin(r_a)*cos(dec),cos(r_a)*cos(dec), sin(dec));
    this->ring.seed = get<int>(ring("seed"), 2.0);
    this->ring.color = get<glm::vec4>(ring("color"), glm::vec4(0.6,0.6,0.6,1.0));
  }
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
  skybox_shader.uniform("projMat", proj_mat);
  skybox_shader.uniform("viewMat", view_mat);
  skybox_shader.uniform("modelMat", skybox_mat);
  skybox_shader.uniform("tex", 0);
  tex.use(0);
  o.render(); 
}