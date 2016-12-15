#include "util.h"

#include <string>
#include <fstream>
#include <cstring>

std::string read_file(std::string filename)
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
	throw std::runtime_error("Can't open" + filename);
	return "";
}

typedef uint32_t DWORD;

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

struct DDS_HEADER {
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
};

int DDSLoader::skipMipmap = 0;

void DDSLoader::setSkipMipmap(int skipMipmap)
{
	DDSLoader::skipMipmap = std::max(0, skipMipmap);
}

DDSLoader::DDSLoader(std::string filename)
{
	in.open(filename.c_str(), std::ios::in | std::ios::binary);
	if (!in)
	{
		throw std::runtime_error("Can't open file" + filename);
	}

	// Magic number
	char buf[4];
	in.seekg(0, std::ios::beg);
	in.read(buf, 4);
	if (strncmp(buf, "DDS ", 4))
	{
		throw std::runtime_error("Not a DDS file");
	}

	// DDS header
	DDS_HEADER header;
	in.read((char*)&header, sizeof(DDS_HEADER));
	char *fourCC;
	fourCC = (char*)&(header.ddspf.dwFourCC);

	if (strncmp(fourCC, "DXT5", 4))
	{
		throw std::runtime_error("Format not supported");
	}

	// Mipmap count check
	mipmapCount = (header.dwFlags&0x20000)?header.dwMipMapCount:1;
	width = header.dwWidth;
	height = header.dwHeight;
}

int DDSLoader::getMipmapCount()
{
	return std::max(1, mipmapCount-skipMipmap);
}

int DDSLoader::getWidth(int mipmapLevel)
{
	return std::max(1, width>>(mipmapLevel+skipMipmap));
}

int DDSLoader::getHeight(int mipmapLevel)
{
	return std::max(1, height>>(mipmapLevel+skipMipmap));
}

void DDSLoader::getImageData(uint32_t mipmapLevel, std::vector<uint8_t> &data)
{
	if (mipmapLevel >= getMipmapCount()) return;
	else if (mipmapLevel < 0) mipmapLevel = 0;

	// Offset into file to get pixel data
	size_t offset = 128;
	for (int i=0;i<mipmapLevel+skipMipmap;++i)
	{
		int mipmapWidth  = std::max(1, (int)(width  >>i));
		int mipmapHeight = std::max(1, (int)(height >>i));
		offset += std::max(1, (mipmapWidth+3)/4)*std::max(1, (mipmapHeight+3)/4)*16;
	}

	int mipmapWidth  = getWidth(mipmapLevel);
	int mipmapHeight = getHeight(mipmapLevel);
	size_t imageSize = std::max(1, (mipmapWidth+3)/4)*std::max(1, (mipmapHeight+3)/4)*16;

	data.resize(imageSize);
	in.seekg(offset, std::ios::beg);
	in.read((char*)data.data(), imageSize);
}