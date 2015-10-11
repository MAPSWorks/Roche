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

class Camera
{
public:
  Camera();
  void update(float ratio);
  void setCenter(glm::vec3 center);
  void setPolarPosition(glm::vec3 pos);
  glm::vec3 &getPolarPosition();
  const glm::vec3 &getPosition();
  void setUp(glm::vec3 up);
  const glm::mat4 &getProjMat();
  const glm::mat4 &getViewMat();

private:
  glm::vec3 polarPos;
  glm::vec3 pos;
  glm::vec3 center;
  glm::vec3 up;
  float fovy;
  float near;
  float far;
  glm::mat4 projMat;
  glm::mat4 viewMat;

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

private:
  void generateModels();
  void loadShaders();
  void loadSkybox();
  void loadPlanetFiles();
  void loadTexture(const std::string &filename, Texture *tex);
  void loadPlanet(Planet *p);
  void unloadPlanet(Planet *p);

  std::deque<Planet> planets;
  Planet *focusedPlanet;
  double epoch;

  // THREADING RELATED STUFF
  std::deque<std::thread> plThreads;
  std::atomic<bool> quit;

  concurrent_queue<std::pair<std::string,Texture*>> texturesToLoad;

  concurrent_queue<TexMipmapData> texturesToUpdate;

  int thread_count;

  // RENDERING RELATED STUFF
  glm::vec3 light_position;
  Renderable skybox_obj, planet_obj, ring_obj, flare_obj;
  Texture flare_tex;
  Shader skybox_shader, planet_shader, ring_shader, sun_shader, flare_shader;
  Skybox skybox;

  // INTERACTION RELATED STUFF
  double preMousePosX, preMousePosY;
  glm::vec3 viewSpeed;
  float maxViewSpeed, viewSmoothness;

  float sensibility;

  Camera camera;
  GLFWwindow *win;
  float ratio;

};

#endif