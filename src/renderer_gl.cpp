#include "renderer_gl.hpp"
#include "ddsloader.hpp"
#include "flare.hpp"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb_image_write.h"

using namespace glm;
using namespace std;

void RendererGL::windowHints()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
}

struct Vertex
{
	vec4 position;
	vec4 uv;
	vec4 normal;
	vec4 tangent;
};

typedef uint32_t Index;

GLenum indexType()
{
	switch (sizeof(Index))
	{
		case 2: return GL_UNSIGNED_SHORT;
		case 1: return GL_UNSIGNED_BYTE;
		default: return GL_UNSIGNED_INT;
	}
}

void generateSphere(
	const int meridians, 
	const int rings, 
	const bool exterior, 
	vector<Vertex> &vertices, 
	vector<Index> &indices)
{
	// Vertices
	vertices.resize((meridians+1)*(rings+1));
	size_t offset = 0;
	for (int i=0;i<=rings;++i)
	{
		const float phi = glm::pi<float>()*((float)i/(float)rings-0.5);
		const float cp = cos(phi);
		const float sp = sin(phi);
		for (int j=0;j<=meridians;++j)
		{
			const float theta = 2*glm::pi<float>()*((float)j/(float)meridians);
			const float ct = cos(theta);
			const float st = sin(theta);
			vec3 pos = vec3(cp*ct,cp*st,sp);
			vec3 normal = normalize(pos);
			vec3 tangent = cross(normal, vec3(0,0,1));
			vertices[offset] = {
				vec4(pos,1), 
				vec4((float)j/(float)meridians, 1.f-(float)i/(float)rings,0.0,0.0),
				vec4(normal,0),
				vec4(tangent,0)
			};
			offset++;
		}
	}

	// Indices
	indices.resize(meridians*rings*6);
	offset = 0;
	for (int i=0;i<rings;++i)
	{
		for (int j=0;j<meridians;++j)
		{
			const Index i1 = i+1;
			const Index j1 = j+1;
			vector<Index> ind = {
				(Index)(i *(rings+1)+j),
				(Index)(i1*(rings+1)+j),
				(Index)(i1*(rings+1)+j1),
				(Index)(i1*(rings+1)+j1),
				(Index)(i *(rings+1)+j1),
				(Index)(i *(rings+1)+j)
			};
			// Remove back face culling
			if (!exterior)
				swap(ind[1], ind[4]);

			// Copy to indices
			memcpy(&indices[offset*6], ind.data(), ind.size()*sizeof(Index));
			offset++;
		}
	}
}

void generateFullscreenTri(vector<Vertex> &vertices, vector<Index> &indices)
{
	vertices.resize(3);
	vertices[0].position = vec4(-2,-1,0,1);
	vertices[1].position = vec4( 2,-1,0,1);
	vertices[2].position = vec4( 0, 4,0,1);

	indices = {0,1,2};
}

void generateFlareModel(vector<Vertex> &vertices, vector<Index> &indices)
{
	const int detail = 32;
	vertices.resize((detail+1)*2);

	for (int i=0;i<=detail;++i)
	{
		const float f = i/(float)detail;
		const vec2 pos = vec2(cos(f*2*glm::pi<float>()),sin(f*2*glm::pi<float>()));
		vertices[i*2+0] = {vec4(0,0,0,1), vec4(f, 0, 0.5, 0.5)};
		vertices[i*2+1] = {vec4(pos,0,1), vec4(f, 1, pos*vec2(0.5)+vec2(0.5))};
	}

	indices.resize(detail*6);
	for (int i=0;i<detail;++i)
	{
		indices[i*6+0] = (i*2)+0;
		indices[i*6+1] = (i*2)+1;
		indices[i*6+2] = (i*2)+2;
		indices[i*6+3] = (i*2)+2;
		indices[i*6+4] = (i*2)+1;
		indices[i*6+5] = (i*2)+3;
	}
}

void generateRingModel(
	const int meridians,
	const float near,
	const float far,
	vector<Vertex> &vertices, 
	vector<Index> &indices)
{
	vertices.resize((meridians+1)*2);
	indices.resize(meridians*6);

	{
		int offset = 0;
		for (int i=0;i<=meridians;++i)
		{
			float angle = (glm::pi<float>()*i)/(float)meridians;
			vec2 pos = vec2(cos(angle),sin(angle));
			vertices[offset+0] = {vec4(pos*near, 0.0,1.0), vec4(pos*1.f,0.0,0.0)};
			vertices[offset+1] = {vec4(pos*far , 0.0,1.0), vec4(pos*2.f,0.0,0.0)};
			offset += 2;
		}
	}

	{
		int offset = 0;
		Index vert = 0;
		for (int i=0;i<meridians;++i)
		{
			indices[offset+0] = vert+2;
			indices[offset+1] = vert+0;
			indices[offset+2] = vert+1;
			indices[offset+3] = vert+2;
			indices[offset+4] = vert+1;
			indices[offset+5] = vert+3;
			offset += 6;
			vert += 2; 
		}
	}
}

GLenum DDSFormatToGL(DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
		case DDSLoader::Format::BC2: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
		case DDSLoader::Format::BC3: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
	}
	return 0;
}

template <class V, class I>
vector<DrawCommand> getCommands(
	GLuint vao, GLenum mode, GLenum indexType,
	Buffer &vertexBuffer,
	Buffer &indexBuffer,
	const vector<vector<V>> vertices, 
	const vector<vector<I>> indices)
{
	vector<DrawCommand> commands(vertices.size());
	vector<pair<BufferRange, BufferRange>> ranges(vertices.size());

	for (int i=0;i<vertices.size();++i)
	{
		ranges[i] = make_pair(
			vertexBuffer.assignVertices(vertices[i].size(), sizeof(V)),
			indexBuffer.assignIndices(indices[i].size(), sizeof(I)));
		commands[i] = DrawCommand(
			vao, mode, indexType,
			sizeof(V), sizeof(I),
			ranges[i].first,
			ranges[i].second);
	}

	vertexBuffer.validate();
	indexBuffer.validate();

	for (int i=0;i<vertices.size();++i)
	{
		vertexBuffer.write(ranges[i].first, vertices[i].data());
		indexBuffer.write(ranges[i].second, indices[i].data());
	}

	return commands;
}

