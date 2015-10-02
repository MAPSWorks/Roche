#include "util.h"

#include <string>
#include <fstream>
#include <iostream>
#include <cstring>

#include <GL/glew.h>
#include "opengl.h"

std::string read_file(const std::string &filename)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
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

void load_DDS(const std::string &filename, Texture &tex)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (!in) throw(errno);

  char buf[4];
  in.seekg(0,std::ios::beg);
  in.read(buf, 4);
  if (strcmp(buf, "DDS "))
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

  GLuint id = tex.getId();
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  float aniso;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmapCount);
  
  for (int i=mipmapCount;i>=0;--i)
  {
    int width = header.dwWidth/(1<<i);
    int height = header.dwHeight/(1<<i);
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    int imageSize = ((width+3)/4)*((height+3)/4)*bytesPer16Pixels;
    //std::cout << "width=" << width << ";height=" << height << ";size=" << imageSize << std::endl;

    char *buffer = new char[imageSize];
    in.seekg(mipmapOffsets[i], std::ios::beg);
    in.read(buffer, imageSize);
    glBindTexture(GL_TEXTURE_2D, id);
    glCompressedTexImage2D(GL_TEXTURE_2D, i, pixelFormat, width,height, 0, imageSize, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, i);
    delete [] buffer;
  }

}