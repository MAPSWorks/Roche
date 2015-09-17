#include "opengl.h"
#include "util.h"
#include "lodepng.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void create_shader(Shader *s)
{
	s->program = glCreateProgram();
}
void delete_shader(Shader *s)
{
	int i;
	for (i=0;i<s->uniform_count;++i)
	{
		free(s->uniforms[i].name);
	}
	if (s->uniforms) free(s->uniforms);
	glDeleteProgram(s->program);
}
void uniform(Shader *s, const char *name, void *value)
{
	int i;
	for (i=0;i<s->uniform_count;++i)
	{
		Uniform *u = &s->uniforms[i];
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

void use_shader(Shader *s)
{
	glUseProgram(s->program);
}
void load_shader(Shader *s, const char* vert_source, const char* frag_source)
{
	GLuint vertex_id, fragment_id;
    GLint success;
    GLchar infoLog[512];

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
    
    glAttachShader(s->program, vertex_id);
    glAttachShader(s->program, fragment_id);

    glLinkProgram(s->program);
    glGetProgramiv(s->program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(s->program, 512, NULL, infoLog);
        fprintf(stderr,"SHADER PROGRAM FAILED TO LINK :\n%s\n", infoLog);
    }
    
    glDeleteShader(vertex_id);
    glDeleteShader(fragment_id);

    glGetProgramiv(s->program, GL_ACTIVE_UNIFORMS, &s->uniform_count);
    int i;
    s->uniforms = malloc(sizeof(Uniform)*s->uniform_count);
    int max_char;
    glGetProgramiv(s->program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_char);
    for (i=0;i<s->uniform_count;++i)
    {
    	char *buffer = malloc(max_char);
    	GLsizei len;
    	GLint size;
    	GLenum type;
    	glGetActiveUniform(s->program, i, max_char, &len, &size, &type, buffer);
    	GLint loc = glGetUniformLocation(s->program, buffer);
    	s->uniforms[i].name = malloc(len+1);
    	strcpy(s->uniforms[i].name, buffer);
    	free(buffer);
    	s->uniforms[i].location = loc;
    	s->uniforms[i].size = size;
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
    	s->uniforms[i].func = f;
    	s->uniforms[i].matrix = type == GL_FLOAT_MAT2
    						 || type == GL_FLOAT_MAT3
						 	 || type == GL_FLOAT_MAT4;
	}
}

void load_shader_from_file(Shader *s,const char* vert_filename, const char* frag_filename)
{
	char *vert_source, *frag_source;
    read_file(vert_filename, &vert_source);
    read_file(frag_filename, &frag_source);
    load_shader(s, vert_source, frag_source);
    free(vert_source);
    free(frag_source);
}

void create_tex(Texture *tex)
{
	glGenTextures(1, &tex->id);
	tex->target = GL_TEXTURE_2D;
}
void delete_tex(Texture *tex)
{
	glDeleteTextures(1, &tex->id);
}
void use_tex(Texture *tex, int unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(tex->target, tex->id);
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
void image_tex(Texture *tex,int channels, int width, int height, void* data)
{
	glBindTexture(tex->target, tex->id);
	glTexImage2D(tex->target, 0, format(channels), width, height, 0, format(channels),GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(tex->target);
	glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
    float aniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    glTexParameterf(tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    glBindTexture(tex->target, 0);
}

void tex_load_from_file(Texture *tex, const char *filename, int channels)
{
    unsigned int error;
    unsigned char *data;
    unsigned int width, height;
    if (channels == 3) error = lodepng_decode24_file(&data, &width, &height, filename);
    else error = lodepng_decode32_file(&data, &width, &height, filename);
    
    if (error) printf("Error %u loading file %s: %s\n", error, filename, lodepng_error_text(error));
    else
    {
        create_tex(tex);
        image_tex(tex, channels, width, height, data);
    }
    free(data);
}

void create_obj(Object *obj)
{
	glGenBuffers(1,&obj->vbo);
	glGenBuffers(1,&obj->ibo);
	obj->count = 0;
}
void delete_obj(Object *obj)
{
	glDeleteBuffers(1, &obj->vbo);
	glDeleteBuffers(1, &obj->ibo);
}
void update_verts_obj(Object *obj, size_t size, void* data)
{
	glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}
void update_ind_obj(Object *obj, size_t size, int* data)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size*4, data, GL_STATIC_DRAW);
	obj->count = size;
}
void render_obj(Object *obj, void (*render_fun)(void))
{
	glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->ibo);
	render_fun();
	glDrawElements(GL_TRIANGLES, obj->count, GL_UNSIGNED_INT, NULL);
}