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
	void getImageData(int mipmapLevel, int mipmapCount, 
		size_t *imageSize, uint8_t *data) const;

private:
	std::string filename = "";
	int mipmapCount = 0;
	int width = 0;
	int height = 0;
	int maxSize;
	Format format = Format::Undefined;
	std::vector<int> offsets; // Offsets for mipmaps in the file
	std::vector<int> sizes; // Image sizes
};