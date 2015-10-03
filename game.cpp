#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>

Camera::Camera()
{
  polarPos = glm::vec3(0.7,0.2,1.2);
  center = glm::vec3(0,0,0);
  up = glm::vec3(0,0,1);

  fovy = 50;
  near = 0.02;
  far = 1000.0;
}

glm::vec3 &Camera::getPolarPosition()
{
  return polarPos;
}

void Camera::setCenter(glm::vec3 center)
{
  this->center = center;
}

void Camera::setPolarPosition(glm::vec3 pos)
{
  this->polarPos = pos;
}

const glm::vec3 &Camera::getPosition()
{
  return pos;
}

void Camera::setUp(glm::vec3 up)
{
  this->up = up;
}

const glm::mat4 &Camera::getProjMat()
{
  return projMat;
}

const glm::mat4 &Camera::getViewMat()
{
  return viewMat;
}

void Camera::update(float ratio)
{
  projMat = glm::perspective((float)(fovy/180*PI),ratio, near, far);
  pos = glm::vec3(cos(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[1])*polarPos[2]);
  viewMat = glm::lookAt(pos+center, center, up);
}

Game::Game()
{
  sensibility = 0.0004;
  light_position = glm::vec3(0,5,0);
  viewSpeed = glm::vec3(0,0,0);
  maxViewSpeed = 0.2;
  viewSmoothness = 0.85;
}

Game::~Game()
{
  ring_shader.destroy();
  planet_shader.destroy();

  ring_obj.destroy();
  planet_obj.destroy();
  skybox_obj.destroy();
  glfwTerminate();

  for (auto it=planetLoaders.begin();it != planetLoaders.end();++it)
  {
    it->mutex.unlock();
    it->stopthread = true;
  }
  for (auto it=plThreads.begin();it != plThreads.end();++it)
  {
    it->join();
  }
}

void plThread(PlanetLoader *pl)
{
  glfwMakeContextCurrent(pl->context);
  while (!pl->stopthread)
  {
    pl->mutex.lock();
    if (pl->planet)
    {
      pl->planet->load();
      pl->planet = NULL;
    }
  }
  glfwDestroyWindow(pl->context);
}

void Game::init()
{
  if (!glfwInit())
    exit(-1);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  glfwWindowHint(GLFW_RED_BITS, mode->redBits);
  glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
  glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
  glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
  glfwWindowHint(GLFW_SAMPLES, 16);

  win = glfwCreateWindow(mode->width, mode->height, "Roche", monitor, NULL);

  if (!win)
  {
    glfwTerminate();
    exit(-1);
  }

  glfwMakeContextCurrent(win);
  glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

  const int plCount = 1;
  for (int i=0;i<plCount;++i)
  {
    planetLoaders.emplace_back();
    planetLoaders.back().context = glfwCreateWindow(1,1,"",NULL,win);
    if (!planetLoaders.back().context)
    {
      std::cout << "Can't create context for texture loading" << std::endl;
      exit(-1);
    }
    planetLoaders.back().planet = NULL;
    planetLoaders.back().stopthread = false;
    planetLoaders.back().mutex.lock();
    plThreads.emplace_back(plThread, &planetLoaders.back());
  }

  GLenum err = glewInit();
  if (GLEW_OK != err)
  {
  std::cout << "Some shit happened: " << glewGetErrorString(err) << std::endl;
  }

  int width, height;
  glfwGetFramebufferSize(win, &width, &height);
  glViewport(0, 0, width, height);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  generateModels();
  loadShaders();
  loadSkybox();
  loadPlanetFiles();
  glfwGetCursorPos(win, &preMousePosX, &preMousePosY);
  ratio = width/(float)height;
}

void Game::generateModels()
{
  float ring_pos[] = 
  { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
   +1.0,-1.0,0.0,1.0,+1.0,-1.0,
   +1.0,+1.0,0.0,1.0,+1.0,+1.0,
   -0.0,+1.0,0.0,1.0,-0.0,+1.0,
  };

  int ring_ind[] = {0,1,2,2,3,0,0,3,2,2,1,0};

  ring_obj.create();
  ring_obj.update_verts(24*4, ring_pos);
  ring_obj.update_ind(12, ring_ind);

  planet_obj.generate_sphere(128,128, 1);
  skybox_obj.generate_sphere(16,16,0);
}

