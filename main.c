#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "vecmath.h"
#include "opengl.h"
#include "planet.h"

#ifdef LINUX
#include <unistd.h>
#endif
#ifdef WINDOWS
#include <windows.h>
#endif

void mySleep(int sleepMs)
{
#ifdef LINUX
    usleep(sleepMs * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
#ifdef WINDOWS
    Sleep(sleepMs);
#endif
}

#define PLANET_STRIDE 24

void generate_sphere(Object *obj, int theta_res, int phi_res, int exterior)
{
    float theta, phi;

    glGenBuffers(1, &obj->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferData(GL_ARRAY_BUFFER, (theta_res+1)*(phi_res+1)*PLANET_STRIDE, NULL, GL_STATIC_DRAW);

    int offset = 0;
    for (phi=0;phi<=1.0;phi += 1.0/phi_res)
    {
        float cosphi = cos(PI*(phi-0.5));
        float sinphi = sin(PI*(phi-0.5));
        for (theta=0;theta<=1.0;theta += 1.0/theta_res)
        {
            float costheta = cos(theta*PI*2);
            float sintheta = sin(theta*PI*2);
            float vertex_data[] = {cosphi*costheta, cosphi*sintheta, sinphi,1.0, theta, 1.0-phi};
            glBufferSubData(GL_ARRAY_BUFFER, offset*PLANET_STRIDE, PLANET_STRIDE, vertex_data);
            offset++;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGenBuffers(1, &obj->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,offset*24,NULL,GL_STATIC_DRAW);
    int i,j;
    offset=0;
    for (i=0;i<phi_res;++i)
    {
        for (j=0;j<theta_res;++j)
        {
            int i1 = i+1;
            int j1 = j+1;
            int indices[] = {i*(phi_res+1) + j, i1*(phi_res+1) + j, i1*(phi_res+1) + j1, i1*(phi_res+1)+j1, i*(phi_res+1) + j1, i*(phi_res+1) + j};
            if (!exterior)
            {
                indices[1] = i*(phi_res+1) + j1;
                indices[4] = i1*(phi_res+1) + j;
            }
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset*24, 24, indices);
            offset++;
        }
    }
    obj->count = offset*6;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

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
    	fprintf(stderr,"Some shit happened: %s\n", glewGetErrorString(err));
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
    create_shader(&ring_shader);
    create_shader(&planet_shader);
    create_shader(&skybox_shader);
    load_shader_from_file(&ring_shader, "ring.vert", "ring.frag");
    load_shader_from_file(&planet_shader, "planet.vert", "planet.frag");
    load_shader_from_file(&skybox_shader, "skybox.vert", "skybox.frag");
    
    int escape_key;
    
    // MODEL GENERATION
    float ring_pos[] = 
    { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
     +1.0,-1.0,0.0,1.0,+1.0,-1.0,
     +1.0,+1.0,0.0,1.0,+1.0,+1.0,
     -0.0,+1.0,0.0,1.0,-0.0,+1.0,
    };

    int ring_ind[] = {0,1,2,2,3,0,0,3,2,2,1,0};

    Object ring_obj;
    create_obj(&ring_obj);
    update_verts_obj(&ring_obj, 24*4, ring_pos);
    update_ind_obj(&ring_obj, 12, ring_ind);

    Object planet_obj;
    generate_sphere(&planet_obj, 128,128, 1);
    
    Object skybox_obj;
    generate_sphere(&skybox_obj, 16,16,0);
    
    // PLANET & SKYBOX INIT
    Planet earth;
    earth.pos = vec3n(0,0,0);
    earth.rot_axis = vec3n(0,0,1);
    earth.rot_epoch = 0.0;
    earth.radius = 0.3;
    earth.ring_inner = 0.4;
    earth.ring_outer = 0.6;
    earth.ring_upvector = vec3_norm(vec3n(1,1,2));
    earth.ring_seed = 1909802985;
    earth.ring_color = vec3n(0.6,0.6,0.6);
    earth.has_rings = 1;
    earth.atmos_color = vec3n(0.6,0.8,1.0);
    earth.cloud_epoch = 0.0;
    strcpy(earth.day_filename, "earth_land.png");
    strcpy(earth.night_filename,"earth_night.png");
    strcpy(earth.clouds_filename, "earth_clouds.png");
    
    Skybox skybox;
    strcpy(skybox.tex_filename,"space_skybox.png");
    skybox.rot_axis = vec3n(1,0,0);
    skybox.rot_angle = 60.0;
    skybox.size = 1800;

    planet_load(&earth);
    skybox_load(&skybox);
    
    // GLOBAL SETTINGS
    vec3 light_dir = vec3_norm(vec3n(1.0, -2.0, -0.8));

    vec3 camera_pos = vec3_mul(light_dir, -1);
    vec3 camera_angle = vec3n(0.0,0.0,0.0);
    vec3 camera_up = vec3n(0.0,0.0,1.0);

    vec3 light_up = vec3n(0,0,1);
    vec3 rings_up = vec3_norm(vec3n(1,-1,2));

    float sensibility = 0.001;
    float speed = 0.006;
    float planet_angle = 0.0;
    float light_angle = 0.0;

    while (!glfwWindowShouldClose(window))
    {
		escape_key = glfwGetKey(window, GLFW_KEY_ESCAPE);
		if (escape_key == GLFW_PRESS) break;

        light_dir.v[0] = cos(light_angle);
        light_dir.v[1] = sin(light_angle);
        light_dir.v[2] = cos(light_angle*0.5)+0.5;
        light_dir = vec3_norm(light_dir);
        light_angle += 0.002;

        double moveX, moveY;
        if (fullscreen || glfwGetKey(window, GLFW_KEY_TAB))
        {
            glfwGetCursorPos(window, &moveX, &moveY);
            moveX -= width/2;
            moveY -= height/2;
            camera_angle.v[0] -= moveX*sensibility;
            camera_angle.v[1] -= moveY*sensibility;
            if (camera_angle.v[1] > PI/2) camera_angle.v[1] = PI/2;
            if (camera_angle.v[1] < -PI/2) camera_angle.v[1] = -PI/2;
            glfwSetCursorPos(window, width/2, height/2);
        }

        vec3 camera_dir = vec3n(cos(camera_angle.v[0])*cos(camera_angle.v[1]), sin(camera_angle.v[0])*cos(camera_angle.v[1]),sin(camera_angle.v[1]));
        vec3 camera_right = vec3_norm(vec3_cross(camera_dir, camera_up));

        if (glfwGetKey(window,GLFW_KEY_W)) camera_pos = vec3_add(camera_pos, vec3_mul(camera_dir, speed));
        else if (glfwGetKey(window, GLFW_KEY_S)) camera_pos = vec3_add(camera_pos, vec3_mul(camera_dir, -speed));
        if (glfwGetKey(window,GLFW_KEY_D)) camera_pos = vec3_add(camera_pos, vec3_mul(camera_right, speed));
        else if (glfwGetKey(window,GLFW_KEY_A)) camera_pos = vec3_add(camera_pos, vec3_mul(camera_right, -speed));

        if (glfwGetKey(window,GLFW_KEY_SPACE)) camera_pos.v[2] += speed;
        else if (glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)) camera_pos.v[2] -= speed;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        glViewport(0,0,width, height);

        mat4 proj_mat = mat4_pers(40,ratio, 0.01,2000);
        mat4 view_mat = mat4_lookAt(camera_pos, vec3_add(camera_pos, camera_dir), camera_up);

        skybox_render(&skybox, proj_mat, view_mat, &skybox_shader, &skybox_obj);
        planet_render(&earth, proj_mat, view_mat, camera_dir, light_dir, &planet_shader, &ring_shader, &planet_obj, &ring_obj);

        mySleep(10);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete_shader(&ring_shader);
    delete_shader(&planet_shader);

    delete_obj(&ring_obj);
    delete_obj(&planet_obj);
    delete_obj(&skybox_obj);

    glfwTerminate();
    return 0;
}
