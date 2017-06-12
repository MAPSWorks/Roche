#pragma once

#include <string>
#include <vector>

class DDSLoader
{
public:
	enum class Format
	{
		Undefined, 
		BC1, BC1_SRGB, 
		BC2, BC2_SRGB,
		BC3, BC3_SRGB,
		BC4, BC4_SIGNED,
		BC5, BC5_SIGNED,
		BC6, BC6_SIGNED,
		BC7, BC7_SRGB
	};

	explicit DDSLoader(int maxSize=-1);
	bool open(const std::string &filename);
	int getMipmapCount() const;
	int getWidth(int mipmapLevel) const;
	int getHeight(int mipmapLevel) const;
	Format getFormat() const;
	size_t getImageSize(int mipmapLevel) const;
	std::vector<uint8_t> getImageData(int mipmapLevel) const;

private:
	std::string _filename = "";
	int _mipmapCount = 0;
	int _width = 0;
	int _height = 0;
	int _maxSize = -1;
	int _skipMipmap = 0;
	Format _format = Format::Undefined;
	std::vector<int> _offsets; // Offsets for mipmaps in the file
	std::vector<int> _sizes; // Image sizes
};