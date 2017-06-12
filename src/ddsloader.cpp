#include "ddsloader.hpp"

#include <string>
#include <fstream>
#include <cstring>
#include <string>
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

typedef uint32_t UINT;

enum DXGI_FORMAT { 
  DXGI_FORMAT_UNKNOWN                     = 0,
  DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
  DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
  DXGI_FORMAT_R32G32B32A32_UINT           = 3,
  DXGI_FORMAT_R32G32B32A32_SINT           = 4,
  DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
  DXGI_FORMAT_R32G32B32_FLOAT             = 6,
  DXGI_FORMAT_R32G32B32_UINT              = 7,
  DXGI_FORMAT_R32G32B32_SINT              = 8,
  DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
  DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
  DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
  DXGI_FORMAT_R16G16B16A16_UINT           = 12,
  DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
  DXGI_FORMAT_R16G16B16A16_SINT           = 14,
  DXGI_FORMAT_R32G32_TYPELESS             = 15,
  DXGI_FORMAT_R32G32_FLOAT                = 16,
  DXGI_FORMAT_R32G32_UINT                 = 17,
  DXGI_FORMAT_R32G32_SINT                 = 18,
  DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
  DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
  DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
  DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
  DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
  DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
  DXGI_FORMAT_R10G10B10A2_UINT            = 25,
  DXGI_FORMAT_R11G11B10_FLOAT             = 26,
  DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
  DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
  DXGI_FORMAT_R8G8B8A8_UINT               = 30,
  DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
  DXGI_FORMAT_R8G8B8A8_SINT               = 32,
  DXGI_FORMAT_R16G16_TYPELESS             = 33,
  DXGI_FORMAT_R16G16_FLOAT                = 34,
  DXGI_FORMAT_R16G16_UNORM                = 35,
  DXGI_FORMAT_R16G16_UINT                 = 36,
  DXGI_FORMAT_R16G16_SNORM                = 37,
  DXGI_FORMAT_R16G16_SINT                 = 38,
  DXGI_FORMAT_R32_TYPELESS                = 39,
  DXGI_FORMAT_D32_FLOAT                   = 40,
  DXGI_FORMAT_R32_FLOAT                   = 41,
  DXGI_FORMAT_R32_UINT                    = 42,
  DXGI_FORMAT_R32_SINT                    = 43,
  DXGI_FORMAT_R24G8_TYPELESS              = 44,
  DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
  DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
  DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
  DXGI_FORMAT_R8G8_TYPELESS               = 48,
  DXGI_FORMAT_R8G8_UNORM                  = 49,
  DXGI_FORMAT_R8G8_UINT                   = 50,
  DXGI_FORMAT_R8G8_SNORM                  = 51,
  DXGI_FORMAT_R8G8_SINT                   = 52,
  DXGI_FORMAT_R16_TYPELESS                = 53,
  DXGI_FORMAT_R16_FLOAT                   = 54,
  DXGI_FORMAT_D16_UNORM                   = 55,
  DXGI_FORMAT_R16_UNORM                   = 56,
  DXGI_FORMAT_R16_UINT                    = 57,
  DXGI_FORMAT_R16_SNORM                   = 58,
  DXGI_FORMAT_R16_SINT                    = 59,
  DXGI_FORMAT_R8_TYPELESS                 = 60,
  DXGI_FORMAT_R8_UNORM                    = 61,
  DXGI_FORMAT_R8_UINT                     = 62,
  DXGI_FORMAT_R8_SNORM                    = 63,
  DXGI_FORMAT_R8_SINT                     = 64,
  DXGI_FORMAT_A8_UNORM                    = 65,
  DXGI_FORMAT_R1_UNORM                    = 66,
  DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
  DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
  DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
  DXGI_FORMAT_BC1_TYPELESS                = 70,
  DXGI_FORMAT_BC1_UNORM                   = 71,
  DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
  DXGI_FORMAT_BC2_TYPELESS                = 73,
  DXGI_FORMAT_BC2_UNORM                   = 74,
  DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
  DXGI_FORMAT_BC3_TYPELESS                = 76,
  DXGI_FORMAT_BC3_UNORM                   = 77,
  DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
  DXGI_FORMAT_BC4_TYPELESS                = 79,
  DXGI_FORMAT_BC4_UNORM                   = 80,
  DXGI_FORMAT_BC4_SNORM                   = 81,
  DXGI_FORMAT_BC5_TYPELESS                = 82,
  DXGI_FORMAT_BC5_UNORM                   = 83,
  DXGI_FORMAT_BC5_SNORM                   = 84,
  DXGI_FORMAT_B5G6R5_UNORM                = 85,
  DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
  DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
  DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
  DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
  DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
  DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
  DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
  DXGI_FORMAT_BC6H_TYPELESS               = 94,
  DXGI_FORMAT_BC6H_UF16                   = 95,
  DXGI_FORMAT_BC6H_SF16                   = 96,
  DXGI_FORMAT_BC7_TYPELESS                = 97,
  DXGI_FORMAT_BC7_UNORM                   = 98,
  DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
  DXGI_FORMAT_AYUV                        = 100,
  DXGI_FORMAT_Y410                        = 101,
  DXGI_FORMAT_Y416                        = 102,
  DXGI_FORMAT_NV12                        = 103,
  DXGI_FORMAT_P010                        = 104,
  DXGI_FORMAT_P016                        = 105,
  DXGI_FORMAT_420_OPAQUE                  = 106,
  DXGI_FORMAT_YUY2                        = 107,
  DXGI_FORMAT_Y210                        = 108,
  DXGI_FORMAT_Y216                        = 109,
  DXGI_FORMAT_NV11                        = 110,
  DXGI_FORMAT_AI44                        = 111,
  DXGI_FORMAT_IA44                        = 112,
  DXGI_FORMAT_P8                          = 113,
  DXGI_FORMAT_A8P8                        = 114,
  DXGI_FORMAT_B4G4R4A4_UNORM              = 115,
  DXGI_FORMAT_P208                        = 130,
  DXGI_FORMAT_V208                        = 131,
  DXGI_FORMAT_V408                        = 132,
  DXGI_FORMAT_FORCE_UINT                  = 0xffffffff
};

