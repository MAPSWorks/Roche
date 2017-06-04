#pragma once

#include <string>
#include <vector>

class DDSLoader
{
public:
	enum class Format
	{
		Undefined, BC1, BC2, BC3
	};

	explicit DDSLoader(int maxSize=-1);
	bool open(const std::string &filename);
	int getMipmapCount() const;
	int getWidth(int mipmapLevel) const;
	int getHeight(int mipmapLevel) const;
	Format getFormat() const;
	void getImageData(int mipmapLevel, 
		size_t *imageSize, uint8_t *data) const;

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