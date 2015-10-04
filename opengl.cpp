#include "opengl.h"
#include "lodepng.h"
#include "util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <cstring>

#include <iostream>
#include <string>
#include <cmath>

#include <thread>

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
  if (uniforms) delete [] uniforms;
  glDeleteProgram(program);
}
void Shader::uniform(const std::string &name, void *value)
{
  int i;
  for (i=0;i<uniform_count;++i)
  {
    Uniform *u = &uniforms[i];
    if (name == u->name)
    {
      if (u->matrix)
        u->func.mat(u->location, u->size, GL_FALSE, value);
      else
        u->func.vec(u->location, u->size, value);
      return;
    }
  }
}

void Shader::uniform(const std::string &name, int value)
{
  int a[] = {value};
  uniform(name, a);
}

void Shader::uniform(const std::string &name, float value)
{
  float a[] = {value};
  uniform(name, a);
}

void Shader::use()
{
  glUseProgram(program);
}
int Shader::load(const std::string &vert_source, const std::string &frag_source)
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
    uniforms[i].name = std::string(buffer);
    uniforms[i].location = loc;
    uniforms[i].size = size;
    UniformFunc f;
    switch(type)
    {
      case GL_FLOAT     : f.vec = glUniform1fv; break;
      case GL_FLOAT_VEC2: f.vec = glUniform2fv; break;
      case GL_FLOAT_VEC3: f.vec = glUniform3fv; break;
      case GL_FLOAT_VEC4: f.vec = glUniform4fv; break;
      case GL_INT   : f.vec = glUniform1iv; break;
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

void Shader::load_from_file(const std::string &vert_filename, const std::string &frag_filename)
{
  std::string vert_source = read_file(vert_filename);
  std::string frag_source = read_file(frag_filename);
  std::cout << "Compiling and linking " << vert_filename << " and " << frag_filename << "..." << std::endl;
  if (load(vert_source, frag_source))
  {
    std::cout << "Success!" << std::endl;
  }
}

void Texture::create()
{
  mutex.lock();
  glGenTextures(1, &id);
  mutex.unlock();
}
void Texture::destroy()
{
  mutex.lock();
  glDeleteTextures(1, &id);
  mutex.unlock();
}
void Texture::use(int unit)
{
  mutex.lock();
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
  mutex.unlock();
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
  mutex.lock();
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, format(channels), width, height, 0, format(channels),GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
  float aniso;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
  glBindTexture(GL_TEXTURE_2D, 0);
  mutex.unlock();
}

/*
void Texture::load_from_file(const std::string &filename, int channels)
{
  unsigned int error;
  unsigned char *data;
  unsigned int width, height;
  if (channels == 3) error = lodepng_decode24_file(&data, &width, &height, filename.c_str());
  else error = lodepng_decode32_file(&data, &width, &height, filename.c_str());
  
  if (error) std::cout << "Error loading file " << filename << ": " << lodepng_error_text(error) << std::endl;
  else
  {
    create();
    image(channels, width, height, data);
  }
  delete [] data;
}
*/

void Texture::setFilename(const std::string &filename)
{
  this->filename = filename;
}

typedef unsigned int DWORD;

struct DDS_PIXELFORMAT {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwFourCC;
  DWORD dwRGBBitCount;
  DWORD dwRBitMask;
  DWORD dwGBitMask;
  DWORD dwBBitMask;
  DWORD dwABitMask;
};

typedef struct {
  DWORD           dwSize;
  DWORD           dwFlags;
  DWORD           dwHeight;
  DWORD           dwWidth;
  DWORD           dwPitchOrLinearSize;
  DWORD           dwDepth;
  DWORD           dwMipMapCount;
  DWORD           dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  DWORD           dwCaps;
  DWORD           dwCaps2;
  DWORD           dwCaps3;
  DWORD           dwCaps4;
  DWORD           dwReserved2;
} DDS_HEADER;

void Texture::load()
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (!in) throw(errno);

  char buf[4];
  in.seekg(0,std::ios::beg);
  in.read(buf, 4);
  if (strncmp(buf, "DDS ", 4))
  {
    std::cout << filename << " isn't a DDS file" << std::endl;
    return;
  } 

  DDS_HEADER header;
  in.read((char*)&header, 124);
  char *fourCC;
  fourCC = (char*)&(header.ddspf.dwFourCC);

  GLenum pixelFormat;
  int bytesPer16Pixels = 16;
  int mipmapCount = (header.dwFlags&0x20000)?header.dwMipMapCount:0;

  if (!strncmp(fourCC, "DXT5", 4))
  {
    bytesPer16Pixels = 16;
    pixelFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
  else
  {
    std::cout << "DDS format not supported for " << filename << std::endl;
    return;
  }
  int *mipmapOffsets = new int[mipmapCount+1];
  mipmapOffsets[0] = 128;
  for (int i=1;i<=mipmapCount;++i)
  {
    int width = header.dwWidth/(1<<(i-1));
    int height = header.dwHeight/(1<<(i-1));
    mipmapOffsets[i] = mipmapOffsets[i-1] + ((width+3)/4)*((height+3)/4)*bytesPer16Pixels;
  }

  mutex.lock();
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  float aniso;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmapCount);
  mutex.unlock();
  
  for (int i=mipmapCount;i>=0;--i)
  {
    int width = header.dwWidth/(1<<i);
    int height = header.dwHeight/(1<<i);
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    int imageSize = ((width+3)/4)*((height+3)/4)*bytesPer16Pixels;

    char *buffer = new char[imageSize];
    in.seekg(mipmapOffsets[i], std::ios::beg);
    in.read(buffer, imageSize);
    mutex.lock();
    glBindTexture(GL_TEXTURE_2D, id);
    glCompressedTexImage2D(GL_TEXTURE_2D, i, pixelFormat, width,height, 0, imageSize, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, i);
    mutex.unlock();
    delete [] buffer;
  }

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