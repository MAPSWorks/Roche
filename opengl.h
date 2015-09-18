#ifndef OPENGL_H
#define OPENGL_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

typedef struct 
{
    GLuint id;
    GLenum target;
} 
Texture;

void create_tex(Texture *tex);
void delete_tex(Texture *tex);
void use_tex(Texture *tex, int unit);
void image_tex(Texture *tex,int channels, int width, int height, void* data);
void tex_load_from_file(Texture *tex, const char *filename, int channels);

typedef struct 
{
    GLuint vbo,ibo;
    int count;
} 
Object;

void create_obj(Object *obj);
void delete_obj(Object *obj);
void update_verts_obj(Object *obj, size_t size, void* data);
void update_ind_obj(Object *obj, size_t size, int* data);
void render_obj(Object *obj, void (*render_fun)(void));

typedef union
{
	void (*vec)(GLint location, GLsizei count, GLvoid *value);
	void (*mat)(GLint location, GLsizei count, GLboolean transpose, GLvoid *value);
} 
UniformFunc;

typedef struct 
{
	char* name;
	GLint location;
	GLint size;
	int matrix; // boolean
	UniformFunc func;
}
Uniform;

typedef struct 
{
    GLuint program;
    Uniform *uniforms;
    int uniform_count;
} 
Shader;

void create_shader(Shader *s);
void delete_shader(Shader *s);
void load_shader(Shader *s,const char* vert_source, const char* frag_source);
void load_shader_from_file(Shader *s,const char* vert_filename, const char* frag_filename);
void uniform(Shader *s, const char *name, void *value);
void uniform1i(Shader *s, const char *name, int value);
void use_shader(Shader *s);

#endif