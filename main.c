#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mat4.h"
#include "vec3.h"
#include "opengl.h"

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

#define RING_ITERATIONS 100

#define PI 3.1415926

void generate_rings(unsigned char *buffer, int size, int seed)
{
    // Starting fill
    int i,j;
    const int ref_size = 4096;
    float *ref_buffer = malloc(sizeof(float)*ref_size);
    for (i=0;i<ref_size;++i)
    {
        ref_buffer[i] = 1.0;
    }
    srand(seed);

    // gap generation
    const int max_gapsize = ref_size/20;
    for (i=0;i<RING_ITERATIONS;++i)
    {
        int gapsize = rand()%(max_gapsize);
        int gap = rand()%(ref_size-gapsize+1);
        float gap_opacity = rand()%RAND_MAX/(float)RAND_MAX;
        if (gap_opacity < 0.4) gap_opacity = 0.4;
        for (j=gap;j<gap+gapsize;++j)
        {
            ref_buffer[j] *= gap_opacity;
        }
    }
    // brightness equalization
    float mean = 0;
    for (i=0;i<ref_size;++i)
    {
        mean += ref_buffer[i];
    }
    mean /= ref_size;
    float mul = 1.0/mean;
    for (i=0;i<ref_size;++i)
    {
        ref_buffer[i] *= mul;
    }

    // fading
    const int fade = ref_size/10;
    for (i=0;i<fade;++i)
    {
        ref_buffer[ref_size-i-1] *= i/(float)fade; 
        ref_buffer[i] *= i/(float)fade;
    }
    float scale = ref_size/(float)size;
    for (i=0;i<size;++i)
    {
        float mean = 0.0;
        for (j=i*scale;j<(i+1)*scale;++j)
        {
            mean += ref_buffer[j];
        }
        mean /= scale;
        buffer[i] = (unsigned char)(mean*255);
    }
    free(ref_buffer);
}

#define PLANET_STRIDE 24

void render_planet()
{
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)16);
}

void render_rings()
{
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
}

