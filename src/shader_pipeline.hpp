#pragma once

#include <vector>
#include <map>
#include <string>

#include "graphics_api.hpp"

class ShaderPipeline
{
public:
	ShaderPipeline() = default;
	explicit ShaderPipeline(GLuint id);
	ShaderPipeline(const ShaderPipeline &) = delete;
	ShaderPipeline(ShaderPipeline &&pipeline);
	ShaderPipeline &operator=(const ShaderPipeline &) = delete;
	ShaderPipeline &operator=(ShaderPipeline &&pipeline);
	~ShaderPipeline();

	void bind();
private:
	GLuint _id = 0;
};

class ShaderFactory
{
public:
	void setVersion(int version);
	void setFolder(const std::string &folder);
	void setSandbox(const std::string &filename);
	ShaderPipeline createPipeline(
		const std::vector<GLenum> &stages,
		const std::vector<std::string> &filenames,
		const std::vector<std::string> &defines = {});

private:
	std::string _versionHeader;
	std::string _folder;
	std::string _sandbox;
	std::map<std::string, std::string> _sourceCache;
};

