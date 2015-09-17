#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "vecmath.h"
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

void generate_sphere(Object *obj, int exterior)
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
            if (!exterior)
            {
                indices[1] = i*(res+1) + j1;
                indices[4] = i1*(res+1) + j;
            }
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset*24, 24, indices);
            offset++;
        }
    }
    obj->count = offset*6;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

mat4 computeRingMatrix(vec3 toward_view, vec3 rings_up, float size)
{
    mat4 ring_mat = mat4_iden();
    rings_up = vec3_norm(rings_up);
    toward_view = vec3_norm(toward_view);

    vec3 rings_right = vec3_norm(vec3_cross(rings_up, toward_view));

    vec3 rings_x = vec3_norm(vec3_cross(rings_up, rings_right));
    int i;
    for (i=0;i<3;++i)
    {
        ring_mat.v[i*4] = rings_x.v[i];
        ring_mat.v[i*4+1] = rings_right.v[i];
        ring_mat.v[i*4+2] = rings_up.v[i];
    }
    return mat4_scale(ring_mat,vec3_mul(vec3n(1,1,1),size));
}

mat4 computeLightMatrix(vec3 light_dir, vec3 light_up, float planet_size, float ring_outer)
{
    mat4 light_mat = mat4_iden();
    light_up = vec3_norm(light_up);
    light_dir = vec3_norm(light_dir);
    vec3 light_right = vec3_norm(vec3_cross(light_dir, light_up));
    light_dir = vec3_mul(light_dir,ring_outer);
    light_up = vec3_mul(light_up, planet_size);
    light_right = vec3_mul(light_right, planet_size);
    int i;
    for (i=0;i<3;++i)
    {
        light_mat.v[i*4] = light_dir.v[i];
        light_mat.v[i*4+1] = light_right.v[i];
        light_mat.v[i*4+2] = light_up.v[i];
    }
    return light_mat;
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
    
    Shader ring_shader, planet_shader, skybox_shader, shadowmap_shader;
    create_shader(&ring_shader);
    create_shader(&planet_shader);
    create_shader(&skybox_shader);

    load_shader_from_file(&ring_shader, "ring.vert", "ring.frag");
    load_shader_from_file(&planet_shader, "planet.vert", "planet.frag");
    load_shader_from_file(&skybox_shader, "skybox.vert", "skybox.frag");
    load_shader_from_file(&shadowmap_shader, "shadow_map.vert", "shadow_map.frag");
    
    const int ringsize = 2048;
    unsigned char *rings = malloc(ringsize);
    generate_rings(rings, ringsize, 1909802985);

    Texture ring_tex;
    create_tex(&ring_tex);
    image_tex(&ring_tex, 1, ringsize, 1, (void*)rings);
    free(rings);
    int escape_key;

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

    Object planet_obj, skybox_obj;
    generate_sphere(&planet_obj, 1);
    generate_sphere(&skybox_obj, 0);

    Texture earth_day, earth_clouds, earth_night, skybox;
    tex_load_from_file(&earth_day, "earth_land.png", 3);
    tex_load_from_file(&earth_clouds, "earth_clouds.png", 3);
    tex_load_from_file(&earth_night, "earth_night.png", 3);
    tex_load_from_file(&skybox, "space_skybox.png", 3);

    Texture default_tex;
    create_tex(&default_tex);
    unsigned char white[] = {0xFF,0xFF,0xFF, 0xFF};
    image_tex(&default_tex, 4, 1,1, white);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    vec3 camera_pos = vec3n(-0.4,0.0,1.0);
    vec3 camera_direction = vec3n(0.0,0.0,0.0);
    vec3 camera_up = vec3n(0.0,0.0,1.0);

    float angle = 1.0;

    float ring_outer = 0.6;
    float ring_inner = 0.4;
    float ring_mindist[] = {ring_inner/ring_outer};
    float planet_size = 0.3;
    vec3 planet_scale = vec3_mul(vec3n(1,1,1),planet_size);
    mat4 planet_mat = mat4_iden();
    planet_mat = mat4_scale(planet_mat, planet_scale);

    vec3 anglerot = vec3n(1,0,0);
    quat skybox_rot = quat_rot(anglerot, 60.0/180.0*PI);
    mat4 skybox_mat = quat_tomatrix(skybox_rot);
    skybox_mat = mat4_scale(skybox_mat,vec3_mul(vec3n(1,1,1),1800));

    //float ring_color[] = {0.89, 0.84, 0.68, 1.0};
    float ring_color[] = {0.6, 0.6, 0.6, 1.0};
    vec3 light_dir = vec3_norm(vec3n(1.0, -2.0, -0.8));
    float cloud_disp[] = {0.0};

    float exposure[] = {1.0};

    const int shadowtex_size = 2048;
    GLuint shadow_fbo;
    GLuint shadow_tex;
    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);

    glGenTextures(1, &shadow_tex);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, shadowtex_size, shadowtex_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    vec3 light_up = vec3n(0,0,1);
    vec3 rings_up = vec3n(0,0,1);

    int zero[] = {0};
    int one[] = {1};
    int two[] = {2};

    while (!glfwWindowShouldClose(window))
    {
		escape_key = glfwGetKey(window, GLFW_KEY_ESCAPE);
		if (escape_key == GLFW_PRESS) break;

        camera_pos.v[0] = cos(angle)*0.6;
        camera_pos.v[1] = sin(angle)*0.6;

        angle += 0.004;
        cloud_disp[0] += 0.0004;
        camera_pos.v[2] -= 0.001;

        if (glfwGetKey(window, GLFW_KEY_E)) exposure[0] -= 0.006;
        if (glfwGetKey(window, GLFW_KEY_A)) exposure[0] += 0.006;

        if (exposure[0] <0.0) exposure[0] = 0.0;
        else if (exposure[0] > 1.0) exposure[0] = 1.0;

        mat4 light_mat = computeLightMatrix(light_dir, light_up, planet_size, ring_outer);
        vec3 toward_view = vec3_cpy(vec3_add(camera_pos, vec3_inv(camera_direction)));
        
        mat4 far_ring_mat = computeRingMatrix(toward_view, rings_up, ring_outer);
        mat4 near_ring_mat = computeRingMatrix(vec3_inv(toward_view), rings_up, ring_outer);

        glViewport(0,0,shadowtex_size, shadowtex_size);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        use_shader(&shadowmap_shader);
        uniform(&shadowmap_shader, "lightMat", light_mat.v);
        uniform(&shadowmap_shader, "minDist", ring_mindist);
        uniform(&shadowmap_shader, "tex", zero);

        use_tex(&ring_tex, 0);
        uniform(&shadowmap_shader, "modelMat", far_ring_mat.v);
        render_obj(&ring_obj, render_rings);

        use_tex(&default_tex, 0);
        uniform(&shadowmap_shader, "modelMat", planet_mat.v);
        render_obj(&planet_obj, render_planet);

        use_tex(&ring_tex, 0);
        uniform(&shadowmap_shader, "modelMat", near_ring_mat.v);
        render_obj(&ring_obj, render_rings);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        glViewport(0,0,width, height);

        mat4 proj_mat = mat4_pers(40,ratio, 0.01,2000);
        mat4 view_mat = mat4_lookAt(camera_pos, camera_direction, camera_up);

        // SKYBOX RENDER
        use_shader(&skybox_shader);
        uniform(&skybox_shader, "projMat", proj_mat.v);
        uniform(&skybox_shader, "viewMat", view_mat.v);
        uniform(&skybox_shader, "modelMat", skybox_mat.v);
        uniform(&skybox_shader, "exposure", exposure);
        uniform(&skybox_shader, "tex", zero);
        use_tex(&skybox,0);
        render_obj(&skybox_obj, render_planet); 

        // FAR RING RENDER
        use_shader(&ring_shader);
        uniform(&ring_shader, "projMat", proj_mat.v);
        uniform(&ring_shader, "viewMat", view_mat.v);
        uniform(&ring_shader, "modelMat", far_ring_mat.v);
        uniform(&ring_shader, "ring_color", ring_color);
        uniform(&ring_shader, "tex", zero);
        uniform(&ring_shader, "minDist", ring_mindist);
        use_tex(&ring_tex,0);
        render_obj(&ring_obj, render_rings);

        // PLANET RENDER
        use_shader(&planet_shader);
        uniform(&planet_shader, "projMat", proj_mat.v);
        uniform(&planet_shader, "viewMat", view_mat.v);
        uniform(&planet_shader, "modelMat", planet_mat.v);
        uniform(&planet_shader, "light_dir", light_dir.v);
        uniform(&planet_shader, "day_tex", zero);
        uniform(&planet_shader, "clouds_tex", one);
        uniform(&planet_shader, "night_tex", two);
        uniform(&planet_shader, "cloud_disp", cloud_disp);
        uniform(&planet_shader, "view_dir", camera_pos.v);
        use_tex(&earth_day,0);
        use_tex(&earth_clouds,1);
        use_tex(&earth_night,2);
        render_obj(&planet_obj, render_planet);

        // NEAR RING RENDER
        use_shader(&ring_shader);
        uniform(&ring_shader, "projMat", proj_mat.v);
        uniform(&ring_shader, "viewMat", view_mat.v);
        uniform(&ring_shader, "modelMat", near_ring_mat.v);
        uniform(&ring_shader, "ring_color", ring_color);
        uniform(&ring_shader, "tex", zero);
        uniform(&ring_shader, "minDist", ring_mindist);
        use_tex(&ring_tex,0);
        render_obj(&ring_obj, render_rings);

        mySleep(10);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete_shader(&ring_shader);
    delete_shader(&planet_shader);

    delete_tex(&ring_tex);
    delete_tex(&earth_day);
    delete_tex(&earth_night);
    delete_tex(&earth_clouds);
    delete_tex(&skybox);

    delete_obj(&ring_obj);
    delete_obj(&planet_obj);

    glfwTerminate();
    return 0;
}