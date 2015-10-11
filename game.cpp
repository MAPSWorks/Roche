#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"
#include "opengl.h"
#include "util.h"
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>

#define MAX_VIEW_DIST 2000000

Camera::Camera()
{
  polarPos = glm::vec3(0.0,0.0,8000);
  center = glm::vec3(0,0,0);
  up = glm::vec3(0,0,1);

  fovy = 50;
  near = 20;
  far = MAX_VIEW_DIST;
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
  flare_obj.destroy();
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

void plThread(TexLoader *pl, std::deque<std::pair<std::string,Texture*>> *texs, std::mutex *ttlm, std::deque<TexMipmapData> *tmd, std::mutex *ttum)
{
  pl->waiting = true;
  while (!pl->stopthread)
  {
    pl->mutex.lock();
    pl->waiting = false;
    ttlm->lock();
    bool e = texs->empty();
    while (!e)
    {
      auto p = texs->front();
      texs->pop_front();
      ttlm->unlock();
      load_DDS(p.first, p.second, tmd, ttum);
      ttlm->lock();
      e = texs->empty();
    } 
    ttlm->unlock();
    pl->waiting = true;
  }
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

  thread_count = 4;
  for (int i=0;i<thread_count;++i)
  {
    planetLoaders.emplace_back();
    planetLoaders.back().stopthread = false;
    planetLoaders.back().mutex.lock();
    plThreads.emplace_back(plThread, &planetLoaders.back(), &texturesToLoad, &ttlm,&texturesToUpdate, &ttum);
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
  camera.getPolarPosition.z = focusedPlanet->radius*2;
}

void Game::generateModels()
{
  float ring_vert[] = 
  { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
   +1.0,-1.0,0.0,1.0,+1.0,-1.0,
   +1.0,+1.0,0.0,1.0,+1.0,+1.0,
   -0.0,+1.0,0.0,1.0,-0.0,+1.0,
  };

  int ring_ind[] = {0,1,2,2,3,0,0,3,2,2,1,0};

  ring_obj.create();
  ring_obj.update_verts(24*4, ring_vert);
  ring_obj.update_ind(12, ring_ind);

  planet_obj.generate_sphere(128,128, 1);
  skybox_obj.generate_sphere(16,16,0);

  float flare_vert[] =
  { -1.0, -1.0, 0.0,1.0,0.0,0.0,
    +1.0, -1.0, 0.0,1.0,1.0,0.0,
    +1.0, +1.0, 0.0,1.0,1.0,1.0,
    -1.0, +1.0, 0.0,1.0,0.0,1.0
  };

  flare_obj.create();
  flare_obj.update_verts(24*4, flare_vert);
  flare_obj.update_ind(12, ring_ind);

  loadTexture("tex/flare.dds", &flare_tex);
}

void Game::loadShaders()
{
  ring_shader.load_from_file("ring.vert", "ring.frag");
  planet_shader.load_from_file("planet.vert", "planet.frag");
  sun_shader.load_from_file("planet.vert", "sun.frag");
  skybox_shader.load_from_file("skybox.vert", "skybox.frag");
  flare_shader.load_from_file("flare.vert", "flare.frag");
}

void Game::loadSkybox()
{
  skybox.rot_axis = glm::vec3(1,0,0);
  skybox.rot_angle = 60.0;
  skybox.size = 100;
  loadTexture("tex/skybox.dds", &skybox.tex);
  skybox.load();
}

void Game::loadPlanetFiles()
{
  planets.emplace_back();
  Planet *sun = &planets.back();
  sun->name = "Sun";
  sun->pos = glm::vec3(0,0,0);
  sun->GM = 132712440018;
  sun->radius = 696000;
  sun->day.create();
  sun->day_filename = "tex/sun/diffuse.dds";
  sun->is_sun = true;
  loadPlanet(sun);

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
  earth->has_rings = true;
  earth->atmos_color = glm::vec3(0.6,0.8,1.0);
  earth->cloud_epoch = 0.0;
  earth->day.create();
  earth->night.create();
  earth->clouds.create();
  earth->day_filename = "tex/earth/diffuse.dds";
  earth->night_filename = "tex/earth/night.dds";
  earth->clouds_filename = "tex/earth/clouds.dds";
  earth->has_night_tex = true;
  earth->has_clouds_tex = true;
  earth->parent = sun;
  earth->ecc = 1.616998394251595E-02;
  earth->sma = 1.495125338678499E+08;
  earth->m0 = 3.571060381240151E+02;
  earth->inc = 2.343719926157708E+01;
  earth->lan = 4.704173983490563E-03;
  earth->arg = 1.042446186418036E+02;
  loadPlanet(earth);

  focusedPlanet = earth;

  Planet::no_night.create();
  Planet::no_clouds.create();
  unsigned char black[4] = {0,0,0,255};
  unsigned char trans[4] = {255,255,255,0};
  TexMipmapData(false, &Planet::no_night, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, black).updateTexture();
  TexMipmapData(false, &Planet::no_clouds, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, trans).updateTexture();
}

void Game::loadPlanet(Planet *p)
{
  p->load();
  loadTexture(p->day_filename, &p->day);
  if (p->has_night_tex) loadTexture(p->night_filename, &p->night);
  if (p->has_clouds_tex) loadTexture(p->clouds_filename, &p->clouds);
}
void Game::unloadPlanet(Planet *p)
{
  p->unload();
}
void Game::loadTexture(const std::string &filename, Texture *tex)
{
  if (thread_count > 0)
  {
    ttlm.lock();
    texturesToLoad.push_back(std::pair<std::string,Texture*>(filename, tex));
    ttlm.unlock();
    for (auto it=planetLoaders.begin();it!=planetLoaders.end();++it)
    {
      if (it->waiting)
      {
        it->mutex.unlock();
        return;
      }
    }
  }
  else
  {
    load_DDS(filename, tex, &texturesToUpdate, &ttum);
  }
}

void Game::update()
{
  for (auto it=planets.begin();it != planets.end(); ++it)
  {
    it->update(epoch);
  }
  //std::cout << "x=" << focusedPlanet->pos.x << ";y=" << focusedPlanet->pos.y << ";z=" << focusedPlanet->pos.z << std::endl;
  epoch += 86400;

  double posX, posY;
  glm::vec2 move;
  glfwGetCursorPos(win, &posX, &posY);
  move.x = -posX+preMousePosX;
  move.y = posY-preMousePosY;

  camera.setCenter(glm::vec3(0,0,0));

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

  if (camera.getPolarPosition().y > PI/2 - 0.001)
  {
    camera.getPolarPosition().y = PI/2 - 0.001;
    viewSpeed.y = 0;
  }
  if (camera.getPolarPosition().y < -PI/2 + 0.001)
  {
    camera.getPolarPosition().y = -PI/2 + 0.001;
    viewSpeed.y = 0;
  }
  if (camera.getPolarPosition().z < focusedPlanet->radius) camera.getPolarPosition().z = focusedPlanet->radius;

  preMousePosX = posX;
  preMousePosY = posY;
}

void Game::render()
{
  ttum.lock();
  while (!texturesToUpdate.empty())
  {
    texturesToUpdate.front().updateTexture();
    texturesToUpdate.pop_front();
  }
  ttum.unlock();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  camera.update(ratio);
  glm::mat4 proj_mat = camera.getProjMat();
  glm::mat4 view_mat = camera.getViewMat();

  std::vector<Planet*> flares; // Planets to be rendered as flares (too far away)
  std::vector<Planet*> mesh; // Planets to be rendered as their whole mesh

  for (auto it=planets.begin();it != planets.end(); ++it)
  {
    glm::vec3 dist = it->pos - focusedPlanet->pos - camera.getPosition();
    if (glm::length(dist) >= MAX_VIEW_DIST)
      flares.push_back(&*it);
    else
      mesh.push_back(&*it);
  }

  glDepthMask(GL_FALSE);
  skybox.render(proj_mat, view_mat, skybox_shader, skybox_obj);
  flare_shader.use();
  flare_shader.uniform("ratio", ratio);
  flare_shader.uniform("tex", 0);
  flare_tex.use(0);
  for (auto it=flares.begin();it != flares.end(); ++it)
  {
    glm::vec4 posOnScreen = proj_mat*view_mat*glm::vec4((*it)->pos - focusedPlanet->pos, 1.0);
    if (posOnScreen.z > 0)
    {
      flare_shader.uniform("size", 0.2f);
      flare_shader.uniform("color", glm::value_ptr(glm::vec4(0.6,0.8,1.0,1.0)));

      glm::vec2 pOS = glm::vec2(posOnScreen/posOnScreen.w);
      flare_shader.uniform("pos", glm::value_ptr(pOS));
      flare_obj.render();
    }
  }
  glDepthMask(GL_TRUE);

  for (auto it=mesh.begin();it != mesh.end(); ++it)
  {
    (*it)->render(proj_mat, view_mat, camera.getPosition(), light_position, focusedPlanet->pos ,planet_shader, sun_shader, ring_shader, planet_obj, ring_obj);
  }
  glfwSwapBuffers(win);
  glfwPollEvents();
}

bool Game::isRunning()
{
  return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}