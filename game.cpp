#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"
#include "opengl.h"
#include "util.h"
#include "concurrent_queue.h"

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
#include <functional>
#include <algorithm>

#include "shaun/sweeper.hpp"
#include "shaun/parser.hpp"
#include "lodepng.h"

#include <glm/ext.hpp>

#define MIN_FLARE_DIST  4000000
#define MAX_PLANET_DIST 8000000

Camera::Camera()
{
  polarPos = glm::vec3(0.0,0.0,8000);
  center = glm::vec3(0,0,0);
  up = glm::vec3(0,0,1);

  fovy = 50;
  near = 20;
  far = MIN_FLARE_DIST;
}

glm::vec3 &Camera::getCenter()
{
  return center;
}

glm::vec3 &Camera::getPolarPosition()
{
  return polarPos;
}
const glm::vec3 &Camera::getPosition()
{
  return pos;
}
glm::vec3 &Camera::getUp()
{
  return up;
}

const glm::mat4 &Camera::getProjMat()
{
  return proj_mat;
}

const glm::mat4 &Camera::getViewMat()
{
  return view_mat;
}

void Camera::update(float ratio)
{
  proj_mat = glm::infinitePerspective((float)(fovy/180*PI),ratio, 0.01f);
  pos = glm::vec3(cos(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[0])*cos(polarPos[1])*polarPos[2], sin(polarPos[1])*polarPos[2]) + center;
  view_mat = glm::lookAt(pos, center, up);
}

float Camera::getNear()
{
  return near;
}
void Camera::setNear(float near)
{
  this->near = near;
}

float Camera::getFar()
{
  return far;
}
void Camera::setFar(float far)
{
  this->far = far;
}

float Camera::getFovy()
{
  return fovy;
}
void Camera::setFovy(float fovy)
{
  this->fovy = fovy;
}

Input::Input(GLFWwindow **win)
{
  this->win = win;
}
bool Input::isPressed(int key)
{
  bool key_pressed = glfwGetKey(*win, key);
  if (key_pressed && !pressed[key])
  {
    pressed[key] = true;
    return true;
  } 
  else if (!key_pressed)
  {
    pressed[key] = false;
    return false;
  }
  return false;
}

bool Input::isHeld(int key)
{
  return glfwGetKey(*win,key);
}

concurrent_queue<std::pair<std::string,Texture*>> Game::textures_to_load;

Game::Game() : rc(planet_shader, atmos_shader, sun_shader, ring_shader, planet_obj, atmos_obj, ring_obj), input(&win)
{
  sensitivity = 0.0004;
  light_position = glm::vec3(0,0,0);
  view_speed = glm::vec3(0,0,0);
  max_view_speed = 0.2;
  view_smoothness = 0.85;
  view_center = glm::vec3(0,0,0);
  switch_previous_planet = NULL;
  save = false;

  time_warp_values = {1,100,10000,86400,1000000};
  time_warp_index = 0;

  switch_frames = 100;
  switch_frame_current = 0;
  is_switching = false;
  focused_planet_id = 0;
  epoch = 0.0;
  quit = false;
}

Game::~Game()
{
  ring_shader.destroy();
  planet_shader.destroy();

  ring_obj.destroy();
  planet_obj.destroy();
  skybox_obj.destroy();
  flare_obj.destroy();

  post_processing.destroy();

  glfwTerminate();

  quit = true;
  for (auto it=tl_threads.begin();it != tl_threads.end();++it)
  {
    it->join();
  }
  screenshot_thread->join();
  delete [] screenshot_buffer;
  delete screenshot_thread;
}

