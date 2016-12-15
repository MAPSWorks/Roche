#pragma once

#include <string>
#include <fstream>
#include <vector>

std::string read_file(std::string filename);

class DDSLoader
{
public:
	DDSLoader(std::string filename);
	int getMipmapCount();
	int getWidth(int mipmapLevel);
	int getHeight(int mipmapLevel);
	void getImageData(uint32_t mipmapLevel, 
		std::vector<uint8_t> &data);

	static void setSkipMipmap(int skipMipmap);

private:
	std::ifstream in;
	int mipmapCount;
	int width;
	int height;

	static int skipMipmap;
};