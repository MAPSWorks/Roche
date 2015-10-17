#ifndef GAME_H
#define GAME_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl.h"
#include "concurrent_queue.h"
#include "planet.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <deque>
#include <mutex>
#include <atomic>
#include <thread>

#define PI        3.14159265358979323846264338327950288 

/// Manages the view of the scene
class Camera
{
public:
  Camera();
  /// Needed before scene rendering,ratio is the screen w/h ratio
  void update(float ratio);
  /// Returns a modifiable reference to the point the view is looking at
  glm::vec3 &getCenter();
  /** Returns a modifiable reference to the the polar coordinates of the view
   * around the center point (theta, phi, distance), theta and phi in radians **/
  glm::vec3 &getPolarPosition();
  /// Returns the actual position of the camera in cartesian coordinates
  const glm::vec3 &getPosition();
  /// Returns a modifiable reference to the up vector of the view
  glm::vec3 &getUp();
  /// Returns the generated projection matrix
  const glm::mat4 &getProjMat();
  /// Returns the generated view matrix
  const glm::mat4 &getViewMat();

  /// Near frustum plane
  float getNear();
  void setNear(float near);

  /// Far frustum plane
  float getFar();
  void setFar(float far);

  /// Field of view on y axis
  float getFovy();
  void setFovy(float fovy);

private:
  glm::vec3 polarPos;
  glm::vec3 pos;
  glm::vec3 center;
  glm::vec3 up;
  float fovy;
  float near;
  float far;
  glm::mat4 proj_mat;
  glm::mat4 view_mat;

};

class Game
{
public:
  Game();
  ~Game();
  void init();
  void update();
  void render();
  bool isRunning();
  static void loadTexture(const std::string &filename, Texture &tex);

private:
  void generateModels();
  void loadShaders();
  void loadSkybox();
  void loadPlanetFiles();

  std::deque<Planet> planets; // Main planet collection
  Planet *focused_planet; // Planet the view follows
  int focused_planet_id;
  glm::vec3 view_center; // The position the view is centered on
  double epoch; // Seconds since January 1st 2015 00:00

  // THREADING RELATED STUFF
  std::deque<std::thread> tl_threads; // Texture loading Threads
  int thread_count; // Number of threads
  std::atomic<bool> quit; // boolean for killing threads

  /// Textures to load associated with their filename 
  static concurrent_queue<std::pair<std::string,Texture*>> textures_to_load;
  /// Loaded mipmap levels waiting to be used by opengl
  concurrent_queue<TexMipmapData> textures_to_update;

  // RENDERING RELATED STUFF
  RenderContext rc;
  glm::vec3 light_position;
  Renderable skybox_obj, planet_obj, ring_obj, flare_obj; // meshes
  Texture flare_tex;
  Shader skybox_shader, planet_shader, ring_shader, sun_shader, flare_shader;
  Skybox skybox;

  // INTERACTION RELATED STUFF
  double pre_mouseposx, pre_mouseposy; // previous cursor position
  glm::vec3 view_speed;
  float max_view_speed, view_smoothness;

  bool is_switching;
  int switch_frames;
  int switch_frame_current;
  float switch_previous_dist;
  bool planet_switch;
  Planet *switch_previous_planet;

  float sensibility;

  Camera camera;
  GLFWwindow *win;
  float ratio;

};

#endif