#ifndef PLANET_H
#define PLANET_H

#include "opengl.h"
#include <string>

#include "glm/glm.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <atomic>

class Planet
{
public:
  std::string name;
  glm::vec3 pos;
  glm::vec3 rot_axis;
  float rot_epoch;
  float radius;

  float ring_inner;
  float ring_outer;
  glm::vec3 ring_upvector;
  int ring_seed;
  glm::vec4 ring_color;
  int has_rings;

  glm::vec3 atmos_color;
  float cloud_epoch;

  Texture day,night,clouds,ring;

  Planet();

  void load();
  void render(glm::mat4 proj_mat, glm::mat4 view_mat, glm::vec3 view_pos, glm::vec3 light_pos, Shader &planet_shader, Shader &ring_shader, Renderable &planet_obj, Renderable &ring_obj);
  void unload();

private:
  std::atomic<bool> loaded;
};

class Skybox
{
public:
  Texture tex;
  glm::vec3 rot_axis;
  float rot_angle;
  float size;

  void load();
  void render(glm::mat4 proj_mat, glm::mat4 view_mat, Shader &skybox_shader, Renderable &o);
};

#endif
