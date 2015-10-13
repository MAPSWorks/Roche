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
  Planet();
  void load();
  void update(double epoch);
  void render(
    const glm::mat4 &proj_mat,
    const glm::mat4 &view_mat,
    const glm::vec3 &view_pos,
    const glm::vec3 &light_pos,
    const glm::vec3 &focused_planet_pos,
    Shader &planet_shader, Shader &sun_shader, Shader &ring_shader,
    Renderable &planet_obj, Renderable &ring_obj);
  void unload();

  static Texture no_night,no_clouds;

  class OrbitalParameters
  {
  public:
    OrbitalParameters();
    OrbitalParameters(double ecc, double sma, double inc, double lan, double arg, double m0);
    void setParameters
    double getEccentricity() const;
    double getSemiMajorAxis() const;
    double getInclination() const;
    double getLongitudeOfAscNode() const;
    double getArgumentOfPeriapsis() const;
    double getMeanAnomalyAtEpoch() const;
  private:
    double ecc, sma, inc, lan, arg, m0;
  };

  class PhysicalProperties
  {
  public:
    PhysicalProperties();
    const glm::vec3 &getRotationAxis();
    float getRotationRate();
    float getRadius();
    const glm::vec3 &getMeanColor();
    const glm::vec3 &getAtmosphereColor();
    double getGM();
    bool isStar();
  private:
    glm::vec3 rotation_axis;
    float rotation_rate; // radians per second
    float radius; // km
    glm::vec3 mean_color;
    glm::vec3 atmosphere_color;
    double GM; // km3 s-2
    bool is_star;
  };

  class RingProperties
  {
  public:
    RingProperties();
    RingProperties(float inner, float outer, const glm::vec3 &normal, int seed, const glm::vec4 &color);
    bool hasRings();
    float getInnerDistance();
    float getOuterDistance();
    const glm::vec3 &getNormal();
    int getSeed();
    const glm::vec4 &getColor();
  private:
    bool has_rings;
    float inner; // ring nearest distance to center of planet (km)
    float outer; // ring farthest distance to center of planet (km)
    glm::vec3 up; // ring plane's normal vector (normalized)
    int seed; // seed for generating the rings
    glm::vec4 color;
  };

  class CloudProperties
  {
  public:
    CloudProperties();

  private:
    bool has_clouds;
    float displacement_rate;
  };

private:
  std::string name;
  glm::vec3 pos;
  float rot_angle;

  float cloud_epoch;

  OrbitalParameters orbit;
  PhysicalProperties phys;
  RingProperties ring;

  Planet *parent;
  Texture day_tex,night_tex,clouds_tex,ring_tex;
  std::string day_filename, clouds_filename, night_filename;

  bool loaded;

  
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
