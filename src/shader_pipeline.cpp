#include "shader_pipeline.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

ShaderPipeline::ShaderPipeline(GLuint id)
{
	_id = id;
}

ShaderPipeline::ShaderPipeline(ShaderPipeline &&pipeline)
{
	_id = pipeline._id;
	pipeline._id = 0;
}

ShaderPipeline &ShaderPipeline::operator=(ShaderPipeline &&pipeline)
{
	if (_id && pipeline._id != _id) glDeleteProgramPipelines(1, &_id);
	_id = pipeline._id;
	pipeline._id = 0;
	return *this;
}

ShaderPipeline::~ShaderPipeline()
{
	if (_id) glDeleteProgramPipelines(1, &_id);
}

void ShaderPipeline::bind()
{
	glBindProgramPipeline(_id);
}

void ShaderFactory::setVersion(int version)
{
	_versionHeader = "#version " + std::to_string(version) + " core\n";
}

void ShaderFactory::setFolder(const string &folder)
{
	_folder = folder;
}

pair<bool, string> loadFile(const string &filename)
{
	ifstream in(filename.c_str(), ios::in | ios::binary);
	if (!in) return make_pair(false, "");
	string source;
	in.seekg(0, ios::end);
	source.resize(in.tellg());
	in.seekg(0, ios::beg);
	in.read(&source[0], source.size());
	return make_pair(true, source);
}

string loadSource(const string &folder, const string &filename)
{
	const auto result{loadFile(folder + filename)};
	if (!result.first) throw runtime_error(string("Can't load ") + filename);
	return result.second;
}

void ShaderFactory::setSandbox(const string &filename)
{
	_sandbox = loadSource(_folder, filename);
}

pair<bool, string> checkShaderProgram(const GLuint program)
{
	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	string log;
	int length = 2048;
	log.resize(length);
	glGetProgramInfoLog(program, log.size(), &length, &log[0]);
	log.resize(length);
	return make_pair(success!=0, log);
}

GLuint createShader(const GLenum type, const string &source)
{
	const char *cstr = source.c_str();

	const GLuint program = glCreateShaderProgramv(type, 1, &cstr);
	const auto res = checkShaderProgram(program);
	if (!res.first)
		throw runtime_error(string("Can't create shader : ") + res.second);
	else
	{
		if (!res.second.empty())
			cout << "Warning: " << res.second << endl;
	}

	return program;
}

string formatDefine(string define)
{
	return "#define " + define + "\n";
}

GLbitfield shaderTypeToStage(GLenum type)
{
	switch (type)
	{
		case GL_VERTEX_SHADER : return GL_VERTEX_SHADER_BIT;
		case GL_TESS_CONTROL_SHADER : return GL_TESS_CONTROL_SHADER_BIT;
		case GL_TESS_EVALUATION_SHADER : return GL_TESS_EVALUATION_SHADER_BIT;
		case GL_GEOMETRY_SHADER : return GL_GEOMETRY_SHADER_BIT;
		case GL_FRAGMENT_SHADER : return GL_FRAGMENT_SHADER_BIT;
		case GL_COMPUTE_SHADER : return GL_COMPUTE_SHADER_BIT;
	}
	return GL_ALL_SHADER_BITS;
}

ShaderPipeline ShaderFactory::createPipeline(
	const vector<GLenum> &stages,
	const vector<string> &filenames,
	const vector<string> &defines)
{
	// Create Pipeline
	GLuint pipelineId;
	glCreateProgramPipelines(1, &pipelineId);

	// Defines
	std::string definesStr = "";
	for_each(defines.begin(), defines.end(), [&](const string &d){
		definesStr += formatDefine(d);
	});

	const std::string preSource = _versionHeader + definesStr + _sandbox;

	// Load sources from filenames
	for (int i=0;i<stages.size();++i)
	{
		const string filename = filenames[i];
		const auto it = _sourceCache.find(filename);
		string source;
		if (it == _sourceCache.end())
		{
			// Loading
			source = loadSource(_folder, filename);
			// Caching
			_sourceCache[filename] = source;
		}
		else
		{
			// Load from cache
			source = it->second;
		}

		const GLenum type = stages[i];
		const string finalSource = preSource + source;
		GLuint shadId = createShader(type, finalSource);

		glUseProgramStages(pipelineId, shaderTypeToStage(stages[i]), shadId);
	}
	return ShaderPipeline(pipelineId);
}