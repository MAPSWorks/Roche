#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"
#include "opengl.h"
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
  polarPos = glm::vec3(0.0,0.0,8000);
  center = glm::vec3(0,0,0);
  up = glm::vec3(0,0,1);

  fovy = 50;
  near = 20;
  far = 20000000.0;
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
  pos = glm::vec3(cos(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[1])*polarPos[2]) + center;
  viewMat = glm::lookAt(pos, center, up);
}

Game::Game()
{
  sensibility = 0.0004;
  light_position = glm::vec3(0,5,0);
  viewSpeed = glm::vec3(0,0,0);
  maxViewSpeed = 0.2;
  viewSmoothness = 0.85;
  epoch = 0.0;
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
    it->stopthread = true;
    it->mutex.unlock();
  }
  for (auto it=plThreads.begin();it != plThreads.end();++it)
  {
    it->join();
  }
}

void plThread(PlanetLoader *pl, std::deque<Texture*> *texs, std::mutex *m)
{
  glfwMakeContextCurrent(pl->context);
  pl->waiting = true;
  while (!pl->stopthread)
  {
    pl->mutex.lock();
    pl->waiting = false;
    m->lock();
    bool empty = texs->empty();
    while (!empty)
    {
      Texture *t = texs->front();
      texs->pop_front();
      m->unlock();
      t->load();
      m->lock();
      empty = texs->empty();
    } 
    m->unlock();
    pl->waiting = true;
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

  const int plCount = 4;
  for (int i=0;i<plCount;++i)
  {
    planetLoaders.emplace_back();
    planetLoaders.back().context = glfwCreateWindow(1,1,"",NULL,win);
    if (!planetLoaders.back().context)
    {
      std::cout << "Can't create context for texture loading" << std::endl;
      exit(-1);
    }
    planetLoaders.back().stopthread = false;
    planetLoaders.back().mutex.lock();
    plThreads.emplace_back(plThread, &planetLoaders.back(), &texturesToLoad, &texsMutex);
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
  skybox.tex.setFilename("space_skybox.dds");
  skybox.rot_axis = glm::vec3(1,0,0);
  skybox.rot_angle = 60.0;
  skybox.size = 15000000;
  loadTexture(&skybox.tex);
  skybox.load();
}

void Game::loadPlanetFiles()
{
  planets.emplace_back();
  Planet *sun = &planets.back();
  sun->name = "Sun";
  sun->pos = glm::vec3(0,0,0);
  sun->GM = 132712440018;

  planets.emplace_back();
  Planet *earth = &planets.back();

  earth->name = "Earth";
  earth->pos = glm::vec3(0,0,0);
  earth->rot_axis = glm::vec3(0,0,1);
  earth->rot_rate =7.292115E-5;
  earth->radius = 6371.01;
  earth->ring_inner = 10000;
  earth->ring_outer = 16000;
  earth->ring_upvector = glm::normalize(glm::vec3(1,1,2));
  earth->ring_seed = 1909802985;
  earth->ring_color = glm::vec4(0.6,0.6,0.6,1.0);
  earth->has_rings = 1;
  earth->atmos_color = glm::vec3(0.6,0.8,1.0);
  earth->cloud_epoch = 0.0;
  earth->day.setFilename("earth_land.dds");
  earth->night.setFilename("earth_night.dds");
  earth->clouds.setFilename("earth_clouds.dds");
  earth->parent = sun;
  earth->ecc = 1.616998394251595E-02;
  earth->sma = 1.495125338678499E+08;
  earth->m0 = 3.571060381240151E+02;
  earth->inc = 2.343719926157708E+01;
  earth->lan = 4.704173983490563E-03;
  earth->arg = 1.042446186418036E+02;
  loadPlanet(&planets.back());

  focusedPlanet = earth;
}

void Game::loadPlanet(Planet *p)
{
  p->load();
  loadTexture(&p->day);
  loadTexture(&p->night);
  loadTexture(&p->clouds);
}
void Game::unloadPlanet(Planet *p)
{
  p->unload();
}
void Game::loadTexture(Texture *tex)
{
  texsMutex.lock();
  texturesToLoad.push_back(tex);
  texsMutex.unlock();
  for (auto it=planetLoaders.begin();it!=planetLoaders.end();++it)
  {
    if (it->waiting)
    {
      it->mutex.unlock();
      return;
    }
  }
}

void Game::update()
{
  focusedPlanet->update(epoch);
  std::cout << "x=" << focusedPlanet->pos.x << ";y=" << focusedPlanet->pos.y << ";z=" << focusedPlanet->pos.z << std::endl;
  epoch += 86400;

  double posX, posY;
  glm::vec2 move;
  glfwGetCursorPos(win, &posX, &posY);
  move.x = -posX+preMousePosX;
  move.y = posY-preMousePosY;

  camera.setCenter(focusedPlanet->pos);

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
  if (camera.getPolarPosition().z < focusedPlanet->radius) camera.getPolarPosition().z = focusedPlanet->radius;

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
  /*for (auto it=planets.begin();it != planets.end(); ++it)
  {
    it->render(proj_mat, view_mat, camera.getPosition(), light_position, planet_shader, ring_shader, planet_obj, ring_obj);
  }*/
  focusedPlanet->render(proj_mat, view_mat, camera.getPosition(), light_position, planet_shader, ring_shader, planet_obj, ring_obj);
  glfwSwapBuffers(win);
  glfwPollEvents();
}

bool Game::isRunning()
{
  return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}