void generate_planet(Object *obj)
{
    const int res = 64;

    float theta, phi;

    glGenBuffers(1, &obj->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferData(GL_ARRAY_BUFFER, (res+1)*(res+1)*PLANET_STRIDE, NULL, GL_STATIC_DRAW);

    int offset = 0;
    for (phi=0;phi<=1.0;phi += 1.0/res)
    {
        float cosphi = cos(PI*(phi-0.5));
        float sinphi = sin(PI*(phi-0.5));
        for (theta=0;theta<=1.0;theta += 1.0/res)
        {
            float costheta = cos(theta*PI*2);
            float sintheta = sin(theta*PI*2);
            float vertex_data[] = {cosphi*costheta, cosphi*sintheta, sinphi,1.0, theta, 1.0-phi, };
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
    for (i=0;i<res;++i)
    {
        for (j=0;j<res;++j)
        {
            int i1 = i+1;
            int j1 = j+1;
            int indices[] = {i*(res+1) + j, i1*(res+1) + j, i1*(res+1) + j1, i1*(res+1)+j1, i*(res+1) + j1, i*(res+1) + j};
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset*24, 24, indices);
            offset++;
        }
    }
    obj->count = offset*6;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void computeRingMatrix(float *ringmat, float *toward_view, float *rings_up)
{
    mat4_iden(ringmat);
    vec3_norm(toward_view, toward_view);

    float rings_right[3];
    vec3_cross(rings_up, toward_view, rings_right);
    vec3_norm(rings_right, rings_right);

    float rings_x[3];
    vec3_cross(rings_up, rings_right, rings_x);
    vec3_norm(rings_x, rings_x);
    int i;
    for (i=0;i<3;++i)
    {
        ringmat[i*4] = rings_x[i];
        ringmat[i*4+1] = rings_right[i];
        ringmat[i*4+2] = rings_up[i];
    }
}

int main(void)
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

	GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Roche", monitor, NULL);
    //GLFWwindow* window = glfwCreateWindow(512, 512, "Roche", NULL, NULL);
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
    
    Shader ring_shader, planet_shader;
    create_shader(&ring_shader);
    create_shader(&planet_shader);

    load_shader_from_file(&ring_shader, "ring.vert", "ring.frag");
    load_shader_from_file(&planet_shader, "planet.vert", "planet.frag");
    
    const int ringsize = 2048;
    unsigned char *rings = malloc(ringsize);
    generate_rings(rings, ringsize, 1909802985);

    Texture ring_tex;
    create_tex(&ring_tex);
    image_tex(&ring_tex, 1, ringsize, 0, (void*)rings);
    free(rings);
    int escape_key;

    float ring_pos[] = 
    { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
     +1.0,-1.0,0.0,1.0,+1.0,-1.0,
     +1.0,+1.0,0.0,1.0,+1.0,+1.0,
     -0.0,+1.0,0.0,1.0,-0.0,+1.0,
    };

    int ring_ind[] = {0,1,2,2,3,0};

    Object ring_obj;
    create_obj(&ring_obj);
    update_verts_obj(&ring_obj, 24*4, ring_pos);
    update_ind_obj(&ring_obj, 6, ring_ind);

    Object planet_obj;
    generate_planet(&planet_obj);

    Texture earth_day, earth_clouds;
    tex_load_from_file(&earth_day, "earth_land.png", 3);
    tex_load_from_file(&earth_clouds, "earth_clouds.png", 3);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    float camera_pos[] = {-0.4,0.0,0.14};
    float camera_direction[] = {0.0,0.0,0.0};
    float camera_up[] = {0.0,0.0,1.0};

    float angle = 1.0;

    float planetmat[16];
    mat4_iden(planetmat);
    planetmat[0] = 0.3;
    planetmat[5] = 0.3;
    planetmat[10] = 0.3;
    float ringmat[16];
    mat4_iden(ringmat);

    float ring_color[] = {0.89, 0.84, 0.68, 1.0};
    float light_dir[] = {1.0, -2.0, -0.8};
    vec3_norm(light_dir, light_dir);
    float cloud_disp[] = {0.0};

    while (!glfwWindowShouldClose(window))
    {
		escape_key = glfwGetKey(window, GLFW_KEY_ESCAPE);
		if (escape_key == GLFW_PRESS) break;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera_pos[0] = cos(angle)*0.6;
        camera_pos[1] = sin(angle)*0.6;

        angle += 0.001;
        cloud_disp[0] += 0.0001;
        camera_pos[2] += 0.001;

        float projmat[16];
        mat4_pers(projmat, 40,ratio, 0.01,20);
        float viewmat[16];
        mat4_lookAt(viewmat, camera_pos, camera_direction, camera_up);

        float toward_view[3]; int i;
        for (i=0;i<3;++i) toward_view[i] = camera_pos[i];
        float rings_up[] = {0,0,1};

        computeRingMatrix(ringmat, toward_view, rings_up);

        use_shader(&ring_shader);
        uniform(&ring_shader, "projMat", projmat);
        uniform(&ring_shader, "viewMat", viewmat);
        uniform(&ring_shader, "modelMat", ringmat);
        uniform(&ring_shader, "ring_color", ring_color);
        use_tex(&ring_tex,0);
        render_obj(&ring_obj, render_rings);

        use_shader(&planet_shader);
        uniform(&planet_shader, "projMat", projmat);
        uniform(&planet_shader, "viewMat", viewmat);
        uniform(&planet_shader, "modelMat", planetmat);
        uniform(&planet_shader, "light_dir", light_dir);
        int zero[] = {0};
        int one[] = {1};
        uniform(&planet_shader, "day_tex", zero);
        uniform(&planet_shader, "clouds_tex", one);
        uniform(&planet_shader, "cloud_disp", cloud_disp);
        uniform(&planet_shader, "view_dir", camera_pos);
        use_tex(&earth_day,0);
        use_tex(&earth_clouds,1);
        render_obj(&planet_obj, render_planet);

        for (i=0;i<3;++i) toward_view[i] = -toward_view[i];

        computeRingMatrix(ringmat, toward_view, rings_up);

        use_shader(&ring_shader);
        uniform(&ring_shader, "projMat", projmat);
        uniform(&ring_shader, "viewMat", viewmat);
        uniform(&ring_shader, "modelMat", ringmat);
        uniform(&ring_shader, "ring_color", ring_color);
        use_tex(&ring_tex,0);
        render_obj(&ring_obj, render_rings);

        mySleep(10);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete_shader(&ring_shader);
    delete_shader(&planet_shader);
    delete_tex(&ring_tex);
    
    delete_obj(&ring_obj);

    glfwTerminate();
    return 0;
}