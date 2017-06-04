#include "ddsloader.hpp"

#include <string>
#include <fstream>
#include <cstring>
#include <algorithm>

using namespace std;

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

DDSLoader::DDSLoader(int maxSize) : _maxSize(maxSize)
{

}

int getFormatBytesPerBlock(const DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1: return 8;
		case DDSLoader::Format::BC2: return 16;
		case DDSLoader::Format::BC3: return 16;
	}
}

int getSize(const int width, const int height,
	const DDSLoader::Format format)
{
	return max(1, (width+3)/4)*max(1, (height+3)/4)*
	getFormatBytesPerBlock(format);
}

int getMipSize(int origSize, int mipLevel)
{
	return max(1, origSize>>mipLevel);
}

DDSLoader::Format getFourCCFormat(const char* fourCC)
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

bool DDSLoader::open(const string &filename)
{
	_filename = filename;
	ifstream in(_filename.c_str(), ios::in | ios::binary);
	if (!in) return false;

	// Magic number
	in.seekg(0, ios::beg);
	char buf[4];
	in.read(buf, 4);
	if (strncmp(buf, "DDS ", 4))
	{
		return false;
	}

	// DDS header
	DDS_HEADER header;
	in.read((char*)&header, sizeof(DDS_HEADER));
	_format = getFourCCFormat((char*)&(header.ddspf.dwFourCC));

	// Header info
	_width = header.dwWidth;
	_height = header.dwHeight;
	_mipmapCount = (header.dwFlags&0x20000)?header.dwMipMapCount:1;

	// Compute which mipmaps to skip
	const int maxDim = max(_width, _height);
	_skipMipmap = [&]{
		int skipMipmap = 0;
		if (_maxSize != -1)
		{
			for (int i=maxDim;i>_maxSize;i=i/2)
				skipMipmap++;
		}
		return skipMipmap;
	}();

	// Skip mipmaps
	size_t offset = 128;
	// Compute mipmap offsets & sizes
	for (int i=0;i<_mipmapCount;++i)
	{
		int size = getSize(
			getMipSize(_width , i),
			getMipSize(_height, i),
			_format);
		_offsets.push_back(offset);
		_sizes.push_back(size);
		offset += size;
	}
	return true;
}

DDSLoader::Format DDSLoader::getFormat() const
{
	return _format;
}

int DDSLoader::getMipmapCount() const
{
	return _mipmapCount-_skipMipmap;
}

int DDSLoader::getWidth(const int mipmapLevel) const
{
	return getMipSize(_width, _skipMipmap+mipmapLevel);
}

int DDSLoader::getHeight(const int mipmapLevel) const
{
	return getMipSize(_height, _skipMipmap+mipmapLevel);
}

size_t DDSLoader::getImageSize(const int mipmapLevel) const
{
	if (mipmapLevel >= getMipmapCount() ||
		mipmapLevel+_skipMipmap<0)
	{
		throw runtime_error("Mipmap level out of range");
	}

	return _sizes[mipmapLevel+_skipMipmap];
}

vector<uint8_t> DDSLoader::getImageData(const int mipmapLevel) const
{
	vector<uint8_t> data(getImageSize(mipmapLevel));

	ifstream in(_filename.c_str(), ios::in | ios::binary);
	if (!in) throw runtime_error(string("Can't open file ") + _filename);
	in.seekg(_offsets[mipmapLevel+_skipMipmap], ios::beg);
	in.read((char*)data.data(), data.size());

	return data;
}