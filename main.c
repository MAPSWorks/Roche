#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mat4.h"

#define RING_ITERATIONS 100

#define PI 3.1415926

void read_file(char* filename, char** buffer_ptr)
{
	long length;
	FILE * f = fopen (filename, "rb");

	if (f)
	{
	  fseek (f, 0, SEEK_END);
	  length = ftell (f);
	  fseek (f, 0, SEEK_SET);
	  char *buffer = malloc (length+1);
	  if (buffer)
	  {
	    fread (buffer, 1, length, f);
	    buffer[length] = 0;
	  }
	  fclose (f);
	  *buffer_ptr = buffer;
	}
}

void load_shaders(GLuint *program, const char* vert_source, const char* frag_source)
{
	GLuint vertex_id, fragment_id;
    GLint success;
    GLchar infoLog[512];
    
    *program = glCreateProgram();

    vertex_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_id, 1, &vert_source, NULL);
    glCompileShader(vertex_id);
    glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex_id, 512, NULL, infoLog);
        fprintf(stderr,"VERTEX SHADER FAILED TO COMPILE :\n%s\n", infoLog);
    }
    
    fragment_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_id, 1, &frag_source, NULL);
    glCompileShader(fragment_id);
    glGetShaderiv(fragment_id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment_id, 512, NULL, infoLog);
        fprintf(stderr,"FRAGMENT SHADER FAILED TO COMPILE :\n%s\n", infoLog);
    }
    
    
    glAttachShader(*program, vertex_id);
    glAttachShader(*program, fragment_id);

    glLinkProgram(*program);
    glGetProgramiv(*program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(*program, 512, NULL, infoLog);
        fprintf(stderr,"SHADER PROGRAM FAILED TO LINK :\n%s\n", infoLog);
    }
    
    glDeleteShader(vertex_id);
    glDeleteShader(fragment_id);
}

void generate_rings(unsigned char *buffer, int size, int seed)
{
    // Starting fill
    int i;
    for (i=0;i<size;++i)
    {
        buffer[i] = 255;
    }
    srand(seed);

    // gap generation
    const int max_gapsize = size/20;
    for (i=0;i<RING_ITERATIONS;++i)
    {
        int gapsize = rand()%(max_gapsize);
        int gap = rand()%(size-gapsize+1);
        float gap_opacity = (rand()%256 / 255.0);
        int j;
        for (j=gap;j<gap+gapsize;++j)
        {
            buffer[j] *= gap_opacity;
        }
    }
    // brightness equalization
    float mean = 0;
    for (i=0;i<size;++i)
    {
        mean += buffer[i];
    }
    mean /= size;
    float mul = 255.0/mean;
    for (i=0;i<size;++i)
    {
        buffer[i] *= mul;
    }

    // fading
    const int fade = size/40;
    for (i=0;i<fade;++i)
    {
        buffer[size-i] *= i/(float)fade; 
        buffer[i] *= i/(float)fade;
    }
}

