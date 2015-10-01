#ifndef GAME_H
#define GAME_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl.h"
#include "planet.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <vector>

#define PI        3.14159265358979323846264338327950288 

class Window
{
public:
    Window();
    ~Window();
    void create();
    void update();
    float getAspectRatio();
    GLFWwindow *getWindow();
private:
    GLFWwindow *win;
};

class Camera
{
public:
    Camera();
    void update(float ratio);
    void setCenter(glm::vec3 center);
    void setPolarPosition(glm::vec3 pos);
    const glm::vec3 &getPolarPosition();
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

    std::vector<Planet> planets;
    glm::vec3 light_position;
    Renderable skybox_obj, planet_obj, ring_obj;
    Shader skybox_shader, planet_shader, ring_shader;
    Skybox skybox;

    double preMousePosX, preMousePosY;

    float sensibility;

    Window window;
    Camera camera;

};

#endif