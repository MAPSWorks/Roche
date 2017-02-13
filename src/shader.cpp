#include "shader.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

Shader::Shader(const GLenum type, const std::string filename)
{
	create(type);
	addSource(filename);
}

Shader::Shader(const GLenum type)
{
	create(type);
}

Shader::Shader()
{

}

void Shader::create(GLenum type)
{
	if (type != GL_COMPUTE_SHADER &&
			type != GL_VERTEX_SHADER &&
			type != GL_TESS_CONTROL_SHADER &&
			type != GL_TESS_EVALUATION_SHADER &&
			type != GL_GEOMETRY_SHADER &&
			type != GL_FRAGMENT_SHADER)
	{
		throw std::runtime_error("Invalid shader type");
	}
	this->type = type;
}

void Shader::addSource(const std::string filename)
{
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		this->source = contents;
		this->filename = filename;
	}
	else
	{
		throw std::runtime_error("Can't open " + filename);
	}
}

GLenum Shader::getType() const
{
	return type;
}

std::string Shader::getSource() const
{
	return source;
}

std::string Shader::getFilename() const
{
	return filename;
}

ShaderProgram::ShaderProgram() : id(-1) {}

void ShaderProgram::addShader(const Shader shader)
{
	auto it = shadersByType.find(shader.getType());
	if (it == shadersByType.end())
	{
		shadersByType.insert(std::make_pair(shader.getType(), shader));
	}
	else
	{
		throw std::runtime_error("Can't have more than one shader of one type in a program");
	}
}

void ShaderProgram::addShader(const GLenum type, const std::string filename)
{
	addShader(Shader(type, filename));
}

void ShaderProgram::setConstant(const std::string name, std::string value)
{
	constants.insert(std::make_pair(name, value));
}

void ShaderProgram::compileAndLink()
{
	this->id = glCreateProgram();

	for (auto it : shadersByType)
	{
		const GLenum type = it.first;
		const std::string source = it.second.getSource();
		const std::string filename = it.second.getFilename();

		// Add constant definitions
		size_t firstLine = source.find_first_of("\n")+1;
		std::string newSource = source;
		for (auto c : constants)
		{
			std::string define = "#define " + c.first + " (" + c.second + ")\n";
			newSource.insert(firstLine, define);
			firstLine += define.size();
		}
		newSource.insert(firstLine, "#line 2\n");

		// Compile shader
		GLint success;
		GLchar log[2048];

		std::vector<const char *> sources = {newSource.c_str()};

		GLuint shaderId = glCreateShader(type);
		glShaderSource(shaderId, sources.size(), sources.data(), nullptr);
		glCompileShader(shaderId);
		glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shaderId, sizeof(log), nullptr, log);
			throw std::runtime_error("Can't compile shader " + filename + " : " + log);
		}
		glAttachShader(this->id, shaderId);
		glDeleteShader(shaderId);
	}
	// Link shader program
	GLint success;
	GLchar log[2048];

	glLinkProgram(this->id);
	glGetProgramiv(this->id, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(this->id, sizeof(log), nullptr, log);
		throw std::runtime_error(std::string("Can't link ") + log);
	}
}

GLuint ShaderProgram::getId() { return id; }