void generate_planet(GLuint *vbo, GLuint *ibo)
{
    const int res = 64;

    float theta, phi;

    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, (res+1)*res*24, NULL, GL_STATIC_DRAW);

    int offset = 0;
    for (phi=0;phi<=1.0;phi += 1.0/res)
    {
        float cosphi = cos(PI*(phi-0.5));
        float sinphi = sin(PI*(phi-0.5));
        for (theta=0;theta<1.0;theta += 1.0/res)
        {
            float costheta = cos(theta*PI*2);
            float sintheta = sin(theta*PI*2);
            float vertex_data[] = {cosphi*costheta*0.3, cosphi*sintheta*0.3, sinphi*0.3,1.0, theta, phi};
            glBufferSubData(GL_ARRAY_BUFFER, offset*24, 24, vertex_data);
            offset++;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGenBuffers(1, ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,res*res*6*4,NULL,GL_STATIC_DRAW);
    int i,j;
    offset=0;
    for (i=0;i<res;++i)
    {
        for (j=0;j<res;++j)
        {
            int i1 = i+1;
            int j1 = (j+1)%res;
            int indices[] = {i*res + j, i1*res + j, i1*res + j1, i1*res+j1, i*res + j1, i*res + j};
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset*24, 24, indices);
            offset++;
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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
    
    GLuint program_ring;
    char *vert_source, *frag_source;
    read_file("ring.vert", &vert_source);
    read_file("ring.frag", &frag_source);
    load_shaders(&program_ring, vert_source, frag_source);
    free(vert_source);
    free(frag_source);

    GLuint program_planet;
    read_file("planet.vert", &vert_source);
    read_file("planet.frag", &frag_source);
    load_shaders(&program_planet, vert_source, frag_source);
    free(vert_source);
    free(frag_source);

    
    const int ringsize = 128;
    unsigned char *rings = malloc(ringsize);
    generate_rings(rings, ringsize, 1909802985);
    GLuint ring_tex;
    glGenTextures(1, &ring_tex);
    glBindTexture(GL_TEXTURE_1D, ring_tex);
    glTexImage1D(GL_TEXTURE_1D, 0, 1, ringsize,0, GL_RED,GL_UNSIGNED_BYTE, rings);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_1D, 0);
    int escape_key;

    float ring_pos[] = 
    { 0.0,-1.0,0.0,1.0,-0.0,-1.0,
     +1.0,-1.0,0.0,1.0,+1.0,-1.0,
     +1.0,+1.0,0.0,1.0,+1.0,+1.0,
     +1.0,+1.0,0.0,1.0,+1.0,+1.0,
     -0.0,+1.0,0.0,1.0,-0.0,+1.0,
     -0.0,-1.0,0.0,1.0,-0.0,-1.0};

    GLuint vbo;
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ring_pos),ring_pos,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint planetvbo, planetibo;
    generate_planet(&planetvbo, &planetibo);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    float camera_pos[] = {0.5,0.1,0.1};
    float camera_direction[] = {-1,0.0,0.0};
    float camera_up[] = {0.0,0.0,1.0};

    float viewprojmat[16];

    GLint ring_color_location = glGetUniformLocation(program_ring, "ring_color");
    GLint ring_viewprojmat_loc = glGetUniformLocation(program_ring, "viewprojMat");
    GLint planet_viewprojmat_loc = glGetUniformLocation(program_planet, "viewprojMat");
    GLint ring_modelmat_loc = glGetUniformLocation(program_ring, "modelMat");
    GLint planet_modelmat_loc = glGetUniformLocation(program_planet, "modelMat");

    float angle = 0;

    float planetmat[16];
    mat4_iden(planetmat);
    float ringmat[16];
    mat4_iden(ringmat);

    while (!glfwWindowShouldClose(window))
    {
		escape_key = glfwGetKey(window, GLFW_KEY_ESCAPE);
		if (escape_key == GLFW_PRESS) break;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera_direction[0] = cos(angle);
        camera_direction[1] = sin(angle);

        angle += 0.01;

        float projmat[16];
        mat4_pers(projmat, 40,ratio, 0.01,20);
        float viewmat[16];
        mat4_lookAt(viewmat, camera_pos, camera_direction, camera_up);
        mat4_mul(projmat, viewmat, viewprojmat);

        ringmat[0] = -1;

        glUseProgram(program_ring);
        glUniformMatrix4fv(ring_viewprojmat_loc, 1, GL_FALSE, viewprojmat);
        glUniformMatrix4fv(ring_modelmat_loc, 1, GL_FALSE, ringmat);
        glUniform4f(ring_color_location, 0.89, 0.84, 0.68, 1.0);
        glBindTexture(GL_TEXTURE_1D, ring_tex);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
        glDrawArrays(GL_TRIANGLES, 0,6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_1D, 0);

        glUseProgram(program_planet);
        glUniformMatrix4fv(planet_viewprojmat_loc, 1, GL_FALSE, viewprojmat);
        glUniformMatrix4fv(planet_modelmat_loc, 1, GL_FALSE, planetmat);
        glBindBuffer(GL_ARRAY_BUFFER, planetvbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planetibo);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
        glDrawElements(GL_TRIANGLES, 64*64*6,GL_UNSIGNED_INT, NULL);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        ringmat[0] = 1;

        glUseProgram(program_ring);
        glUniformMatrix4fv(ring_viewprojmat_loc, 1, GL_FALSE, viewprojmat);
        glUniformMatrix4fv(ring_modelmat_loc, 1, GL_FALSE, ringmat);
        glUniform4f(ring_color_location, 0.89, 0.84, 0.68, 1.0);
        glBindTexture(GL_TEXTURE_1D, ring_tex);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
        glDrawArrays(GL_TRIANGLES, 0,6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_1D, 0);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteTextures(1, &ring_tex);
    glDeleteBuffers(1, &vbo);
    glfwTerminate();
    return 0;
}