void tlThread(std::atomic<bool> &quit, concurrent_queue<std::pair<std::string,Texture*>> &texs, concurrent_queue<TexMipmapData> &tmd, DDSLoader &loader)
{
  std::pair<std::string,Texture*> st;
  while (!quit)
  {
    if (texs.try_next(st))
    {
      loader.load(st.first, *st.second, tmd);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void ssThread(std::atomic<bool> &quit, unsigned char *buffer, std::atomic<bool> &save, GLFWwindow *win)
{
  while (!quit)
  {
    if (save)
    {
      time_t t = time(0);
      struct tm *now = localtime(&t);
      std::stringstream filename;
      filename << "./screenshots/screenshot_" << (now->tm_year+1900) << "-" << (now->tm_mon+1) << "-" << (now->tm_mday) << "_" << (now->tm_hour) << "-" << (now->tm_min) << "-" << (now->tm_sec) << ".png";
      int width,height;
      glfwGetWindowSize(win, &width, &height);

      unsigned int error = lodepng::encode(filename.str(), buffer, width,height);
      if(error) std::cout << "Can't save screenshot: " << lodepng_error_text(error) << std::endl;
      save = false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void Game::loadSettingsFile()
{
  try 
  {
    shaun::parser p;
    shaun::object obj = p.parse(read_file("settings.sn").c_str());
    shaun::sweeper swp(&obj);

    shaun::sweeper video(swp("video"));
    auto fs = video("fullscreen");
    fullscreen = (fs.is_null())?true:(bool)fs.value<shaun::boolean>();

    if (!fullscreen)
    {
      win_w = video("res_x").value<shaun::number>();
      win_h = video("res_y").value<shaun::number>();
    }

    shaun::sweeper graphics(swp("graphics"));
    auto max_size = graphics("texture_max_size");
    dds_loader.setMaxSize((max_size.is_null())?0:max_size.value<shaun::number>());

    auto ssaa_fact = graphics("ssaa_factor");
    ssaa_factor = (ssaa_fact.is_null())?1.0:ssaa_fact.value<shaun::number>();

    shaun::sweeper controls(swp("controls"));
    sensitivity = controls("sensitivity").value<shaun::number>();
  } 
  catch (shaun::parse_error e)
  {
    std::cout << e << std::endl;
  }
}

void Game::init()
{

  loadSettingsFile();

  if (!glfwInit())
    exit(-1);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  glfwWindowHint(GLFW_RED_BITS, mode->redBits);
  glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
  glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
  glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

  if (fullscreen)
    win = glfwCreateWindow(mode->width, mode->height, "Roche", monitor, NULL);
  else
    win = glfwCreateWindow(win_w, win_h, "Roche", NULL, NULL);

  if (!win)
  {
    glfwTerminate();
    exit(-1);
  }

  screenshot_buffer = new unsigned char[mode->width*mode->height*4];

  glfwMakeContextCurrent(win);

  // THREAD INITIALIZATION
  thread_count = 1;
  for (int i=0;i<thread_count;++i)
  {
    tl_threads.emplace_back(tlThread, std::ref(quit), std::ref(textures_to_load),std::ref(textures_to_update), std::ref(dds_loader));
  }
  screenshot_thread = new std::thread(ssThread, std::ref(quit), screenshot_buffer, std::ref(save), win);

  GLenum err = glewInit();
  if (GLEW_OK != err)
  {
    std::cout << "Some shit happened: " << glewGetErrorString(err) << std::endl;
  }

  int width, height;
  glfwGetFramebufferSize(win, &width, &height);
  glViewport(0, 0, width, height);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
  glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
  glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

  generateModels();
  loadShaders();
  loadSkybox();
  loadPlanetFiles();
  glfwGetCursorPos(win, &pre_mouseposx, &pre_mouseposy);
  ratio = width/(float)height;
  camera.getPolarPosition().z = focused_planet->getBody().radius*4;
  post_processing.create(win, ssaa_factor);
  post_processing.addShader(new HDRAction(post_hdr));
  post_processing.addShader(new PostProcessingAction(post_default));
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
  ring_obj.updateVerts(24*4, ring_vert);
  ring_obj.updateIndices(12, ring_ind);

  planet_obj.generateSphere(128,128, 1);
  atmos_obj.generateSphere(128,128, 0);
  skybox_obj.generateSphere(16,16,0);

  float flare_vert[] =
  { -1.0, -1.0, 0.0,1.0,0.0,0.0,
    +1.0, -1.0, 0.0,1.0,1.0,0.0,
    +1.0, +1.0, 0.0,1.0,1.0,1.0,
    -1.0, +1.0, 0.0,1.0,0.0,1.0
  };

  flare_obj.create();
  flare_obj.updateVerts(24*4, flare_vert);
  flare_obj.updateIndices(12, ring_ind);

  flare_tex.create();
  loadTexture("tex/flare.dds", flare_tex);
}

void Game::loadShaders()
{
  ring_shader.loadFromFile("shaders/ring.vert", "shaders/ring.frag");
  planet_shader.loadFromFile("shaders/planet.vert", "shaders/planet.frag");
  atmos_shader.loadFromFile("shaders/atmos.vert", "shaders/atmos.frag");
  sun_shader.loadFromFile("shaders/planet.vert", "shaders/sun.frag");
  skybox_shader.loadFromFile("shaders/skybox.vert", "shaders/skybox.frag");
  flare_shader.loadFromFile("shaders/flare.vert", "shaders/flare.frag");
  post_default.loadFromFile("shaders/post_fxaa.vert", "shaders/post_fxaa.frag");
  post_hdr.loadFromFile("shaders/post.vert", "shaders/post_hdr.frag");
}

void Game::loadSkybox()
{
  skybox.rot_axis = glm::vec3(1,0,0);
  skybox.rot_angle = 60.0;
  skybox.size = 100;
  skybox.tex.create();
  loadTexture("tex/skybox.dds", skybox.tex);
  skybox.load();
}

void Game::loadPlanetFiles()
{
  try 
  {
    shaun::parser p;
    shaun::object obj = p.parse(read_file("planets.sn").c_str());
    shaun::sweeper swp(&obj);

    shaun::sweeper pl(swp("planets"));
    for (unsigned int i=0;i<pl.value<shaun::list>().elements().size();++i)
    {
      planets.emplace_back();
      try
      {
        planets.back().createFromFile(pl[i]);
      }
      catch (std::string str)
      {
        std::cout << str << std::endl;
        exit(-1);
      }
    }
    for (Planet &p : planets)
    {
      p.getOrbit().setParentFromName(planets);
    }

    focused_planet = &planets[focused_planet_id];
  } 
  catch (shaun::parse_error e)
  {
    std::cout << e << std::endl;
  }
}

void Game::loadTexture(const std::string &filename, Texture &tex)
{
  textures_to_load.push(std::pair<std::string,Texture*>(filename, &tex));
}

class PlanetCompareByDist
{
public:
  PlanetCompareByDist(const glm::vec3 &view_pos)
  {
    this->view_pos = view_pos;
  }
  bool operator()(Planet *p1, Planet *p2)
  {
    return getDist(p1) > getDist(p2);
  }
  float getDist(Planet *p)
  {
    glm::vec3 temp = p->getPosition() - view_pos;
    return glm::dot(temp,temp);
  }
private:
  glm::vec3 view_pos;
};

void Game::update(double dt)
{
  for (Planet &p: planets)
  {
    p.getOrbit().reset();
  }
  for (Planet &p: planets)
  {
    p.update(epoch);
  }

  if (input.isPressed(GLFW_KEY_K))
  {
    if (time_warp_index > 0) time_warp_index--;
  }
  if (input.isPressed(GLFW_KEY_L))
  {
    if (time_warp_index < (int)time_warp_values.size()-1) time_warp_index++;
  }

  epoch += time_warp_values[time_warp_index]*dt;

  if (input.isPressed(GLFW_KEY_TAB))
  {
    if (is_switching)
    {
      is_switching = false;
      camera.getPolarPosition().z = focused_planet->getBody().radius*4;
    }
    else
    {
      switch_previous_planet = focused_planet;
      focused_planet_id = (focused_planet_id+1)%planets.size();
      focused_planet = &planets[focused_planet_id];
      switch_previous_dist = camera.getPolarPosition().z;
      is_switching = true;
    } 
  }

  double posX, posY;
  glm::vec2 move;
  glfwGetCursorPos(win, &posX, &posY);
  move.x = -posX+pre_mouseposx;
  move.y = posY-pre_mouseposy;

  camera.getCenter() = glm::vec3(0,0,0);

  if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2))
  {
    view_speed.x += move.x*sensitivity;
    view_speed.y += move.y*sensitivity;
    for (int i=0;i<2;++i)
    {
      if (view_speed[i] > max_view_speed) view_speed[i] = max_view_speed;
      if (view_speed[i] < -max_view_speed) view_speed[i] = -max_view_speed;
    }
  }
  else if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_3))
  {
    view_speed.z += (move.y*sensitivity);
  }

  camera.getPolarPosition().x += view_speed.x;
  camera.getPolarPosition().y += view_speed.y;
  camera.getPolarPosition().z *= 1.0+view_speed.z;

  view_speed *= view_smoothness;

  if (camera.getPolarPosition().y > PI/2 - 0.001)
  {
    camera.getPolarPosition().y = PI/2 - 0.001;
    view_speed.y = 0;
  }
  if (camera.getPolarPosition().y < -PI/2 + 0.001)
  {
    camera.getPolarPosition().y = -PI/2 + 0.001;
    view_speed.y = 0;
  }
  float radius = focused_planet->getBody().radius;
  if (camera.getPolarPosition().z < radius) camera.getPolarPosition().z = radius;

  pre_mouseposx = posX;
  pre_mouseposy = posY;

  if (input.isPressed(GLFW_KEY_F12) && !save)
  {
    int width,height;
    glfwGetWindowSize(win, &width, &height);
    glReadPixels(0,0,width,height, GL_RGBA, GL_UNSIGNED_BYTE, screenshot_buffer);
    save = true;
  }
}

void Game::render()
{
  if (tl_threads.empty())
  {
    while (textures_to_load.empty())
    {
      auto st = textures_to_load.front();
      dds_loader.load(st.first, *st.second, textures_to_update);
      textures_to_load.pop();
    }
  }

  TexMipmapData d;
  while (textures_to_update.try_next(d))
  {
    d.updateTexture();
  }

  if (switch_frame_current > switch_frames)
  {
    is_switching = false;
    switch_frame_current = 0;
  }

  if (is_switching)
  {
    float t = switch_frame_current/(float)switch_frames;
    float f = 6*t*t*t*t*t-15*t*t*t*t+10*t*t*t;
    view_center = (focused_planet->getPosition() - switch_previous_planet->getPosition())*f + switch_previous_planet->getPosition();
    float target_dist = focused_planet->getBody().radius*4;
    camera.getPolarPosition().z = (target_dist - switch_previous_dist)*f + switch_previous_dist;

    ++switch_frame_current;
  }
  else
  {
    view_center = focused_planet->getPosition();
  }

  post_processing.bind();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  camera.update(ratio);
  glm::mat4 proj_mat = camera.getProjMat();
  glm::mat4 view_mat = camera.getViewMat();

  std::vector<Planet*> flares; // Planets to be rendered as flares (too far away)
  std::vector<Planet*> meshes; // Planets to be rendered as their whole mesh

  // Planet sorting between flares and close meshes
  for (Planet &p : planets)
  {
    glm::vec3 dist = p.getPosition() - view_center - camera.getPosition();
    float len_dist = glm::length(dist);
    if (!p.getBody().is_star && len_dist >= MIN_FLARE_DIST)
    {
      flares.push_back(&p);
    }
    if (p.getBody().is_star || len_dist <= MAX_PLANET_DIST)
    {
      meshes.push_back(&p);
      p.load();
    } 
    else
    {
      p.unload();
    }
  }
  
  skybox.render(proj_mat, view_mat, skybox_shader, skybox_obj);
  flare_shader.use();
  flare_shader.uniform("ratio", ratio);
  flare_shader.uniform("tex", 0);
  flare_tex.use(0);
  for (Planet *flare : flares)
  {
    glm::vec4 posOnScreen = proj_mat*view_mat*glm::vec4(flare->getPosition() - view_center, 1.0);
    if (posOnScreen.z > 0)
    {
      float size_on_screen = (glm::degrees((float)atan(flare->getBody().radius/MIN_FLARE_DIST))*4)/camera.getFovy();
      float alpha = (glm::length(flare->getPosition() - view_center - camera.getPosition()) - MIN_FLARE_DIST) / (MAX_PLANET_DIST - MIN_FLARE_DIST);
      glm::vec3 color = (0.4+flare->getBody().albedo)*flare->getBody().mean_color * (
        glm::dot(
          glm::normalize(flare->getPosition() - view_center - camera.getPosition()), 
          glm::normalize(flare->getPosition()))*0.5+0.5);
      flare_shader.uniform("size", size_on_screen);
      flare_shader.uniform("color", glm::value_ptr(glm::vec4(color,std::min(1.0f,alpha))));

      glm::vec2 pOS = glm::vec2(posOnScreen/posOnScreen.w);
      flare_shader.uniform("pos", glm::value_ptr(pOS));
      flare_obj.render();
    }
  }

  rc.proj_mat = proj_mat;
  rc.view_mat = view_mat;
  rc.view_pos = camera.getPosition();
  rc.light_pos = light_position;
  rc.view_center = view_center;

  std::sort(meshes.begin(),meshes.end(), PlanetCompareByDist(camera.getPosition()+view_center));
  for (Planet *mesh : meshes)
  {
    mesh->render(rc);
  }

  post_processing.render();
  glfwSwapBuffers(win);
  glfwPollEvents();
}

bool Game::isRunning()
{
  return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}