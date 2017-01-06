#pragma once

#include <string>
#include <fstream>
#include <vector>

std::string read_file(std::string filename);

class DDSLoader
{
public:
	enum Format
	{
		Undefined, BC1, BC2, BC3
	};


	bool open(std::string filename);
	int getMipmapCount();
	int getWidth(int mipmapLevel);
	int getHeight(int mipmapLevel);
	Format getFormat();
	void getImageData(uint32_t mipmapLevel, 
		std::vector<uint8_t> &data);
	void getImageData(uint32_t mipmapLevel, size_t *imageSize, uint8_t *data);

	static void setSkipMipmap(int skipMipmap);

private:
	std::string filename;
	int mipmapCount;
	int width;
	int height;
	Format format;
	std::vector<int> offsets; // Offsets for mipmaps in the file
	std::vector<int> sizes; // Image sizes

	static int skipMipmap;
};