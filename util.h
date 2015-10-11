#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <deque>
#include <mutex>
#include "opengl.h"

std::string read_file(const std::string &filename);
void load_DDS(
	const std::string &filename,
	Texture *tex,std::deque<TexMipmapData> *tmd,
	std::mutex *ttum);

#endif