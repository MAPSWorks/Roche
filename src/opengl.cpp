#include "opengl.h"
#include "util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <cstring>
#include <cmath>
#include <memory>

#include <thread>

#include <glm/gtc/type_ptr.hpp>

#include <glm/ext.hpp>

#define PLANET_STRIDE 24

#define PI        3.14159265358979323846264338327950288 

void Renderable::generateSphere(int theta_res, int phi_res, int exterior)
{
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, (theta_res+1)*(phi_res+1)*PLANET_STRIDE, NULL, GL_STATIC_DRAW);

  int offset = 0;
  for (float phi=0;phi<=1.0;phi += 1.0/phi_res)
  {
    float cosphi = cos(PI*(phi-0.5));
    float sinphi = sin(PI*(phi-0.5));
    for (float theta=0;theta<=1.0;theta += 1.0/theta_res)
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
  offset=0;
  for (int i=0;i<phi_res;++i)
  {
    for (int j=0;j<theta_res;++j)
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
  glDeleteProgram(program);
}
void Shader::uniform(const std::string &name,const void *value) const
{
  for (auto u=uniforms.begin(); u != uniforms.end(); ++u)
  {
    if (name == u->name)
    {
      if (u->type == matrix)
        u->func.mat(u->location, u->size, GL_FALSE, (const float*)value);
      else if (u->type == veci)
        u->func.veci(u->location, u->size, (const int*)value);
      else
        u->func.vecf(u->location, u->size, (const float*)value);
      return;
    }
  }
}

void Shader::uniform(const std::string &name, int value) const
{
  int a[] = {value};
  uniform(name, a);
}

void Shader::uniform(const std::string &name, float value) const
{
  float a[] = {value};
  uniform(name, a);
}

void Shader::uniform(const std::string &name, const glm::vec3 &value) const
{
  uniform(name, glm::value_ptr(value));
}

void Shader::uniform(const std::string &name, const glm::vec4 &value) const
{
  uniform(name, glm::value_ptr(value));
}

void Shader::uniform(const std::string &name, const glm::mat4 &value) const
{
  uniform(name, glm::value_ptr(value));
}

void Shader::use() const
{
  glUseProgram(program);
}
bool Shader::load(const std::string &vert_source, const std::string &frag_source)
{
  const int LOG_SIZE = 1024;
  GLuint vertex_id, fragment_id;
  GLint success;
  GLchar infoLog[LOG_SIZE];

  const char* vs = vert_source.c_str();
  const char* fs = frag_source.c_str();

  vertex_id = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_id, 1, &vs, NULL);
  glCompileShader(vertex_id);
  glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &success);
  glGetShaderInfoLog(vertex_id, LOG_SIZE, NULL, infoLog);
  if (strlen(infoLog)) std::cout << "VERTEX SHADER LOG :\n " << infoLog << std::endl;
  if (!success) std::cout << "VERTEX SHADER FAILED TO COMPILE" << std::endl;

  fragment_id = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_id, 1, &fs, NULL);
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
    return false;
  }
  
  glDeleteShader(vertex_id);
  glDeleteShader(fragment_id);

  int uniform_count;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
  int max_char;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_char);
  for (int i=0;i<uniform_count;++i)
  {
    char *buffer = new char[max_char];
    GLsizei len;
    GLint size;
    GLenum type;
    glGetActiveUniform(program, i, max_char, &len, &size, &type, buffer);
    GLint loc = glGetUniformLocation(program, buffer);
    Uniform u;
    u.name = std::string(buffer);
    delete buffer;
    u.location = loc;
    u.size = size;
    UniformFunc f;
    switch(type)
    {
      case GL_FLOAT     : f.vecf = glUniform1fv; break;
      case GL_FLOAT_VEC2: f.vecf = glUniform2fv; break;
      case GL_FLOAT_VEC3: f.vecf = glUniform3fv; break;
      case GL_FLOAT_VEC4: f.vecf = glUniform4fv; break;
      case GL_INT   :   f.veci = glUniform1iv; break;
      case GL_INT_VEC2: f.veci = glUniform2iv; break;
      case GL_INT_VEC3: f.veci = glUniform3iv; break;
      case GL_INT_VEC4: f.veci = glUniform4iv; break;
      case GL_FLOAT_MAT2: f.mat = glUniformMatrix2fv; break;
      case GL_FLOAT_MAT3: f.mat = glUniformMatrix3fv; break;
      case GL_FLOAT_MAT4: f.mat = glUniformMatrix4fv; break;
      default : f.veci = glUniform1iv;
    }
    u.func = f;
    if (type == GL_FLOAT_MAT2
     || type == GL_FLOAT_MAT3
     || type == GL_FLOAT_MAT4)
      u.type = matrix;
    else if (type == GL_FLOAT || 
             type == GL_FLOAT_VEC2 ||
             type == GL_FLOAT_VEC3 ||
             type == GL_FLOAT_VEC4)
      u.type = vecf;
    else u.type = veci;

    uniforms.push_back(u);
  }
  return true;
}

void Shader::loadFromFile(const std::string &vert_filename, const std::string &frag_filename)
{
  create();
  std::string vert_source = read_file(vert_filename);
  std::string frag_source = read_file(frag_filename);
  std::cout << "Compiling and linking " << vert_filename << " and " << frag_filename << "..." << std::endl;
  if (load(vert_source, frag_source))
  {
    std::cout << "Success!" << std::endl;
  }
}

Texture::Texture()
{
  created = false;
  max_level = -1;
  base_level = -1;
}

