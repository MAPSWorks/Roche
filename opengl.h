#ifndef OPENGL_H
#define OPENGL_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

class TexMipmapData;

/// Stores information for using an opengl texture
class Texture
{

public:
  Texture();
  void create();
  void destroy();
  void use(int unit) const;
  void update(const TexMipmapData &data);
  void genMipmaps();

private:
  GLuint id;
  bool created;
  int max_level, base_level;
};

/// Image data of a texture's mipmap level
class TexMipmapData
{
public:
  TexMipmapData();
  TexMipmapData(const TexMipmapData& cpy);
  TexMipmapData(
    bool compressed, // Indicates if the data is compressed or not (dxt/raw pixel array)
    Texture &tex, // Reference to the texture
    int level, // mipmap level to update
    GLenum internalFormat, // Format of the pixel data
    int width, // width of image
    int height, // height of image
    int size_or_type, // size of pixel array in case of compressed texture, pixel data type otherwise
    void *data); // pointer to pixel array
  void updateTexture();
private:
  Texture *tex;
  bool compressed;
  int level;
  GLenum internalFormat;
  int width;
  int height;
  int size_or_type;
  std::shared_ptr<void> data;
  friend class Texture;
};

/// Opengl buffer containing geometry data
class Renderable
{
public:
  void create();
  void destroy();
  void updateVerts(size_t size, void* data);
  void updateIndices(size_t size, int* data);
  void render() const;
  void generateSphere(int theta_res, int phi_res, int exterior);

private:
  GLuint vbo,ibo;
  int count;

};

typedef union
{
  void (*vec)(GLint location, GLsizei count, GLvoid *value); // glUniform**v function
  void (*mat)(GLint location, GLsizei count, GLboolean transpose, GLvoid *value); // glUniformMatrix**v function
} 
UniformFunc;

typedef struct 
{
  std::string name; // uniform name in GLSL program
  GLint location; // GLSL location of uniform
  GLint size; // number of components
  bool matrix; // boolean
  UniformFunc func; // glUniform function to use when updating uniform
}
Uniform; /// Interface to uniform variable in GLSL

/// Shader program using vertex and fragment shader
class Shader {

private:
  GLuint program; /// program id
  std::vector<Uniform> uniforms; /// Uniform variables in this program

public:
  void create();
  void destroy();
  /// Loads vertex and fragment shaders from strings
  bool load(const std::string &vert_source, const std::string &frag_source);
  /// Loads vertex and fragment shaders from files
  void loadFromFile(const std::string &vert_filename, const std::string &frag_filename);
  void uniform(const std::string &name, const void *value) const; /// Sets a Uniform variable's value from a pointer
  void uniform(const std::string &name, int value) const; /// Sets a uniform variable's value from an integer
  void uniform(const std::string &name, float value) const; /// Sets a uniform variable's value from a float
  void uniform(const std::string &name, const glm::vec3 &value) const;
  void uniform(const std::string &name, const glm::vec4 &value) const;
  void uniform(const std::string &name, const glm::mat4 &value) const;
  void use() const;
};

class PostProcessingAction
{
protected:
  const Shader &s;
public:
  PostProcessingAction(const Shader &s);
  virtual ~PostProcessingAction();
  virtual void action(GLuint tex, int width, int height);
};

class PostProcessing
{
private:
  GLuint fbos[2];
  GLuint targets[2];

  GLuint quad_obj;
  int width,height;
  std::vector<PostProcessingAction*> shaders;

public:
  ~PostProcessing();
  void create(GLFWwindow *win);
  void destroy();
  void bind();
  void render();
  void addShader(PostProcessingAction *ppa);
};

class HDRAction : public PostProcessingAction
{
private:
  float exposure;
public:
  HDRAction(const Shader &s);
  void action(GLuint tex, int width, int height);
};

#endif