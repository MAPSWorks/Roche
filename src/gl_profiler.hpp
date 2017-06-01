#pragma once

#include "graphics_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <utility>

class GPUProfilerGL
{
public:
	GPUProfilerGL() = default;
	~GPUProfilerGL();
	void begin(std::string name);
	void end();
	std::vector<std::pair<std::string,uint64_t>> get();
private:
	std::map<std::string, std::pair<GLuint, GLuint>> queries[2];
	std::stack<std::string> names;
	std::vector<std::string> orderedNames[2];
	int bufferId = 0;
};