void Texture::create()
{
  if (!created)
  {
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    float aniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    created = true;
  }

}
void Texture::destroy()
{
  if (created)
  {
    glDeleteTextures(1, &id);
    created = false;
  }
}
void Texture::use(int unit) const
{
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
}
void Texture::genMipmaps()
{
  if (created)
  {
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
    glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
}

void Texture::update(const TexMipmapData &data)
{
  if (created && data.data) // i'm sorry
  {
    glBindTexture(GL_TEXTURE_2D, id);
    if (data.compressed)
      glCompressedTexImage2D(
        GL_TEXTURE_2D, 
        data.level,
        data.internalFormat,
        data.width,data.height, 0,
        data.size_or_type,
        data.data.get());
    else
      glTexImage2D(
        GL_TEXTURE_2D,
        data.level,
        data.internalFormat,
        data.width,data.height, 0,
        data.internalFormat,
        data.size_or_type,
        data.data.get());
    if (base_level == -1 || data.level < base_level)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, data.level);
    if (max_level == -1 || data.level > max_level)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, data.level);
  }
}

TexMipmapData::TexMipmapData()
{
}

TexMipmapData::TexMipmapData(const TexMipmapData& cpy)
{
  this->tex = cpy.tex;
  this->level = cpy.level;
  this->internalFormat = cpy.internalFormat;
  this->width = cpy.width;
  this->height = cpy.height;
  this->size_or_type = cpy.size_or_type;
  this->data = cpy.data;
  this->compressed = cpy.compressed;  
}

TexMipmapData::TexMipmapData(
    bool compressed,
    Texture &tex,
    int level,
    GLenum internalFormat,
    int width,
    int height,
    int size_or_type,
    void *data)
{
  std::shared_ptr<char> buf(static_cast<char*>(data));
  this->tex = &tex;
  this->level = level;
  this->internalFormat = internalFormat;
  this->width = width;
  this->height = height;
  this->size_or_type = size_or_type;
  this->data = buf;
  this->compressed = compressed;
}

void TexMipmapData::updateTexture()
{
  tex->update(*this);
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
void Renderable::updateVerts(size_t size, void* data)
{
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}
void Renderable::updateIndices(size_t size, int* data)
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size*4, data, GL_STATIC_DRAW);
  count = size;
}
void Renderable::render() const
{
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
  glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, NULL);
}

#define PRINT_STATUS(s) case s: return #s;

std::string displayFBOStatus(GLenum s)
{
  switch (s)
  {
    PRINT_STATUS(GL_FRAMEBUFFER_COMPLETE)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
    PRINT_STATUS(GL_FRAMEBUFFER_UNSUPPORTED)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
    PRINT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
    default: return "Unknown status";
  }
}

void PostProcessing::create(GLFWwindow *win, float ssaa_factor)
{
  glfwGetWindowSize(win, &width, &height);

  glGenFramebuffers(2,fbos);

  glGenTextures(2,targets);

  for (int i=0;i<2;++i)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
    glBindTexture(GL_TEXTURE_2D, targets[i]);
    glfwGetWindowSize(win, &true_width, &true_height);
    width = true_width * ssaa_factor;
    height = true_height * ssaa_factor;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targets[i], 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      std::cout << "Post-processing framebuffer failed : " << displayFBOStatus(status) << std::endl;
      exit(-1);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  //float quad[] = {0,0,1,0,1,1,0,0,1,1,0,1};
  float quad[] = {0,0,0,1,1,1,0,0,1,1,1,0};
  glGenBuffers(1,&quad_obj);
  glBindBuffer(GL_ARRAY_BUFFER, quad_obj);
  glBufferData(GL_ARRAY_BUFFER, 6*2*4, quad, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void PostProcessing::destroy()
{
  glDeleteFramebuffers(2,fbos);
  glDeleteFramebuffers(2,targets);
  glDeleteBuffers(1,&quad_obj);
}
void PostProcessing::bind()
{
  glViewport(0,0,width, height);
  glScissor(0,0,width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, shaders.size()?fbos[0]:0);
}

void PostProcessing::render()
{
  int passes = shaders.size();

  int fbo=0;
  for (int i=0;i<passes;++i)
  {
    int fbo_id = (i<passes-1)?fbos[(fbo+1)&1]:0;
    if (fbo_id==0)
    {
      glViewport(0,0,true_width,true_height);
      glScissor(0,0,true_width, true_height);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,fbo_id);
    glClear(GL_COLOR_BUFFER_BIT);
    shaders[i]->action(targets[i], width, height, true_width, true_height);
    glBindBuffer(GL_ARRAY_BUFFER, quad_obj);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(GLvoid*)0);
    glDrawArrays(GL_TRIANGLES, 0,6);
    fbo = (fbo+1)&1;
  }
}
void PostProcessing::addShader(PostProcessingAction *ppa)
{
  shaders.push_back(ppa);
}

PostProcessing::~PostProcessing()
{
  for (PostProcessingAction *ppa : shaders)
  {
    delete ppa;
  }
}

PostProcessingAction::PostProcessingAction(const Shader &s1) : s(s1)
{
}

PostProcessingAction::~PostProcessingAction()
{
}

HDRAction::HDRAction(const Shader &s) : PostProcessingAction(s)
{
  exposure = 0.5;
}

void PostProcessingAction::action(GLuint tex, int width, int height, int true_width, int true_height)
{
  s.use();
  s.uniform("tex", 0);
  glm::vec2 offset = glm::vec2(1.0/(float)true_width,1.0/(float)true_height);
  s.uniform("off",glm::value_ptr(offset));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,tex);
}

void HDRAction::action(GLuint tex, int width, int height, int true_width, int true_height)
{
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,tex);

  exposure = 0.8;

  s.use();
  s.uniform("tex", 0);
  s.uniform("exposure", exposure);
}