void Game::loadShaders()
{
  ring_shader.create();
  planet_shader.create();
  skybox_shader.create();
  ring_shader.load_from_file("ring.vert", "ring.frag");
  planet_shader.load_from_file("planet.vert", "planet.frag");
  skybox_shader.load_from_file("skybox.vert", "skybox.frag");
}

void Game::loadSkybox()
{
  skybox.tex_filename = "space_skybox.dds";
  skybox.rot_axis = glm::vec3(1,0,0);
  skybox.rot_angle = 60.0;
  skybox.size = 200;
  skybox.load();
}

void Game::loadPlanetFiles()
{
  planets.emplace_back();
  Planet *earth = &planets.back();

  earth->name = "Earth";
  earth->pos = glm::vec3(0,0,0);
  earth->rot_axis = glm::vec3(0,0,1);
  earth->rot_epoch = 0.0;
  earth->radius = 0.3;
  earth->ring_inner = 0.4;
  earth->ring_outer = 0.6;
  earth->ring_upvector = glm::normalize(glm::vec3(1,1,2));
  earth->ring_seed = 1909802985;
  earth->ring_color = glm::vec4(0.6,0.6,0.6,1.0);
  earth->has_rings = 1;
  earth->atmos_color = glm::vec3(0.6,0.8,1.0);
  earth->cloud_epoch = 0.0;
  earth->day_filename = "earth_land.dds";
  earth->night_filename = "earth_night.dds";
  earth->clouds_filename = "earth_clouds.dds";
  
  for (auto it=planetLoaders.begin();it!=planetLoaders.end();++it)
  {
    if (!it->planet)
    {
      it->planet = &planets.back();
      it->mutex.unlock();
      return;
    }
  }
  planets.back().load();
}

void Game::update()
{
  double posX, posY;
  glm::vec2 move;
  glfwGetCursorPos(win, &posX, &posY);
  move.x = -posX+preMousePosX;
  move.y = posY-preMousePosY;

  if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2))
  {
    viewSpeed.x += move.x*sensibility;
    viewSpeed.y += move.y*sensibility;
    for (int i=0;i<2;++i)
    {
      if (viewSpeed[i] > maxViewSpeed) viewSpeed[i] = maxViewSpeed;
      if (viewSpeed[i] < -maxViewSpeed) viewSpeed[i] = -maxViewSpeed;
    }
  }
  else if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_3))
  {
    viewSpeed.z += (move.y*sensibility);
  }

  camera.getPolarPosition().x += viewSpeed.x;
  camera.getPolarPosition().y += viewSpeed.y;
  camera.getPolarPosition().z *= 1.0+viewSpeed.z;

  viewSpeed *= viewSmoothness;

  if (camera.getPolarPosition().y > PI/2 - 0.0001) camera.getPolarPosition().y = PI/2 - 0.0001;
  if (camera.getPolarPosition().y < -PI/2 + 0.0001) camera.getPolarPosition().y = -PI/2 + 0.0001;
  if (camera.getPolarPosition().z < 0.4) camera.getPolarPosition().z = 0.4;

  preMousePosX = posX;
  preMousePosY = posY;
}

void Game::render()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  camera.update(ratio);
  glm::mat4 proj_mat = camera.getProjMat();
  glm::mat4 view_mat = camera.getViewMat();
  skybox.render(proj_mat, view_mat, skybox_shader, skybox_obj);
  for (auto it=planets.begin();it != planets.end(); ++it)
  {
    it->render(proj_mat, view_mat, camera.getPosition(), light_position, planet_shader, ring_shader, planet_obj, ring_obj);
  }
  glfwSwapBuffers(win);
  glfwPollEvents();
}

bool Game::isRunning()
{
  return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}