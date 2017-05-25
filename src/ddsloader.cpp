#include "ddsloader.hpp"

#include <string>
#include <fstream>
#include <cstring>
#include <algorithm>

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

void DDSLoader::setSkipMipmap(const int skipMipmap)
{
	DDSLoader::skipMipmap = std::max(0, skipMipmap);
}

DDSLoader::Format DDSLoader::getFormat()
{
	return format;
}

int getFormatBytesPerBlock(DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1: return 8;
		case DDSLoader::Format::BC2: return 16;
		case DDSLoader::Format::BC3: return 16;
	}
}

int getImageSize(const int width, const int height, DDSLoader::Format format)
{
	return std::max(1, (width+3)/4)*std::max(1, (height+3)/4)*getFormatBytesPerBlock(format);
}

DDSLoader::Format getFourCCFormat(char* fourCC)
{
	if (!strncmp(fourCC, "DXT1", 4))
	{
		return DDSLoader::Format::BC1;
	}
	if (!strncmp(fourCC, "DXT3", 4))
	{
		return DDSLoader::Format::BC2;
	}
	if (!strncmp(fourCC, "DXT5", 4))
	{
		return DDSLoader::Format::BC3;
	}
	return DDSLoader::Format::Undefined;
}

bool DDSLoader::open(const std::string filename)
{
	this->filename = filename;
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
	if (!in) return false;

	// Magic number
	in.seekg(0, std::ios::beg);
	char buf[4];
	in.read(buf, 4);
	if (strncmp(buf, "DDS ", 4))
	{
		return false;
	}

	// DDS header
	DDS_HEADER header;
	in.read((char*)&header, sizeof(DDS_HEADER));
	format = getFourCCFormat((char*)&(header.ddspf.dwFourCC));

	// Mipmap count check
	mipmapCount = (header.dwFlags&0x20000)?header.dwMipMapCount:1;

	width = header.dwWidth;
	height = header.dwHeight;

	size_t offset = 128;
	for (int i=0;i<skipMipmap;++i)
	{
		offset += getImageSize(getWidth(i), getHeight(i), format);
	}

	for (int i=skipMipmap;i<mipmapCount;++i)
	{
		int size = getImageSize(getWidth(i), getHeight(i), format);
		offsets.push_back(offset);
		sizes.push_back(size);
		offset += size;
	}

	width = width >> skipMipmap;
	height = height >> skipMipmap;
	mipmapCount -= skipMipmap;

	return true;
}

int DDSLoader::getMipmapCount()
{
	return mipmapCount;
}

int DDSLoader::getWidth(const int mipmapLevel)
{
	return std::max(1, width>>mipmapLevel);
}

int DDSLoader::getHeight(const int mipmapLevel)
{
	return std::max(1, height>>mipmapLevel);
}

void DDSLoader::getImageData(int mipmapLevel, int mipmapCount, size_t *imageSize, uint8_t *data)
{
	if (mipmapLevel+mipmapCount-1 >= getMipmapCount()) return;
	else if (mipmapLevel < 0) mipmapLevel = 0;

	// Offset into file to get pixel data
	int size = 0;
	for (int i=mipmapLevel;i<mipmapLevel+mipmapCount;++i) size += sizes[i];
	if (imageSize) *imageSize = size;

	if (data)
	{
		std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
		if (!in) return;
		const int offset = offsets[mipmapLevel];
		in.seekg(offset, std::ios::beg);
		in.read((char*)data, size);
	}
}