void RendererGL::init(
	const vector<Planet> planetParams,
	const int msaa,
	const int maxTexSize,
	const int windowWidth,
	const int windowHeight)
{
	this->planetParams = planetParams;
	this->planetCount = planetParams.size();
	this->msaaSamples = msaa;
	this->maxTexSize = maxTexSize;
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	this->bufferFrames = 3; // triple-buffering

	this->planetData.resize(planetCount);
	this->fences.resize(bufferFrames);

	vertexBuffer = Buffer(
		Buffer::Usage::STATIC,
		Buffer::Access::WRITE_ONLY);

	indexBuffer = Buffer(
		Buffer::Usage::STATIC, 
		Buffer::Access::WRITE_ONLY);

	createVertexArray();

	// Static vertex & index data
	vector<vector<Vertex>> vertices(3);
	vector<vector<Index>> indices(3);

	// Fullscreen tri
	generateFullscreenTri(vertices[0], indices[0]);

	// Flare
	generateFlareModel(vertices[1], indices[1]);

	// Sphere
	const int planetMeridians = 128;
	const int planetRings = 128;
	generateSphere(planetMeridians, planetRings, false, 
		vertices[2], indices[2]);

	// Load custom models
	vector<int> planetModelId(planetCount, 2);
	vector<int> ringModelId(planetCount, -1);
	for (uint32_t i=0;i<planetCount;++i)
	{
		const Planet param = planetParams[i];
		// Rings
		if (param.hasRing())
		{
			ringModelId[i] = vertices.size();
			vertices.emplace_back();
			indices.emplace_back();
			float near = param.getRing().getInnerDistance();
			float far = param.getRing().getOuterDistance();
			const int ringMeridians = 128;
			generateRingModel(ringMeridians, near, far, 
				vertices.back(), indices.back());
		}
	}

	vector<DrawCommand> commands = getCommands(
		vertexArray, GL_TRIANGLES, indexType(),
		vertexBuffer,
		indexBuffer,
		vertices,
		indices);

	fullscreenTri = commands[0];
	flareModel    = commands[1];
	sphere        = commands[2];

	for (uint32_t i=0;i<planetCount;++i)
	{
		auto &data = planetData[i];
		data.planetModel = commands[planetModelId[i]];
		data.ringModel   = commands[ringModelId[i]];
	}

	// Dynamic UBO buffer assigning
	uboBuffer = Buffer(
		Buffer::Usage::DYNAMIC, 
		Buffer::Access::WRITE_ONLY);

	dynamicData.resize(bufferFrames); // multiple buffering
	for (auto &data : dynamicData)
	{
		// Scene UBO
		data.sceneUBO = uboBuffer.assignUBO(sizeof(SceneDynamicUBO));
		// Planet UBOs
		data.planetUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			data.planetUBOs[j] = uboBuffer.assignUBO(sizeof(PlanetDynamicUBO));
		}
		// Flare UBOs
		data.flareUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			data.flareUBOs[j] = uboBuffer.assignUBO(sizeof(FlareDynamicUBO));
		}
	}

	uboBuffer.validate();

	createShaders();
	createRendertargets();
	createTextures();
	createScreenshot();
	initStreamTexThread();
	initScreenshotThread();
}

void RendererGL::createTextures()
{
	// Anisotropy
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

	const float requestedAnisotropy = 16.f;
	textureAnisotropy = (requestedAnisotropy > maxAnisotropy)?maxAnisotropy:requestedAnisotropy;

	// Default diffuse tex
	const uint8_t diffuseData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &diffuseTexDefault);
	glTextureStorage2D(diffuseTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(diffuseTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, diffuseData);
	
	// Default cloud tex
	const uint8_t cloudData[] = {255, 255, 255, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &cloudTexDefault);
	glTextureStorage2D(cloudTexDefault, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(cloudTexDefault, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cloudData);

	// Default night tex
	const uint8_t nightData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &nightTexDefault);
	glTextureStorage2D(nightTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(nightTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, nightData);

	createFlare();

	// Samplers
	glCreateSamplers(1, &planetTexSampler);
	glSamplerParameterf(planetTexSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glCreateSamplers(1, &atmoSampler);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glCreateSamplers(1, &ringSampler);
	glSamplerParameteri(ringSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(ringSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(ringSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
}

int mipmapCount(int size)
{
	return 1 +std::floor(std::log2(size));
}

void RendererGL::createFlare()
{
	const int flareSize = 512;
	const int mips = mipmapCount(flareSize);
	{
		vector<uint16_t> pixelData = generateFlareIntensityTex(flareSize);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareIntensityTex);
		glTextureStorage1D(flareIntensityTex, mips, GL_R16F, flareSize);
		glTextureSubImage1D(flareIntensityTex, 0, 0, flareSize, GL_RED, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareIntensityTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareIntensityTex);
	}
	{
		vector<uint8_t> pixelData = generateFlareLinesTex(flareSize);
		glCreateTextures(GL_TEXTURE_2D, 1, &flareLinesTex);
		glTextureStorage2D(flareLinesTex, mips, GL_R8, flareSize, flareSize);
		glTextureSubImage2D(flareLinesTex, 0, 0, 0, flareSize, flareSize, GL_RED, GL_UNSIGNED_BYTE, pixelData.data());
		glGenerateTextureMipmap(flareLinesTex);
	}
	{
		vector<uint16_t> pixelData = generateFlareHaloTex(flareSize);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareHaloTex);
		glTextureStorage1D(flareHaloTex, mips, GL_RGBA16F, flareSize);
		glTextureSubImage1D(flareHaloTex, 0, 0, flareSize, GL_RGBA, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareHaloTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareHaloTex);
	}
}

void RendererGL::createVertexArray()
{
	// Vertex Array Object creation
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &vertexArray);
	glVertexArrayVertexBuffer(vertexArray, VERTEX_BINDING, vertexBuffer.getId(), 0, sizeof(Vertex));
	glVertexArrayElementBuffer(vertexArray, indexBuffer.getId());

	const int VERTEX_ATTRIB_POS     = 0;
	const int VERTEX_ATTRIB_UV      = 1;
	const int VERTEX_ATTRIB_NORMAL  = 2;
	const int VERTEX_ATTRIB_TANGENT = 3;

	// Position
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_POS, 4, GL_FLOAT, false, offsetof(Vertex, position));

	// UVs
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_UV, 4, GL_FLOAT, false, offsetof(Vertex, uv));

	// Normals
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_NORMAL);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_NORMAL, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_NORMAL, 4, GL_FLOAT, false, offsetof(Vertex, normal));

	// Tangents
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_TANGENT);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_TANGENT, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_TANGENT, 4, GL_FLOAT, false, offsetof(Vertex, tangent));
}

