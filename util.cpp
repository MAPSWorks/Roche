#include "util.h"

#include <string>
#include <fstream>
#include "opengl.h"
#include <iostream>
#include <cstring>

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

void load_DDS(
  const std::string &filename,
  Texture *tex,concurrent_queue<TexMipmapData> &tmd)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (!in)
  {
    std::cout << "Can't open file " << filename << std::endl;
    throw(errno);
  }

  // Reads the first 4 bytes and checks the dds file signature
  char buf[4];
  in.seekg(0,std::ios::beg);
  in.read(buf, 4);
  if (strncmp(buf, "DDS ", 4))
  {
    std::cout << filename << " isn't a DDS file" << std::endl;
    return;
  } 

  // Loads the DDS header
  DDS_HEADER header;
  in.read((char*)&header, 124);
  // Extracts the DXT format
  char *fourCC;
  fourCC = (char*)&(header.ddspf.dwFourCC);

  GLenum pixelFormat; // format used in the glteximage call
  int bytesPer16Pixels = 16; // number of bytes for each block of pixels (4x4)
  int mipmapCount = (header.dwFlags&0x20000)?header.dwMipMapCount:1; // number of images in the file

  // Selects the proper pixel format and block size
  if (!strncmp(fourCC, "DXT5", 4))
  {
    bytesPer16Pixels = 16;
    pixelFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
  else // only dxt5 is supported yet
  {
    std::cout << "DDS format not supported for " << filename << std::endl;
    return;
  }
  // mipmapOffsets[i] is the offset from the beginning of the file where the data of level i is
  int *mipmapOffsets = new int[mipmapCount];
  mipmapOffsets[0] = 128; // first level is just after header
  for (int i=1;i<mipmapCount;++i)
  {
    int width = header.dwWidth>>(i-1); // level is half the width & height of the previous one
    int height = header.dwHeight>>(i-1);
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    // offset = previous offset + previous size
    mipmapOffsets[i] = mipmapOffsets[i-1] + std::max(1,(width+3)/4)*std::max(1,(height+3)/4)*bytesPer16Pixels;
  }
  // Iterates through the mipmap levels from the smallest to the largest
  for (int i=mipmapCount-1;i>=0;--i)
  {
    int width = header.dwWidth>>i;
    int height = header.dwHeight>>i;
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    int imageSize = std::max(1,(width+3)/4)*std::max(1,(height+3)/4)*bytesPer16Pixels;

    // Reads data from file to buffer
    char *buffer = new char[imageSize];
    in.seekg(mipmapOffsets[i], std::ios::beg);
    in.read(buffer, imageSize);

    // Updates texture
    tmd.push(TexMipmapData(true, tex, i, pixelFormat, width, height, imageSize, buffer));
  }
  delete [] mipmapOffsets;
  in.close();
}

