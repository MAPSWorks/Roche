#pragma once

#include "graphics_api.hpp"
#include <string>
#include <map>

class Shader
{
public:
	/**
	 * @param type one of GL_COMPUTE_SHADER, GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, or GL_FRAGMENT_SHADER
	 */
	Shader(GLenum type, std::string filename);
	Shader(GLenum type);
	Shader();
	void create(GLenum type);
	void addSource(std::string filename);
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
	ShaderProgram();

	void addShader(Shader shader);
	void addShader(GLenum type, std::string filename);
	void setConstant(std::string name, std::string value);
	void compileAndLink();

	GLuint getId();

private:
	GLuint id;
	std::map<std::string, std::string> constants;
	std::map<GLenum, Shader> shadersByType;
};