void RendererGL::createRendertargets()
{
	// Depth stencil texture
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthStencilTex);
	glTextureStorage2DMultisample(
		depthStencilTex, msaaSamples, GL_DEPTH24_STENCIL8, windowWidth, windowHeight, GL_FALSE);

	// HDR MSAA Rendertarget
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &hdrMSRendertarget);
	glTextureStorage2DMultisample(hdrMSRendertarget, msaaSamples, GL_R11F_G11F_B10F,
		windowWidth, windowHeight, GL_FALSE);

	// Highpass rendertargets
	glCreateTextures(GL_TEXTURE_2D, 5, highpassRendertargets);
	for (int i=0;i<5;++i)
	{
		glTextureStorage2D(highpassRendertargets[i], 
			1, GL_R11F_G11F_B10F, windowWidth>>i, windowHeight>>i);
	}

	// Bloom rendertargets
	glCreateTextures(GL_TEXTURE_2D, 4, bloomRendertargets);
	for (int i=0;i<4;++i)
	{
		glTextureStorage2D(bloomRendertargets[i], 
			1, GL_R11F_G11F_B10F, windowWidth>>(i+1), windowHeight>>(i+1));

	}

	// Sampler
	glCreateSamplers(1, &rendertargetSampler);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Framebuffers
	glCreateFramebuffers(1, &hdrFbo);
	glNamedFramebufferTexture(hdrFbo, GL_COLOR_ATTACHMENT0, hdrMSRendertarget, 0);
	glNamedFramebufferTexture(hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);

	// Enable SRGB output
	glEnable(GL_FRAMEBUFFER_SRGB);
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

pair<bool, string> checkShaderProgram(const GLuint program)
{
	GLint success{0};
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success) return make_pair(true, "");

	string log;
	log.resize(2048);
	glGetProgramInfoLog(program, log.size(), nullptr, &log[0]);
	return make_pair(false, log);
}

GLuint createShader(const GLenum type, const vector<string> &sources)
{
	const vector<const char*> csources = [&]{
		vector<const char*> s(sources.size());
		for (int i=0;i<sources.size();++i) s[i] = sources[i].c_str();
		return s;
	}();
	const GLuint program{
		glCreateShaderProgramv(type, csources.size(), csources.data())};
	const auto res{checkShaderProgram(program)};
	if (!res.first)
		throw runtime_error(string("Can't create shader : ") + res.second);
	return program;
}

void RendererGL::createShaders()
{
	// Base folder
	const string folder{"shaders/"};

	// Header
	const string headerSource{loadSource(folder, "header.shad")};

	// Vert shaders
	const string planetVertSource{loadSource(folder, "planet.vert")};
	const string flareVertSource{loadSource(folder, "flare.vert")};
	const string deferredSource{loadSource(folder, "deferred.vert")};
	
	// Frag shaders
	const string planetFragSource{loadSource(folder, "planet.frag")};
	const string atmoSource{loadSource(folder, "atmo.frag")};
	const string ringFragSource{loadSource(folder, "ring.frag")};
	const string flareFragSource{loadSource(folder, "flare.frag")};
	const string tonemapSource{loadSource(folder, "tonemap.frag")};

	// Compute shaders
	const string downsampleSource{loadSource(folder, "downsample.comp")};
	const string highpassSource{loadSource(folder, "highpass.comp")};
	const string blurWSource{loadSource(folder, "blur_w.comp")};
	const string blurHSource{loadSource(folder, "blur_h.comp")};
	const string bloomAddSource{loadSource(folder, "bloom_add.comp")};

	// Defines
	const string isStar{"#define IS_STAR\n"};
	const string hasAtmo{"#define HAS_ATMO\n"};
	const string isAtmo{"#define IS_ATMO\n"};
	const string isFarRing{"#define IS_FAR_RING\n"};
	const string isNearRing{"#define IS_NEAR_RING\n"};

	// Vertex shader programs
	shaderVertPlanetBare = createShader(GL_VERTEX_SHADER, 
		{headerSource, planetVertSource});

	shaderVertPlanetAtmo = createShader(GL_VERTEX_SHADER, 
		{headerSource, hasAtmo, planetVertSource});

	shaderVertAtmo = createShader(GL_VERTEX_SHADER, 
		{headerSource, isAtmo, planetVertSource});

	shaderVertSun = createShader(GL_VERTEX_SHADER,
		{headerSource, isStar, planetVertSource});

	shaderVertRingFar = createShader(GL_VERTEX_SHADER,
		{headerSource, isFarRing, planetVertSource});

	shaderVertRingNear = createShader(GL_VERTEX_SHADER,
		{headerSource, isNearRing, planetVertSource});

	shaderVertFlare = createShader(GL_VERTEX_SHADER,
		{headerSource, flareVertSource});

	shaderVertTonemap = createShader(GL_VERTEX_SHADER,
		{headerSource, deferredSource});

	// Fragment shader programs
	shaderFragPlanetBare = createShader(GL_FRAGMENT_SHADER, 
		{headerSource, planetFragSource});

	shaderFragPlanetAtmo = createShader(GL_FRAGMENT_SHADER, 
		{headerSource, hasAtmo, planetFragSource});

	shaderFragAtmo = createShader(GL_FRAGMENT_SHADER, 
		{headerSource, isAtmo, atmoSource});

	shaderFragSun = createShader(GL_FRAGMENT_SHADER,
		{headerSource, isStar, planetFragSource});

	shaderFragRingFar = createShader(GL_FRAGMENT_SHADER,
		{headerSource, isFarRing, ringFragSource});

	shaderFragRingNear = createShader(GL_FRAGMENT_SHADER,
		{headerSource, isNearRing, ringFragSource});

	shaderFragFlare = createShader(GL_FRAGMENT_SHADER,
		{headerSource, flareFragSource});

	shaderFragTonemap = createShader(GL_FRAGMENT_SHADER,
		{headerSource, tonemapSource});

	// Compute shader programs
	shaderCompHighpass = createShader(GL_COMPUTE_SHADER,
		{headerSource, highpassSource});

	shaderCompDownsample = createShader(GL_COMPUTE_SHADER,
		{headerSource, downsampleSource});

	shaderCompBlurW = createShader(GL_COMPUTE_SHADER,
		{headerSource, blurWSource});

	shaderCompBlurH = createShader(GL_COMPUTE_SHADER,
		{headerSource, blurHSource});

	shaderCompBloomAdd = createShader(GL_COMPUTE_SHADER,
		{headerSource, bloomAddSource});

	// Pipelines
	glCreateProgramPipelines(1, &pipelinePlanetBare);
	glCreateProgramPipelines(1, &pipelinePlanetAtmo);
	glCreateProgramPipelines(1, &pipelineAtmo);
	glCreateProgramPipelines(1, &pipelineSun);
	glCreateProgramPipelines(1, &pipelineRingFar);
	glCreateProgramPipelines(1, &pipelineRingNear);
	glCreateProgramPipelines(1, &pipelineFlare);
	glCreateProgramPipelines(1, &pipelineTonemap);

	// Compute pipelines
	glCreateProgramPipelines(1, &pipelineHighpass);
	glCreateProgramPipelines(1, &pipelineDownsample);
	glCreateProgramPipelines(1, &pipelineBlurW);
	glCreateProgramPipelines(1, &pipelineBlurH);
	glCreateProgramPipelines(1, &pipelineBloomAdd);

	// Pipeline use
	glUseProgramStages(pipelinePlanetBare, GL_VERTEX_SHADER_BIT, shaderVertPlanetBare);
	glUseProgramStages(pipelinePlanetAtmo, GL_VERTEX_SHADER_BIT, shaderVertPlanetAtmo);
	glUseProgramStages(pipelineAtmo, GL_VERTEX_SHADER_BIT, shaderVertAtmo);
	glUseProgramStages(pipelineSun, GL_VERTEX_SHADER_BIT, shaderVertSun);
	glUseProgramStages(pipelineRingFar, GL_VERTEX_SHADER_BIT, shaderVertRingFar);
	glUseProgramStages(pipelineRingNear, GL_VERTEX_SHADER_BIT, shaderVertRingNear);
	glUseProgramStages(pipelineFlare, GL_VERTEX_SHADER_BIT, shaderVertFlare);
	glUseProgramStages(pipelineTonemap, GL_VERTEX_SHADER_BIT, shaderVertTonemap);

	glUseProgramStages(pipelinePlanetBare, GL_FRAGMENT_SHADER_BIT, shaderFragPlanetBare);
	glUseProgramStages(pipelinePlanetAtmo, GL_FRAGMENT_SHADER_BIT, shaderFragPlanetAtmo);
	glUseProgramStages(pipelineAtmo, GL_FRAGMENT_SHADER_BIT, shaderFragAtmo);
	glUseProgramStages(pipelineSun, GL_FRAGMENT_SHADER_BIT, shaderFragSun);
	glUseProgramStages(pipelineRingFar, GL_FRAGMENT_SHADER_BIT, shaderFragRingFar);
	glUseProgramStages(pipelineRingNear, GL_FRAGMENT_SHADER_BIT, shaderFragRingNear);
	glUseProgramStages(pipelineFlare, GL_FRAGMENT_SHADER_BIT, shaderFragFlare);
	glUseProgramStages(pipelineTonemap, GL_FRAGMENT_SHADER_BIT, shaderFragTonemap);

	glUseProgramStages(pipelineHighpass, GL_COMPUTE_SHADER_BIT, shaderCompHighpass);
	glUseProgramStages(pipelineDownsample, GL_COMPUTE_SHADER_BIT, shaderCompDownsample);
	glUseProgramStages(pipelineBlurW, GL_COMPUTE_SHADER_BIT, shaderCompBlurW);
	glUseProgramStages(pipelineBlurH, GL_COMPUTE_SHADER_BIT, shaderCompBlurH);
	glUseProgramStages(pipelineBloomAdd, GL_COMPUTE_SHADER_BIT, shaderCompBloomAdd);
}

