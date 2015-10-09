#ifndef OPENGL_H
#define OPENGL_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <mutex>

class Texture
{

public:
  void create();
  void destroy();
  void use(int unit);
  void image(int channels, int width, int height, void* data);
  void setFilename(const std::string &filename);
  void load();

private:
  GLuint id;
  std::mutex mutex;
  std::string filename;
};

class Renderable
{

private:
  GLuint vbo,ibo;
  int count;

public:
  void create();
  void destroy();
  void update_verts(size_t size, void* data);
  void update_ind(size_t size, int* data);
  void render();
  void generate_sphere(int theta_res, int phi_res, int exterior);
};

typedef union
{
  void (*vec)(GLint location, GLsizei count, GLvoid *value);
  void (*mat)(GLint location, GLsizei count, GLboolean transpose, GLvoid *value);
} 
UniformFunc;

typedef struct 
{
  std::string name;
  GLint location;
  GLint size;
  int matrix; // boolean
  UniformFunc func;
}
Uniform;

class Shader {

private:
  GLuint program;
  Uniform *uniforms;
  int uniform_count;

public:
  void create();
  void destroy();
  int load(const std::string &vert_source, const std::string &frag_source);
  void load_from_file(const std::string &vert_filename, const std::string &frag_filename);
  void uniform(const std::string &name, const void *value);
  void uniform(const std::string &name, int value);
  void uniform(const std::string &name, float value);
  void use();
};

#endif