#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cmath>
#include <cstring>

#include "opengl.h"
#include "planet.h"

#include <glm/glm.hpp>

#ifdef LINUX
#include <unistd.h>
#endif
#ifdef WINDOWS
#include <windows.h>
#endif

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

void mySleep(int sleepMs)
{
#ifdef LINUX
    usleep(sleepMs * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
#ifdef WINDOWS
    Sleep(sleepMs);
#endif
}

#define PI        3.14159265358979323846264338327950288 

int main(int argc, char **argv)
{
    if (!glfwInit())
        return -1;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_SAMPLES, 16);

    GLFWwindow* window;
    int fullscreen;

    if (argc >= 2 && strcmp(argv[1], "-w")==0)
    {
	   window = glfwCreateWindow(768, 768, "Roche", NULL, NULL);
       fullscreen = 0;
    }
    else
    {
       window = glfwCreateWindow(mode->width, mode->height, "Roche", monitor, NULL);
       fullscreen = 1;
    }

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
    	std::cout << "Some shit happened: " << glewGetErrorString(err) << std::endl;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float ratio = width / (float) height;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    glfwSetCursorPos(window, width/2, height/2);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    
    // SHADER LOADING
    Shader ring_shader, planet_shader, skybox_shader;
    ring_shader.create();
    planet_shader.create();
    skybox_shader.create();
    ring_shader.load_from_file("ring.vert", "ring.frag");
    planet_shader.load_from_file("planet.vert", "planet.frag");
    skybox_shader.load_from_file("skybox.vert", "skybox.frag");
    
    int escape_key;
    
    // MODEL GENERATION
    float ring_pos[] = 
    { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
     +1.0,-1.0,0.0,1.0,+1.0,-1.0,
     +1.0,+1.0,0.0,1.0,+1.0,+1.0,
     -0.0,+1.0,0.0,1.0,-0.0,+1.0,
    };

    int ring_ind[] = {0,1,2,2,3,0,0,3,2,2,1,0};

    Renderable ring_obj;
    ring_obj.create();
    ring_obj.update_verts(24*4, ring_pos);
    ring_obj.update_ind(12, ring_ind);

    Renderable planet_obj, skybox_obj;
    planet_obj.generate_sphere(128,128, 1);
    skybox_obj.generate_sphere(16,16,0);
    
    // PLANET & SKYBOX INIT
    Planet earth;
    earth.pos = glm::vec3(0,0,0);
    earth.rot_axis = glm::vec3(0,0,1);
    earth.rot_epoch = 0.0;
    earth.radius = 0.3;
    earth.ring_inner = 0.4;
    earth.ring_outer = 0.6;
    earth.ring_upvector = glm::normalize(glm::vec3(1,1,2));
    earth.ring_seed = 1909802985;
    earth.ring_color = glm::vec4(0.6,0.6,0.6,1.0);
    earth.has_rings = 1;
    earth.atmos_color = glm::vec3(0.6,0.8,1.0);
    earth.cloud_epoch = 0.0;
    earth.day_filename = "earth_land.png";
    earth.night_filename = "earth_night.png";
    earth.clouds_filename = "earth_clouds.png";
    
    Skybox skybox;
    skybox.tex_filename = "space_skybox.png";
    skybox.rot_axis = glm::vec3(1,0,0);
    skybox.rot_angle = 60.0;
    skybox.size = 1800;

    earth.load();
    skybox.load();

    // GLOBAL SETTINGS
    glm::vec3 light_dir = glm::normalize(glm::vec3(1.0, -2.0, -0.8));

    glm::vec3 camera_pos = - light_dir;
    glm::vec3 camera_angle = glm::vec3(0,0,0);
    glm::vec3 camera_up = glm::vec3(0,0,1);

    float sensibility = 0.001;
    float speed = 0.006;
    float light_angle = 0.0;

    while (!glfwWindowShouldClose(window))
    {
		escape_key = glfwGetKey(window, GLFW_KEY_ESCAPE);
		if (escape_key == GLFW_PRESS) break;

        light_dir[0] = cos(light_angle);
        light_dir[1] = sin(light_angle);
        light_dir[2] = cos(light_angle*0.5)+0.5;
        light_dir = glm::normalize(light_dir);
        light_angle += 0.002;

        earth.pos[0] += 0.001;

        double moveX, moveY;
        if (fullscreen || glfwGetKey(window, GLFW_KEY_TAB))
        {
            glfwGetCursorPos(window, &moveX, &moveY);
            moveX -= width/2;
            moveY -= height/2;
            camera_angle[0] -= moveX*sensibility;
            camera_angle[1] -= moveY*sensibility;
            if (camera_angle[1] > PI/2) camera_angle[1] = PI/2;
            if (camera_angle[1] < -PI/2) camera_angle[1] = -PI/2;
            glfwSetCursorPos(window, width/2, height/2);
        }

        glm::vec3 camera_dir = glm::vec3(cos(camera_angle[0])*cos(camera_angle[1]), sin(camera_angle[0])*cos(camera_angle[1]),sin(camera_angle[1]));
        glm::vec3 camera_right = glm::normalize(glm::cross(camera_dir, camera_up));

        if (glfwGetKey(window,GLFW_KEY_W)) camera_pos += camera_dir*speed;
        else if (glfwGetKey(window, GLFW_KEY_S)) camera_pos -= camera_dir*speed;
        if (glfwGetKey(window,GLFW_KEY_D)) camera_pos += camera_right*speed;
        else if (glfwGetKey(window,GLFW_KEY_A)) camera_pos -= camera_right*speed;

        if (glfwGetKey(window,GLFW_KEY_SPACE)) camera_pos[2] += speed;
        else if (glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)) camera_pos[2] -= speed;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        glViewport(0,0,width, height);

        glm::mat4 proj_mat = glm::perspective((float)(65.0/180*PI),ratio, 0.01F,2000.0f);
        glm::mat4 view_mat = glm::lookAt(camera_pos, camera_pos + camera_dir, camera_up);

        skybox.render(proj_mat, view_mat, skybox_shader, skybox_obj);
        earth.render(proj_mat, view_mat, camera_pos, light_dir, planet_shader, ring_shader, planet_obj, ring_obj);

        mySleep(10);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    ring_shader.destroy();
    planet_shader.destroy();

    ring_obj.destroy();
    planet_obj.destroy();
    skybox_obj.destroy();

    glfwTerminate();
    return 0;
}