void RendererGL::createScreenshot()
{
	// Find best transfer format
	glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_READ_PIXELS_FORMAT,
		1, (GLint*)&screenshotBestFormat);
	if (screenshotBestFormat != GL_BGRA) screenshotBestFormat = GL_RGBA;

	screenshotBuffer.resize(4*windowWidth*windowHeight);
}

void RendererGL::destroy()
{
	// Kill thread
	{
		lock_guard<mutex> lk1(texWaitMutex);
		lock_guard<mutex> lk2(screenshotMutex);
		killThread = true;
	}
	texWaitCondition.notify_one();
	texLoadThread.join();
	screenshotCond.notify_one();
	screenshotThread.join();
}

void RendererGL::takeScreenshot(const string &filename)
{
	screenshot = true;
	screenshotFilename = filename;
}

RendererGL::PlanetData::StreamTex RendererGL::loadDDSTexture(
	const string filename,
	int *lodMin,
	const vec4 defaultColor)
{
	DDSLoader loader(maxTexSize);
	if (!loader.open(filename)) return PlanetData::StreamTex(0,0,0,0);

	if (loader.getMipmapCount() != 
		mipmapCount(std::max(loader.getWidth(0), loader.getHeight(0))))
	{
		cout << "Warning: Can't load stream texture " << filename
		<< ": not enough mipmaps" << endl;
		return PlanetData::StreamTex(0,0,0,0);
	}

	const int mipmapCount = loader.getMipmapCount();
	// Create texture
	GLuint id;
	glCreateTextures(GL_TEXTURE_2D, 1, &id);
	glTextureStorage2D(id, 
		mipmapCount, DDSFormatToGL(loader.getFormat()), 
		loader.getWidth(0), loader.getHeight(0));
	// Starting lod
	const int maxLod = mipmapCount-1;
	const vector<uint8_t> block = loader.getImageData(maxLod);
	glCompressedTextureSubImage2D(id, maxLod, 0, 0, 1, 1, 
		DDSFormatToGL(loader.getFormat()), block.size(), block.data());

	// Create sampler
	GLuint sampler;
	glCreateSamplers(1, &sampler);
	glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameteri(sampler, GL_TEXTURE_MIN_LOD, maxLod);

	// Create jobs
	const int jobCount = mipmapCount-1;
	vector<TexWait> texWait(jobCount);
	for (int i=0;i<jobCount;++i)
	{
		texWait[i].id = id;
		texWait[i].sampler = sampler;
		texWait[i].lodMin = lodMin;
		texWait[i].mipmap = jobCount-i-1;
		texWait[i].loader = loader;
	}

	// Push jobs to queue
	{
		lock_guard<mutex> lk(texWaitMutex);
		for (auto tw : texWait)
		{
			texWaitQueue.push(tw);
		}
	}
	// Wake up loading thread
	texWaitCondition.notify_one();
	return PlanetData::StreamTex(id, sampler, maxLod, maxLod);
	
}

void RendererGL::unloadDDSTexture(PlanetData::StreamTex tex)
{
	if (tex.id)
		glDeleteTextures(1, &tex.id);
	if (tex.sampler)
		glDeleteSamplers(1, &tex.sampler);
}

