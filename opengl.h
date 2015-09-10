#ifndef OPENGL_H
#define OPENGL_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

typedef struct 
{
    GLuint id;
    GLenum target;
} Texture;

void create_tex(Texture *tex, GLenum target);
void delete_tex(Texture *tex);
void use_tex(Texture *tex, int unit);
void image1d32_tex(Texture *tex,int width, void* data);
void image1d24_tex(Texture *tex,int width, void* data);
void image2d32_tex(Texture *tex,int width, int height, void* data);
void image2d24_tex(Texture *tex,int width, int height, void* data);

typedef struct 
{
    GLuint vbo,ibo;
} Object;

void create_obj(Object *obj);
void delete_obj(Object *obj);
void update_obj(Object *obj, size_t size, void* data);
void render_obj(Object *obj, void (*render_fun)(void), float *model_mat);

#define UNIFORM_MAX_CHARS 64

typedef struct 
{
    GLuint program;
    char uniform_names[32*UNIFORM_MAX_CHARS];
    GLint uniforms[32];
} Shader;

void create_shader(Shader *s);
void delete_shader(Shader *s);
void load_shader(Shader *s,const char* vert_source, const char* frag_source);
void uniform1f_shader(Shader *s, int id, void *value);
void uniform2f_shader(Shader *s, int id, void *value);
void uniform3f_shader(Shader *s, int id, void *value);
void uniform4f_shader(Shader *s, int id, void *value);
void uniform_matrix3_shader(Shader *s, int id, void *value);
void uniform_matrix4_shader(Shader *s, int id, void *value);
void uniform1i_shader(Shader *s, int id, void *value);

#endif