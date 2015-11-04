#ifndef PLANET_H
#define PLANET_H

#include "opengl.h"
#include <string>
#include <deque>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "shaun/sweeper.hpp"

class Planet;

class RenderContext
{
public:
  RenderContext(Shader &planet_shader, Shader &atmos_shader, Shader &sun_shader, Shader &ring_shader,
    Renderable &planet_obj, Renderable &atmos_obj, Renderable &ring_obj);
  glm::mat4 proj_mat;
  glm::mat4 view_mat;
  glm::vec3 view_pos;
  glm::vec3 light_pos;
  glm::dvec3 view_center;
  Shader &planet_shader, &atmos_shader, &sun_shader, &ring_shader;
  Renderable &planet_obj, &atmos_obj, &ring_obj;
};

class Orbit
{
public:
  Orbit();
  void setParameters(const std::string &parent_body, double ecc, double sma, double inc, double lan, double arg, double m0);
  void computePosition(double epoch); /// Computes the position from current epoch and parent body
  const glm::dvec3 &getPosition() const;
  void setParentFromName(std::deque<Planet> &planets);
  bool isUpdated();
  void reset();
  void print() const;
private:
  double ecc, sma, inc, lan, arg, m0;
  bool updated;
  std::string parent_body;

  glm::dvec3 position;
  Planet *parent;
};

class Ring
{
public:
  Ring();
  void load();
  void unload();
  void render(const glm::mat4 &model_mat, const glm::mat4 &light_mat, const RenderContext &rc) const;
  void useTexture(int unit) const;
  void print() const;

  bool has_rings;
  float inner; // ring nearest distance to center of planet (km)
  float outer; // ring farthest distance to center of planet (km)
  glm::vec3 normal; // ring plane's normal vector (normalized)
  int seed; // seed for generating the rings
  glm::vec4 color;
  
private:
  void generateRings(unsigned char *buffer, int size, int seed);

  Texture tex;

  static Texture no_rings;
  static bool no_rings_init;
};

class Atmosphere
{
public:
  Atmosphere();
  void print() const;

  float K_R,K_M,E,G_M;
  glm::vec3 C_R;
  float max_height;
  float scale_height;
  GLuint lookup_table;
};

class Body
{
public:
  Body();
  void load();
  void unload();
  void update(double epoch);
  void render(const glm::dvec3 &pos, const RenderContext &rc, const Ring &rings, const Atmosphere &atmos);

  void print() const;

  float radius; // km 
  glm::vec3 rotation_axis;
  float rotation_period; // radians per second
  glm::vec3 mean_color;
  float albedo;
  double GM; // km3 s-2
  bool is_star;
  std::string diffuse_filename;
  bool has_night_tex;
  std::string night_filename;
  bool has_cloud_tex;
  float cloud_disp_rate;
  std::string cloud_filename;

private:
  glm::mat4 computeLightMatrix(const glm::vec3 &light_dir,const glm::vec3 &light_up, float planet_size, float ring_outer);
  void computeRingMatrix(glm::vec3 toward_view, glm::vec3 rings_up, float size, glm::mat4 &near_mat, glm::mat4 &far_mat);

  Texture cloud_tex;
  Texture diffuse_tex;    
  Texture night_tex;
  float rotation_angle;
  float cloud_disp;

  static Texture no_night,no_clouds;
  static bool no_tex_init;
};

class Planet
{
public:
  Planet();
  void createFromFile(shaun::sweeper &swp);
  void load();
  void update(double epoch);
  void setParentBody(std::deque<Planet> &planets);
  void render(const RenderContext &rc);
  void unload();
  void print() const;

  const std::string &getName() const;
  const glm::dvec3 &getPosition() const;

  Orbit &getOrbit();
  Body &getBody();
  Ring &getRing();
  Atmosphere &getAtmosphere();

  static int SCATTERING_RES;

private:
  template <class T>
  T get(shaun::sweeper &swp, T def);

  float scat_density(const glm::vec2 &p);
  float scat_density(float p);
  float scat_optic(const glm::vec2 &a,const glm::vec2 &b);
  float ray_sphere_far(glm::vec2 ori, glm::vec2 ray, float radius);

  std::string name;

  bool loaded;

  Orbit orbit;
  Body body;
  Ring ring;
  Atmosphere atmos;
};

class Skybox
{
public:
  Texture tex;
  glm::vec3 rot_axis;
  float rot_angle;
  float size;

  void load();
  void render(const glm::mat4 &proj_mat, const glm::mat4 &view_mat, Shader &skybox_shader, Renderable &o);
};

#endif