enum D3D10_RESOURCE_DIMENSION { 
  D3D10_RESOURCE_DIMENSION_UNKNOWN    = 0,
  D3D10_RESOURCE_DIMENSION_BUFFER     = 1,
  D3D10_RESOURCE_DIMENSION_TEXTURE1D  = 2,
  D3D10_RESOURCE_DIMENSION_TEXTURE2D  = 3,
  D3D10_RESOURCE_DIMENSION_TEXTURE3D  = 4
};

struct DDS_HEADER_DXT10 {
	DXGI_FORMAT              dxgiFormat;
	D3D10_RESOURCE_DIMENSION resourceDimension;
	UINT                     miscFlag;
	UINT                     arraySize;
	UINT                     miscFlags2;
};

DDSLoader::DDSLoader(int maxSize) : _maxSize(maxSize)
{

}

int getFormatBytesPerBlock(const DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1 : 
		case DDSLoader::Format::BC1_SRGB : 
		return 8;
		case DDSLoader::Format::BC2 : 
		case DDSLoader::Format::BC2_SRGB : 
		return 16;
		case DDSLoader::Format::BC3 : 
		case DDSLoader::Format::BC3_SRGB : 
		return 16;
		case DDSLoader::Format::BC4 : 
		case DDSLoader::Format::BC4_SIGNED : 
		return 8;
		case DDSLoader::Format::BC5 : 
		case DDSLoader::Format::BC5_SIGNED : 
		return 16;
		case DDSLoader::Format::BC6 : 
		case DDSLoader::Format::BC6_SIGNED : 
		return 16;
		case DDSLoader::Format::BC7 : 
		case DDSLoader::Format::BC7_SRGB : 
		return 16;
	}
	return 0;
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

DDSLoader::Format getDDSFormat(const DDS_HEADER &header)
{
	string fourCC = string((const char*)&header.ddspf.dwFourCC, 4);
	if (fourCC == "DXT1")
	{
		return DDSLoader::Format::BC1_SRGB;
	}
	if (fourCC == "DXT2" || fourCC == "DXT3")
	{
		return DDSLoader::Format::BC2_SRGB;
	}
	if (fourCC == "DXT4" || fourCC == "DXT5")
	{
		return DDSLoader::Format::BC3_SRGB;
	}
	return DDSLoader::Format::Undefined;
}

DDSLoader::Format getDX10Format(const DDS_HEADER_DXT10 &header)
{
	switch (header.dxgiFormat)
	{
		case   DXGI_FORMAT_BC1_TYPELESS                :
		case   DXGI_FORMAT_BC1_UNORM                   :
			return DDSLoader::Format::BC1;
		case   DXGI_FORMAT_BC1_UNORM_SRGB              :
			return DDSLoader::Format::BC1_SRGB;
		case   DXGI_FORMAT_BC2_TYPELESS                :
		case   DXGI_FORMAT_BC2_UNORM                   :
			return DDSLoader::Format::BC2;
		case   DXGI_FORMAT_BC2_UNORM_SRGB              :
			return DDSLoader::Format::BC2;
		case   DXGI_FORMAT_BC3_TYPELESS                :
		case   DXGI_FORMAT_BC3_UNORM                   :
			return DDSLoader::Format::BC3;
		case   DXGI_FORMAT_BC3_UNORM_SRGB              :
			return DDSLoader::Format::BC3_SRGB;			
		case   DXGI_FORMAT_BC4_TYPELESS                :
		case   DXGI_FORMAT_BC4_UNORM                   :
			return DDSLoader::Format::BC4;
		case   DXGI_FORMAT_BC4_SNORM                   :
			return DDSLoader::Format::BC4_SIGNED;
		case   DXGI_FORMAT_BC5_TYPELESS                :
		case   DXGI_FORMAT_BC5_UNORM                   :
			return DDSLoader::Format::BC5;
		case   DXGI_FORMAT_BC5_SNORM                   :
			return DDSLoader::Format::BC5_SIGNED;
		case   DXGI_FORMAT_BC6H_TYPELESS               :
		case   DXGI_FORMAT_BC6H_UF16                   :
			return DDSLoader::Format::BC6;
		case   DXGI_FORMAT_BC6H_SF16                   :
			return DDSLoader::Format::BC6_SIGNED;
		case   DXGI_FORMAT_BC7_TYPELESS                :
		case   DXGI_FORMAT_BC7_UNORM                   :
			return DDSLoader::Format::BC7;
		case   DXGI_FORMAT_BC7_UNORM_SRGB              :
			return DDSLoader::Format::BC7_SRGB;
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
	bool hasDX10Header = false;
	DDS_HEADER header;
	in.read((char*)&header, sizeof(DDS_HEADER));
	if (!strncmp((const char*)&header.ddspf.dwFourCC, "DX10", 4))
	{
		hasDX10Header = true;
		DDS_HEADER_DXT10 dx10Header;
		in.read((char*)&header + sizeof(DDS_HEADER), sizeof(DDS_HEADER_DXT10));
		_format = getDX10Format(dx10Header);
	}
	else
	{
		_format = getDDSFormat(header);
	}
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
	size_t offset = 128+(hasDX10Header?sizeof(DDS_HEADER_DXT10):0);
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