void RendererGL::render(
		const dvec3 viewPos, 
		const float fovy,
		const dvec3 viewCenter,
		const vec3 viewUp,
		const float exposure,
		const float ambientColor,
		const vector<PlanetState> planetStates)
{

	const float closePlanetMinSizePixels = 1;
	this->closePlanetMaxDistance = windowHeight/(closePlanetMinSizePixels*tan(fovy/2));
	this->farPlanetMinDistance = closePlanetMaxDistance*0.35;
	this->farPlanetOptimalDistance = closePlanetMaxDistance*2.0;
	this->texLoadDistance = closePlanetMaxDistance*1.4;
	this->texUnloadDistance = closePlanetMaxDistance*1.6;

	profiler.begin("Full frame");

	auto &currentData = dynamicData[frameId];

	// Projection and view matrices
	const mat4 projMat = perspective(fovy, windowWidth/(float)windowHeight, 1.f, (float)5e10);
	const mat4 viewMat = lookAt(vec3(0), (vec3)(viewCenter-viewPos), viewUp);

	// Planet classification
	vector<uint32_t> closePlanets;
	vector<uint32_t> farPlanets;
	vector<uint32_t> atmoPlanets;

	vector<uint32_t> texLoadPlanets;
	vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<planetStates.size();++i)
	{
		auto &data = planetData[i];
		const float radius = planetParams[i].getBody().getRadius();
		const dvec3 pos = planetStates[i].getPosition();
		const double dist = distance(viewPos, pos)/radius;
		if (dist < texLoadDistance && !data.texLoaded)
		{
			// Textures need to be loaded
			texLoadPlanets.push_back(i);
		}
		else if (dist > texUnloadDistance && data.texLoaded)
		{
			// Textures need to be unloaded
			texUnloadPlanets.push_back(i);
		}

		// Don't render planets behind the view
		if ((viewMat*vec4(pos - viewPos,1.0)).z < 0)
		{
			if (dist < closePlanetMaxDistance)
			{
				// Detailed model
				closePlanets.push_back(i);
				// Planet atmospheres
				if (planetParams[i].hasAtmo())
				{
					atmoPlanets.push_back(i);
				}
			}
			if (dist > farPlanetMinDistance || planetParams[i].isStar())
			{
				// Flares
				farPlanets.push_back(i);
			}
		}
	}

	// Manage stream textures
	loadTextures(texLoadPlanets);
	unloadTextures(texUnloadPlanets);
	uploadLoadedTextures();

	const float exp = pow(2, exposure);

	// Scene uniform update
	SceneDynamicUBO sceneUBO;
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.viewPos = vec4(0.0,0.0,0.0,1.0);
	sceneUBO.ambientColor = ambientColor;
	sceneUBO.exposure = exp;

	// Planet uniform update
	// Close planets (detailed model)
	vector<PlanetDynamicUBO> planetUBOs(planetCount);
	for (uint32_t i : closePlanets)
	{
		planetUBOs[i] = getPlanetUBO(viewPos, viewMat, 
			planetStates[i], planetParams[i], planetData[i]);
	}
	// Far away planets (flare)
	vector<FlareDynamicUBO> flareUBOs(planetCount);
	for (uint32_t i : farPlanets)
	{
		flareUBOs[i] = getFlareUBO(viewPos, projMat, viewMat, fovy, exp,
			planetStates[i], planetParams[i]);
	}

	// Dynamic data upload
	profiler.begin("Sync wait");
	fences[frameId].waitClient();
	profiler.end();

	uboBuffer.write(currentData.sceneUBO, &sceneUBO);
	for (uint32_t i=0;i<planetUBOs.size();++i)
	{
		uboBuffer.write(currentData.planetUBOs[i], &planetUBOs[i]);
	}
	for (uint32_t i=0;i<flareUBOs.size();++i)
	{
		uboBuffer.write(currentData.flareUBOs[i], &flareUBOs[i]);
	}

	// Planet sorting from front to back
	sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		const float distI = distance(planetStates[i].getPosition(), viewPos);
		const float distJ = distance(planetStates[j].getPosition(), viewPos);
		return distI < distJ;
	});

	// Atmosphere sorting from back to front
	sort(atmoPlanets.begin(), atmoPlanets.end(), [&](int i, int j)
	{
		const float distI = distance(planetStates[i].getPosition(), viewPos);
		const float distJ = distance(planetStates[j].getPosition(), viewPos);
		return distI > distJ;
	});

	// Backface culling
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	profiler.begin("Planets");
	renderHdr(closePlanets, currentData);
	profiler.end();
	profiler.begin("Atmospheres");
	renderAtmo(atmoPlanets, currentData);
	profiler.end();
	profiler.begin("Bloom");
	renderBloom();
	profiler.end();
	profiler.begin("Flares");
	renderFlares(farPlanets, currentData);
	profiler.end();
	profiler.begin("Tonemapping");
	renderTonemap(currentData);
	profiler.end();

	if (screenshot)
	{
		saveScreenshot();
		screenshot = false;
	}

	profiler.end();

	fences[frameId].lock();

	frameId = (frameId+1)%bufferFrames;

}

void RendererGL::saveScreenshot()
{
	// Cancel if already saving screenshot
	{
		lock_guard<mutex> lk(screenshotMutex);
		if (screenshotTaken) return;
	}
	// Read screen
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, windowWidth, windowHeight, 
		screenshotBestFormat, GL_UNSIGNED_BYTE, screenshotBuffer.data());

	// Tell the thread to save
	{
		lock_guard<mutex> lk(screenshotMutex);
		screenshotTaken = true;
	}
	screenshotCond.notify_one();
}

