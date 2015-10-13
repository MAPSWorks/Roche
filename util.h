#ifndef UTIL_H
#define UTIL_H

#include <string>
#include "opengl.h"
#include "concurrent_queue.h"

std::string read_file(const std::string &filename);
void load_DDS(
	const std::string &filename,
	Texture &tex,concurrent_queue<TexMipmapData> &tmd);

#endif