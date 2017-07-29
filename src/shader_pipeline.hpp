#pragma once

#include <vector>
#include <map>
#include <string>

#include "graphics_api.hpp"

/**
 * Shader Pipeline generated from ShaderFactory.
 * call bind() to make subsequent draw or dispatch calls use this pipeline.
 */
class ShaderPipeline
{
public:
	/// Empty Pipeline
	ShaderPipeline() = default;
	/// Pipeline from gl object
	explicit ShaderPipeline(GLuint id);
	/// Can't be copied
	ShaderPipeline(const ShaderPipeline &) = delete;
	ShaderPipeline(ShaderPipeline &&pipeline);
	/// Can't be copied
	ShaderPipeline &operator=(const ShaderPipeline &) = delete;
	ShaderPipeline &operator=(ShaderPipeline &&pipeline);
	~ShaderPipeline();

	/// Use pipeline for subsequent draw calls or compute dispatchs
	void bind();
private:
	GLuint _id = 0; /// OpenGL pipeline ID
};

/**
 * Creates Shader Pipelines from source files
 */
class ShaderFactory
{
public:
	ShaderFactory();
	/// Sets GLSL version (default 450)
	void setVersion(int version);
	/// Sets base folder of source files
	void setFolder(const std::string &folder);
	/// Sets sandbox file to be prepended to all source files
	void setSandbox(const std::string &filename);
	/**
	 * Creates Shader Pipeline from source files
	 * @param stageFilenames pairs of stages and source filenames
	 * @param defines constants to be defined in the source files
	 * @return new Shader Pipeline
	 */
	ShaderPipeline createPipeline(
		const std::vector<std::pair<GLenum,std::string>> &stageFilenames,
		const std::vector<std::string> &defines = {});

private:
	/// GLSL version header
	std::string _versionHeader = "";
	/// Base folder
	std::string _folder = ""; 
	/// Sandbox source
	std::string _sandbox = "";
	/// filename->source map for caching
	std::map<std::string, std::string> _sourceCache; 
};