void RendererGL::renderHdr(
	const vector<uint32_t> closePlanets,
	const DynamicData ddata)
{
	// Depth test/write
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// No blending
	glDisable(GL_BLEND);

	// Clearing
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
	float clearColor[] = {0.f,0.f,0.f,0.f};
	float clearDepth[] = {1.f};
	glClearNamedFramebufferfv(hdrFbo, GL_COLOR, 0, clearColor);
	glClearNamedFramebufferfv(hdrFbo, GL_DEPTH, 0, clearDepth);

	// Chose default or custom textures
	vector<GLuint> diffuseTextures(planetCount);
	vector<GLuint> cloudTextures(planetCount);
	vector<GLuint> nightTextures(planetCount);

	// Planet rendering
	for (uint32_t i : closePlanets)
	{
		auto &data = planetData[i];
		const bool star = planetParams[i].isStar();
		const bool hasAtmo = planetParams[i].hasAtmo();
		glBindProgramPipeline(
			star?
			pipelineSun:
			(hasAtmo?pipelinePlanetAtmo:pipelinePlanetBare));

		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			ddata.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		// Bind textures
		bool hasDiffuse = data.diffuse.id;
		bool hasCloud = data.cloud.id;
		bool hasNight = data.night.id;
		GLuint samplers[] = {
			hasDiffuse?data.diffuse.sampler:planetTexSampler,
			hasCloud?data.cloud.sampler:planetTexSampler,
			hasNight?data.night.sampler:planetTexSampler};
		GLuint texs[] = {
			hasDiffuse?data.diffuse.id:diffuseTexDefault,
			hasCloud?data.cloud.id:cloudTexDefault,
			hasNight?data.night.id:nightTexDefault};
		glBindSamplers(2, 3, samplers);
		glBindTextures(2, 3, texs);

		// Min LOD smoothing
		for (auto tex : {&data.diffuse, &data.cloud, &data.night})
		{
			if (tex->id)
			{
				if (tex->smoothLodMin > tex->lodMin)
					tex->smoothLodMin = std::max((float)tex->lodMin, 
						tex->smoothLodMin - 0.1f*ceil(tex->smoothLodMin - tex->lodMin));
				glSamplerParameterf(tex->sampler, GL_TEXTURE_MIN_LOD, tex->smoothLodMin);
			}
		}

		if (hasAtmo)
		{
			glBindSampler(5, atmoSampler);
			glBindTextureUnit(5, data.atmoLookupTable);
		}

		data.planetModel.draw();
	}
}

void RendererGL::renderAtmo(
	const vector<uint32_t> atmoPlanets,
	const DynamicData data)
{
	// Only depth test
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// Blending
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_SRC_ALPHA);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

	for (uint32_t i : atmoPlanets)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			data.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		bool hasRings = planetParams[i].hasRing();

		auto &data = planetData[i];
		DrawCommand ringModel = data.ringModel;

		// Far rings
		if (hasRings)
		{
			glBindProgramPipeline(pipelineRingFar);
			glBindSampler(2, ringSampler);
			glBindTextureUnit(2, data.ringTex1);
			glBindSampler(3, ringSampler);
			glBindTextureUnit(3, data.ringTex2);
			ringModel.draw();
		}

		// Atmosphere
		glBindProgramPipeline(pipelineAtmo);
		
		glBindSampler(2, atmoSampler);
		glBindTextureUnit(2, data.atmoLookupTable);
		data.planetModel.draw();

		// Near rings
		if (hasRings)
		{
			glBindProgramPipeline(pipelineRingNear);
			glBindSampler(2, ringSampler);
			glBindTextureUnit(2, data.ringTex1);
			glBindSampler(3, ringSampler);
			glBindTextureUnit(3, data.ringTex2);
			ringModel.draw();
		}
	}
}

