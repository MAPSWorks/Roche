#pragma once

#include "graphics_api.hpp"
#include <string>
#include <map>

class Shader
{
public:
	/**
	 * @param type one of GL_COMPUTE_SHADER, GL_VERTEX_SHADER, 
	 * GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER,
	 * or GL_FRAGMENT_SHADER
	 */
	Shader() = delete;
	explicit Shader(GLenum type, const std::string &filename="");
	GLenum getType() const;
	std::string getSource() const;
	std::string getFilename() const;

private:
	GLenum type;
	std::string source;
	std::string filename;
};

class ShaderProgram
{
public:
	ShaderProgram() = default;
	ShaderProgram(const ShaderProgram &p);
	ShaderProgram(ShaderProgram &&p);
	ShaderProgram &operator=(const ShaderProgram &p);
	ShaderProgram &operator=(ShaderProgram &&p);
	~ShaderProgram();

	void addShader(const Shader &shader);
	void setConstant(const std::string &name, const std::string &value="");
	void compileAndLink();

	const GLuint &getId() const;

private:
	GLuint id = 0;
	std::map<std::string, std::string> constants;
	std::map<GLenum, Shader> shadersByType;
};