#include "opengl.h"
#include "lodepng.h"

extern "C" {
    #include "util.h"
}

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cstring>
#include <math.h>

#define PLANET_STRIDE 24

#define PI        3.14159265358979323846264338327950288 

void Renderable::generate_sphere(int theta_res, int phi_res, int exterior)
{
    float theta, phi;

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
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
            float vertex_data[] = {cosphi*costheta, cosphi*sintheta, sinphi,1.0, theta, 1.0f-phi};
            glBufferSubData(GL_ARRAY_BUFFER, offset*PLANET_STRIDE, PLANET_STRIDE, vertex_data);
            offset++;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
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
    count = offset*6;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void Shader::create()
{
	program = glCreateProgram();
}
void Shader::destroy()
{
	int i;
	for (i=0;i<uniform_count;++i)
	{
		delete [] uniforms[i].name;
	}
	if (uniforms) delete [] uniforms;
	glDeleteProgram(program);
}
void Shader::uniform(const char *name, void *value)
{
	int i;
	for (i=0;i<uniform_count;++i)
	{
		Uniform *u = &uniforms[i];
		if (!strcmp(name, u->name))
		{
			if (u->matrix)
				u->func.mat(u->location, u->size, GL_FALSE, value);
			else
				u->func.vec(u->location, u->size, value);
			return;
		}
	}
}

void Shader::uniform(const char *name, int value)
{
    int a[] = {value};
    uniform(name, a);
}

void Shader::uniform(const char *name, float value)
{
    float a[] = {value};
    uniform(name, a);
}

void Shader::use()
{
	glUseProgram(program);
}
int Shader::load(const char* vert_source, const char* frag_source)
{
    const int LOG_SIZE = 1024;
	GLuint vertex_id, fragment_id;
    GLint success;
    GLchar infoLog[LOG_SIZE];

    vertex_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_id, 1, &vert_source, NULL);
    glCompileShader(vertex_id);
    glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &success);
    glGetShaderInfoLog(vertex_id, LOG_SIZE, NULL, infoLog);
    if (strlen(infoLog)) std::cout << "VERTEX SHADER LOG :\n " << infoLog << std::endl;
    if (!success) std::cout << "VERTEX SHADER FAILED TO COMPILE" << std::endl;

    fragment_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_id, 1, &frag_source, NULL);
    glCompileShader(fragment_id);
    glGetShaderiv(fragment_id, GL_COMPILE_STATUS, &success);
    glGetShaderInfoLog(fragment_id, LOG_SIZE, NULL, infoLog);
    if (strlen(infoLog)) std::cout << "FRAGMENT SHADER LOG :\n " << infoLog << std::endl;
    if (!success) std::cout << "FRAGMENT SHADER FAILED TO COMPILE" << std::endl;
    
    glAttachShader(program, vertex_id);
    glAttachShader(program, fragment_id);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    glGetShaderInfoLog(program, LOG_SIZE, NULL, infoLog);
    if (strlen(infoLog)) std::cout << "SHADER PROGRAM LOG :\n " << infoLog << std::endl;
    if (!success)
    {
        std::cout << "SHADER PROGRAM FAILED TO LINK" << std::endl;
        return 0;
    }
    
    glDeleteShader(vertex_id);
    glDeleteShader(fragment_id);

    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
    int i;
    uniforms = new Uniform[uniform_count];
    int max_char;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_char);
    for (i=0;i<uniform_count;++i)
    {
    	char *buffer = new char[max_char];
    	GLsizei len;
    	GLint size;
    	GLenum type;
    	glGetActiveUniform(program, i, max_char, &len, &size, &type, buffer);
    	GLint loc = glGetUniformLocation(program, buffer);
    	uniforms[i].name = new char[len+1];
    	strcpy(uniforms[i].name, buffer);
    	delete [] buffer;
    	uniforms[i].location = loc;
    	uniforms[i].size = size;
    	UniformFunc f;
    	switch(type)
    	{
    		case GL_FLOAT 	  : f.vec = glUniform1fv; break;
    		case GL_FLOAT_VEC2: f.vec = glUniform2fv; break;
    		case GL_FLOAT_VEC3: f.vec = glUniform3fv; break;
    		case GL_FLOAT_VEC4: f.vec = glUniform4fv; break;
    		case GL_INT 	: f.vec = glUniform1iv; break;
    		case GL_INT_VEC2: f.vec = glUniform2iv; break;
    		case GL_INT_VEC3: f.vec = glUniform3iv; break;
    		case GL_INT_VEC4: f.vec = glUniform4iv; break;
    		case GL_FLOAT_MAT2: f.mat = glUniformMatrix2fv; break;
    		case GL_FLOAT_MAT3: f.mat = glUniformMatrix3fv; break;
    		case GL_FLOAT_MAT4: f.mat = glUniformMatrix4fv; break;
    		default : f.vec = glUniform1iv;
    	}
    	uniforms[i].func = f;
    	uniforms[i].matrix = type == GL_FLOAT_MAT2
    						 || type == GL_FLOAT_MAT3
						 	 || type == GL_FLOAT_MAT4;
	}
    return 1;
}

void Shader::load_from_file(const char* vert_filename, const char* frag_filename)
{
	char *vert_source, *frag_source;
    read_file(vert_filename, &vert_source);
    read_file(frag_filename, &frag_source);
    std::cout << "Compiling and linking " << vert_filename << " and " << frag_filename << "..." << std::endl;
    if (load(vert_source, frag_source))
    {
        std::cout << "Success!" << std::endl;
    }
    delete [] vert_source;
    delete [] frag_source;
}

void Texture::create()
{
	glGenTextures(1, &id);
	target = GL_TEXTURE_2D;
}
void Texture::destroy()
{
	glDeleteTextures(1, &id);
}
void Texture::use(int unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, id);
}
GLenum format(int channels)
{
	switch (channels)
	{
		case 1: return GL_DEPTH_COMPONENT;
		case 2: return GL_RG;
		case 3: return GL_RGB;
		case 4: return GL_RGBA;
		default: return GL_RGBA;
	}
}
void Texture::image(int channels, int width, int height, void* data)
{
	glBindTexture(target, id);
	glTexImage2D(target, 0, format(channels), width, height, 0, format(channels),GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(target);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
    float aniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    glBindTexture(target, 0);
}

void Texture::load_from_file(const char *filename, int channels)
{
    unsigned int error;
    unsigned char *data;
    unsigned int width, height;
    if (channels == 3) error = lodepng_decode24_file(&data, &width, &height, filename);
    else error = lodepng_decode32_file(&data, &width, &height, filename);
    
    if (error) std::cout << "Error loading file " << filename << ": " << lodepng_error_text(error) << std::endl;
    else
    {
        create();
        image(channels, width, height, data);
    }
    delete [] data;
}

void Renderable::create()
{
	glGenBuffers(1,&vbo);
	glGenBuffers(1,&ibo);
	count = 0;
}
void Renderable::destroy()
{
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ibo);
}
void Renderable::update_verts(size_t size, void* data)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}
void Renderable::update_ind(size_t size, int* data)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size*4, data, GL_STATIC_DRAW);
	count = size;
}
void Renderable::render(void (*render_fun)(void))
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	render_fun();
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, NULL);
}