void RendererGL::renderBloom()
{
	const int workgroupSize = 16;
	// Highpass
	glBindProgramPipeline(pipelineHighpass);
	glBindImageTexture(0, hdrMSRendertarget       , 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
	glBindImageTexture(1, highpassRendertargets[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
	glDispatchCompute(
		(int)ceil(windowWidth/(float)workgroupSize), 
		(int)ceil(windowHeight/(float)workgroupSize), 1);

	glBindProgramPipeline(pipelineDownsample);
	// Downsample to 16x
	for (int i=0;i<4;++i)
	{
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glBindImageTexture(0, highpassRendertargets[i]  , 0, GL_FALSE, 0, GL_READ_ONLY, GL_R11F_G11F_B10F);
		glBindImageTexture(1, highpassRendertargets[i+1], 0, GL_FALSE, 0, GL_WRITE_ONLY,GL_R11F_G11F_B10F);
		glDispatchCompute(
			(int)ceil(windowWidth /(float)(workgroupSize<<(i+1))), 
			(int)ceil(windowHeight/(float)(workgroupSize<<(i+1))), 1);
	}

	const GLuint bloomImages[] = {
		highpassRendertargets[4], // Blur x input  - 16x
		bloomRendertargets[3],    // Blur y input
		highpassRendertargets[3], // Add image input
		bloomRendertargets[2],    // ...           - 8x
		highpassRendertargets[3],
		highpassRendertargets[2],
		bloomRendertargets[1],    // ...           - 4x
		highpassRendertargets[2],
		highpassRendertargets[1],
		bloomRendertargets[0],   // Only blur      - 2x
		highpassRendertargets[1]
	};

	for (int i=0;i<4;++i)
	{
		const int dispatchSize = workgroupSize<<(4-i);

		// Blur horizontally & vertically
		const GLuint blurImages[] = {bloomImages[i*3+0], bloomImages[i*3+1]};
		const int blurPasses = 2;
		for (int j=0;j<2;++j)
		{
			glBindProgramPipeline(j?pipelineBlurW:pipelineBlurH);
			for (int k=0;k<blurPasses;++k)
			{
				const int ping = (j*blurPasses+k)%2;
				const int pong = (ping+1)%2;
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				glBindImageTexture(0, blurImages[ping], 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
				glBindImageTexture(1, blurImages[pong], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
				glDispatchCompute(
					(int)ceil(windowWidth /(float)dispatchSize),
					(int)ceil(windowHeight/(float)dispatchSize), 1);
			}
		}

		// Add blur with higher res
		if (i!=3)
		{
			glBindProgramPipeline(pipelineBloomAdd);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			glBindSampler(0, rendertargetSampler);
			glBindTextureUnit(0, bloomImages[i*3+0]);
			glBindImageTexture(1, bloomImages[i*3+2], 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
			glBindImageTexture(2, bloomImages[i*3+3], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
			glDispatchCompute(
				(int)ceil(2*windowWidth /(float)dispatchSize),
				(int)ceil(2*windowHeight/(float)dispatchSize), 1);
		}
	}
}

void RendererGL::renderFlares(
	const vector<uint32_t> farPlanets, 
	const DynamicData data)
{
	// Only depth test
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Blending add
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

	glBindProgramPipeline(pipelineFlare);

	for (uint32_t i : farPlanets)
	{
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			data.flareUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		// Bind textures
		GLuint samplers[] = {0,0,0};
		glBindSamplers(2, 3, samplers);
		glBindTextureUnit(2, flareIntensityTex);
		glBindTextureUnit(3, flareLinesTex);
		glBindTextureUnit(4, flareHaloTex);

		flareModel.draw();
	}
}

void RendererGL::renderTonemap(const DynamicData data)
{
	// No stencil test/write
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// No depth test/write
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindProgramPipeline(pipelineTonemap);

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

	// Bind image after bloom is done
	glBindSampler(1, rendertargetSampler);
	glBindTextureUnit(1, hdrMSRendertarget);
	glBindSampler(2, rendertargetSampler);
	glBindTextureUnit(2, bloomRendertargets[0]);

	fullscreenTri.draw();
}

void RendererGL::loadTextures(const vector<uint32_t> texLoadPlanets)
{
	// Texture loading
	for (uint32_t i : texLoadPlanets)
	{
		const Planet param = planetParams[i];
		auto &data = planetData[i];
		// Textures & samplers
		data.diffuse = loadDDSTexture(param.getBody().getDiffuseFilename(), &data.diffuse.lodMin, vec4(1,1,1,1));
		if (param.hasClouds())
			data.cloud = loadDDSTexture(param.getClouds().getFilename(), &data.cloud.lodMin  , vec4(0,0,0,0));
		if (param.hasNight())
			data.night = loadDDSTexture(param.getNight().getFilename(), &data.night.lodMin  , vec4(0,0,0,0));

		// Generate atmospheric scattering lookup texture
		if (param.hasAtmo())
		{
			const int size = 128;
			vector<float> table = 
				planetParams[i].getAtmo().generateLookupTable(size, param.getBody().getRadius());

			GLuint &tex = data.atmoLookupTable;

			glCreateTextures(GL_TEXTURE_2D, 1, &tex);
			glTextureStorage2D(tex, mipmapCount(size), GL_RG32F, size, size);
			glTextureSubImage2D(tex, 0, 0, 0, size, size, GL_RG, GL_FLOAT, table.data());
			glGenerateTextureMipmap(tex);
		}

		// Load ring textures
		if (param.hasRing())
		{
			// Load files
			const Planet::Ring &ring = param.getRing();
			vector<float> backscat = ring.loadFile(ring.getBackscatFilename());
			vector<float> forwardscat = ring.loadFile(ring.getForwardscatFilename());
			vector<float> unlit = ring.loadFile(ring.getUnlitFilename());
			vector<float> transparency = ring.loadFile(ring.getTransparencyFilename());
			vector<float> color = ring.loadFile(ring.getColorFilename());

			size_t size = backscat.size();

			// Check sizes
			if (size != forwardscat.size() &&
				size != unlit.size() &&
				size != transparency.size() &&
				size*3 != color.size())
			{
				throw runtime_error("Ring texture sizes don't match");
			}

			// Assemble values into two textures (t1 for back, forward and unlit, t2 for color+transparency)
			vector<float> t1(size*3);
			vector<float> t2(size*4);
			for (int i=0;i<size;++i)
			{
				t1[i*3+0] = backscat[i];
				t1[i*3+1] = forwardscat[i];
				t1[i*3+2] = unlit[i];
				t2[i*4+0] = color[i*3+0];
				t2[i*4+1] = color[i*3+1];
				t2[i*4+2] = color[i*3+2];
				t2[i*4+3] = transparency[i];
			}

			GLuint &tex1 = data.ringTex1;
			GLuint &tex2 = data.ringTex2;

			glCreateTextures(GL_TEXTURE_1D, 1, &tex1);
			glTextureStorage1D(tex1, mipmapCount(size), GL_RGB32F, size);
			glTextureSubImage1D(tex1, 0, 0, size, GL_RGB, GL_FLOAT, t1.data());
			glGenerateTextureMipmap(tex1);

			glCreateTextures(GL_TEXTURE_1D, 1, &tex2);
			glTextureStorage1D(tex2, mipmapCount(size), GL_RGBA32F, size);
			glTextureSubImage1D(tex2, 0, 0, size, GL_RGBA, GL_FLOAT, t2.data());
			glGenerateTextureMipmap(tex2);
		}

		data.texLoaded = true;
	}
}

void RendererGL::unloadTextures(vector<uint32_t> texUnloadPlanets)
{
	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		auto &data = planetData[i];
		const auto param = planetParams[i];
		unloadDDSTexture(data.diffuse);
		unloadDDSTexture(data.cloud);
		unloadDDSTexture(data.night);

		data.diffuse = PlanetData::StreamTex();
		data.cloud = PlanetData::StreamTex();
		data.night = PlanetData::StreamTex();

		if (param.hasAtmo())
		{
			glDeleteTextures(1, &data.atmoLookupTable);
		}

		if (param.hasRing())
		{
			glDeleteTextures(1, &data.ringTex1);
			glDeleteTextures(2, &data.ringTex2);
		}

		data.texLoaded = false;
	}
}

void RendererGL::uploadLoadedTextures()
{
	// Texture uploading
	lock_guard<mutex> lk(texLoadedMutex);
	while (!texLoadedQueue.empty())
	{
		TexLoaded texLoaded = texLoadedQueue.front();
		texLoadedQueue.pop();
		glCompressedTextureSubImage2D(
			texLoaded.id,
			texLoaded.mipmap,
			0, 0, 
			texLoaded.width, texLoaded.height, 
			texLoaded.format,
			texLoaded.data.size(), texLoaded.data.data());
		*texLoaded.lodMin = std::min(*texLoaded.lodMin, texLoaded.mipmap);
	}
}

RendererGL::PlanetDynamicUBO RendererGL::getPlanetUBO(
	const dvec3 viewPos, const mat4 viewMat,
	const PlanetState state, const Planet params,
	const PlanetData data)
{
	const vec3 planetPos = state.getPosition() - viewPos;

	// Planet rotation
	const vec3 north = vec3(0,0,1);
	const vec3 rotAxis = params.getBody().getRotationAxis();
	const quat q = rotate(quat(), 
		(float)acos(dot(north, rotAxis)), 
		cross(north, rotAxis))*
		rotate(quat(), state.getRotationAngle(), north);

	// Model matrix
	const mat4 modelMat = 
		translate(mat4(), planetPos)*
		mat4_cast(q)*
		scale(mat4(), vec3(params.getBody().getRadius()));

	// Atmosphere matrix
	const mat4 atmoMat = (params.hasAtmo())?
		translate(mat4(), planetPos)*
		mat4_cast(q)*
		scale(mat4(), -vec3(params.getBody().getRadius()+params.getAtmo().getMaxHeight())):
		mat4(0.0);

	// Ring matrices
	pair<mat4, mat4> ringMatrices = [&]{
		if (!params.hasRing()) return make_pair(mat4(0),mat4(0));

		const vec3 towards = normalize(planetPos);
		const vec3 up = params.getRing().getNormal();
		const float sideflip = (dot(towards, up)<0)?1.f:-1.f;
		const vec3 right = normalize(cross(towards, up));
		const vec3 newTowards = cross(right, up);

		const mat4 lookAtFar = mat4(mat3(sideflip*right, -newTowards, up));
		const mat4 lookAtNear = mat4(mat3(-sideflip*right, newTowards, up));

		const mat4 ringFarMat = 
			translate(mat4(), planetPos)*
			lookAtFar;

		const mat4 ringNearMat =
			translate(mat4(), planetPos)*
			lookAtNear;

		return make_pair(ringFarMat, ringNearMat);
	}();
	

	// Light direction
	const vec3 lightDir = vec3(normalize(-state.getPosition()));

	PlanetDynamicUBO ubo{};
	ubo.modelMat = modelMat;
	ubo.atmoMat = atmoMat;
	ubo.ringFarMat = ringMatrices.first;
	ubo.ringNearMat = ringMatrices.second;
	ubo.planetPos = viewMat*vec4(planetPos, 1.0);
	ubo.lightDir = viewMat*vec4(lightDir,0.0);
	ubo.K = params.hasAtmo()
		?params.getAtmo().getScatteringConstant()
		:glm::vec4(0.0);
	ubo.cloudDisp = state.getCloudDisp();
	ubo.nightTexIntensity = params.hasNight()
		?params.getNight().getIntensity():0.0;
	ubo.starBrightness = params.isStar()
		?params.getStar().getBrightness():0.0;
	ubo.radius = params.getBody().getRadius();
	ubo.atmoHeight = params.hasAtmo()?params.getAtmo().getMaxHeight():0.0;

	return ubo;
}

RendererGL::FlareDynamicUBO RendererGL::getFlareUBO(
	const dvec3 viewPos, const mat4 projMat,
	const mat4 viewMat, const float fovy, const float exp, 
	const PlanetState state, const Planet params)
{
	const vec3 planetPos = vec3(state.getPosition() - viewPos);
	const vec4 clip = projMat*viewMat*vec4(planetPos,1.0);
	const vec2 screen = vec2(clip)/clip.w;
	const float FLARE_SIZE_DEGREES = 20.0;
	const mat4 modelMat = 
		translate(mat4(), vec3(screen, 0.999))*
		scale(mat4(), 
			vec3(windowHeight/(float)windowWidth,1.0,0.0)*
			FLARE_SIZE_DEGREES*(float)glm::pi<float>()/(fovy*180.0f));

	const float phaseAngle = acos(dot(
		(vec3)normalize(state.getPosition()), 
		normalize(planetPos)));
	const float phase = 
		(1-phaseAngle/glm::pi<float>())*cos(phaseAngle)+
		(1/glm::pi<float>())*sin(phaseAngle);
	const float radius = params.getBody().getRadius();
	const double dist = distance(viewPos, state.getPosition())/radius;
	const float fade = clamp(params.isStar()?
		(float)   dist/10:
		(float) ((dist-farPlanetMinDistance)/
		(farPlanetOptimalDistance-farPlanetMinDistance)),0.f,1.f);
	const vec3 cutDist = planetPos*0.005f;
	
	const float brightness = std::min(4.0f,
		exp*
		radius*radius*
		(params.isStar()?params.getStar().getBrightness():
		0.2f*phase)
		/dot(cutDist,cutDist))
		*fade;

	FlareDynamicUBO ubo;
	ubo.modelMat = modelMat;
	ubo.color = vec4(params.getBody().getMeanColor(),1.0);
	ubo.brightness = brightness;

	return ubo;
}

void RendererGL::initStreamTexThread() {
	texLoadThread = thread([&,this]()
	{
		while (true)
		{
			// Wait for kill or queue not empty
			{
				unique_lock<mutex> lk(texWaitMutex);
				texWaitCondition.wait(lk, [&]{ return killThread || !texWaitQueue.empty();});
				if (killThread) return;
			}

			// Get info about texture were are going to load
			TexWait texWait;
			{
				lock_guard<mutex> lk(texWaitMutex);
				texWait = texWaitQueue.front();
				texWaitQueue.pop();
			}

			// Load image data
			TexLoaded tl{};
			tl.id = texWait.id;
			tl.sampler = texWait.sampler;
			tl.lodMin = texWait.lodMin;
			tl.mipmap = texWait.mipmap;
			tl.format = DDSFormatToGL(texWait.loader.getFormat());
			tl.width  = texWait.loader.getWidth (tl.mipmap);
			tl.height = texWait.loader.getHeight(tl.mipmap);
			tl.data = texWait.loader.getImageData(texWait.mipmap);;

			// Emulate slow loading times with this
			//this_thread::sleep_for(chrono::milliseconds(1000));
			
			// Push loaded texture into queue
			{
				lock_guard<mutex> lk(texLoadedMutex);
				texLoadedQueue.push(tl);
			}
		}
	});
}

void RendererGL::initScreenshotThread()
{
	screenshotThread = thread([&,this]()
	{
		while (true)
		{
			// Wait on kill or screenshot waiting to be saved
			{
				unique_lock<mutex> lk(screenshotMutex);
				screenshotCond.wait(lk, [&]{ return killThread || screenshotTaken;});
				if (killThread) return;
			}

			// Create temp buffer for operations
			vector<uint8_t> buffer(4*windowWidth*windowHeight);
			{
				lock_guard<mutex> lk(screenshotMutex);
				// Flip upside down
				for (int i=0;i<windowHeight;++i)
				{
					memcpy(buffer.data()+i*windowWidth*4, 
						screenshotBuffer.data()+(windowHeight-i-1)*windowWidth*4, 
						windowWidth*4);
				}
				// Unlock resource
				screenshotTaken = false;
			}

			// Flip GL_BGRA to GL_RGBA
			if (screenshotBestFormat == GL_BGRA)
			{
				for (int i=0;i<windowWidth*windowHeight*4;i+=4)
				{
					swap(buffer[i+0], buffer[i+2]);
				}
			}

			// Save screenshot
			if (!stbi_write_png(screenshotFilename.c_str(), 
				windowWidth, windowHeight, 4,
				buffer.data(), windowWidth*4))
			{
				cout << "WARNING : Can't save screenshot " << 
					screenshotFilename << endl;
			}
		}
	});
}

vector<pair<string,uint64_t>> RendererGL::getProfilerTimes()
{
	return profiler.get();
}