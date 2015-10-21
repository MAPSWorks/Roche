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
  glm::vec3 view_center;
  Shader &planet_shader, &atmos_shader, &sun_shader, &ring_shader;
  Renderable &planet_obj, &atmos_obj, &ring_obj;
};

class Orbit
{
public:
  Orbit();
  void setParameters(const std::string &parent_body, double ecc, double sma, double inc, double lan, double arg, double m0);
  void computePosition(double epoch); /// Computes the position from current epoch and parent body
  const glm::vec3 &getPosition() const;
  void setParentFromName(std::deque<Planet> &planets);
  bool isUpdated();
  void reset();
  void print() const;
private:
  double ecc, sma, inc, lan, arg, m0;
  bool updated;
  std::string parent_body;

  glm::vec3 position;
  Planet *parent;
};

class Ring
{
public:
  Ring();
  void setProperties(
    float inner,
    float outer,
    const glm::vec3 &normal,
    int seed,
    const glm::vec4 &color);
  void load();
  void unload();
  void render(const glm::mat4 &model_mat, const glm::mat4 &light_mat, const RenderContext &rc) const;
  void useTexture(int unit) const;
  float getInner() const;
  float getOuter() const;
  const glm::vec3 &getNormal() const;
  void print() const;
  
private:
  void generateRings(unsigned char *buffer, int size, int seed);

  bool has_rings;
  float inner; // ring nearest distance to center of planet (km)
  float outer; // ring farthest distance to center of planet (km)
  glm::vec3 up; // ring plane's normal vector (normalized)
  int seed; // seed for generating the rings
  glm::vec4 color;

  Texture tex;

  bool loaded;

  static Texture no_rings;
  static bool no_rings_init;
};

class Atmosphere
{
public:
  Atmosphere();
  void setProperties(const glm::vec3 &atmosphere_color, float max_altitude);
  const glm::vec3 &getColor() const;
  float getMaxAltitude() const;
  void print() const;
private:
  glm::vec3 color;
  float max_altitude;
};

class Body
{
public:
  Body();
  void setProperties(
    float radius,
    const glm::vec3 rot_axis,
    float rotation_rate,
    glm::vec3 mean_color,
    double GM,
    bool is_star,
    const std::string &diffuse_filename,
    const std::string &night_filename,
    const std::string &cloud_filename,
    float cloud_disp_rate);
  void load();
  void unload();
  void update(double epoch);
  void render(const glm::vec3 &pos, const RenderContext &rc, const Ring &rings, const Atmosphere &atmos);

  float getRadius() const;
  double getGM() const;
  void print() const;

private:
  glm::mat4 computeLightMatrix(const glm::vec3 &light_dir,const glm::vec3 &light_up, float planet_size, float ring_outer);
  void computeRingMatrix(glm::vec3 toward_view, glm::vec3 rings_up, float size, glm::mat4 &near_mat, glm::mat4 &far_mat);

  float radius; // km 
  glm::vec3 rotation_axis;
  float rotation_rate; // radians per second
  glm::vec3 mean_color;
  double GM; // km3 s-2
  bool is_star;
  std::string diffuse_filename;
  bool has_night_tex;
  std::string night_filename;
  bool has_cloud_tex;
  float cloud_disp_rate;
  std::string cloud_filename;


  Texture cloud_tex;
  Texture diffuse_tex;    
  Texture night_tex;
  float rotation_angle;
  float cloud_disp;

  bool loaded;

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
  const glm::vec3 &getPosition() const;

  Orbit &getOrbit();
  Body &getBody();
  Ring &getRing();
  Atmosphere &getAtmosphere();

private:
  template <class T>
  T shaun_fieldToType(shaun::sweeper &swp, T def);

  std::string name;

  Orbit orbit;
  Body phys;
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
