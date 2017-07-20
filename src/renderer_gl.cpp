#include "renderer_gl.hpp"
#include "ddsloader.hpp"
#include "flare.hpp"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <array>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

struct ModelInfo
{
	GLenum mode;
	vector<Vertex> vertices;
	vector<Index> indices;
};

ModelInfo generateSphere(
	const int meridians, 
	const int rings)
{
	ModelInfo info{};
	// Vertices
	info.mode = GL_PATCHES;
	info.vertices.resize((meridians+1)*(rings+1));
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
			const vec3 pos = vec3(cp*ct,cp*st,sp);
			const vec2 uv = vec2(j/(float)meridians, 1.f-i/(float)rings);
			const vec3 normal = normalize(pos);
			const vec3 tangent = cross(normal, vec3(0,0,1));
			info.vertices[offset] = {
				vec4(pos,1), 
				vec4(uv,0,0),
				vec4(normal,0),
				vec4(tangent,0)
			};
			offset++;
		}
	}

	// Indices
	info.indices.resize(meridians*rings*4);
	offset = 0;
	for (int i=0;i<rings;++i)
	{
		for (int j=0;j<meridians;++j)
		{
			const Index i1 = i+1;
			const Index j1 = j+1;
			vector<Index> ind = {
				(Index)(i *(rings+1)+j),
				(Index)(i *(rings+1)+j1),
				(Index)(i1*(rings+1)+j),
				(Index)(i1*(rings+1)+j1)
			};
			// Copy to indices
			memcpy(&info.indices[offset], ind.data(), ind.size()*sizeof(Index));
			offset += 4;
		}
	}
	return info;
}

ModelInfo generateFullscreenTri()
{
	ModelInfo info{};
	info.mode = GL_TRIANGLES;
	info.vertices.resize(3);
	info.vertices[0].position = vec4(-2,-1,0,1);
	info.vertices[1].position = vec4( 2,-1,0,1);
	info.vertices[2].position = vec4( 0, 4,0,1);

	info.indices = {0,1,2};
	return info;
}

ModelInfo generateFlareModel()
{
	ModelInfo info{};
	info.mode = GL_TRIANGLES;
	const int detail = 32;
	info.vertices.resize((detail+1)*2);

	for (int i=0;i<=detail;++i)
	{
		const float f = i/(float)detail;
		const vec2 pos = vec2(cos(f*2*glm::pi<float>()),sin(f*2*glm::pi<float>()));
		info.vertices[i*2+0] = {vec4(0,0,0,1), vec4(f, 0, 0.5, 0.5)};
		info.vertices[i*2+1] = {vec4(pos,0,1), vec4(f, 1, pos*vec2(0.5)+vec2(0.5))};
	}

	info.indices.resize(detail*6);
	for (int i=0;i<detail;++i)
	{
		info.indices[i*6+0] = (i*2)+0;
		info.indices[i*6+1] = (i*2)+1;
		info.indices[i*6+2] = (i*2)+2;
		info.indices[i*6+3] = (i*2)+2;
		info.indices[i*6+4] = (i*2)+1;
		info.indices[i*6+5] = (i*2)+3;
	}
	return info;
}

ModelInfo generateRingModel(
	const int meridians,
	const float near,
	const float far)
{
	ModelInfo info{};
	info.mode = GL_PATCHES;
	info.vertices.resize((meridians+1)*2);
	info.indices.resize(meridians*4);

	{
		int offset = 0;
		for (int i=0;i<=meridians;++i)
		{
			float angle = (glm::pi<float>()*i)/(float)meridians;
			vec2 pos = vec2(cos(angle),sin(angle));
			info.vertices[offset+0] = {vec4(pos*near, 0.0,1.0), vec4(pos*1.f,0.0,0.0)};
			info.vertices[offset+1] = {vec4(pos*far , 0.0,1.0), vec4(pos*2.f,0.0,0.0)};
			offset += 2;
		}
	}

	{
		int offset = 0;
		Index vert = 0;
		for (int i=0;i<meridians;++i)
		{
			info.indices[offset+0] = vert+0;
			info.indices[offset+1] = vert+1;
			info.indices[offset+2] = vert+2;
			info.indices[offset+3] = vert+3;
			offset += 4;
			vert += 2; 
		}
	}
	return info;
}

vector<DrawCommand> getCommands(
	GLuint vao, 
	GLenum indexType,
	Buffer &vertexBuffer,
	Buffer &indexBuffer,
	const vector<ModelInfo> &infos)
{
	vector<DrawCommand> commands(infos.size());
	vector<pair<BufferRange, BufferRange>> ranges(infos.size());

	for (int i=0;i<infos.size();++i)
	{
		ranges[i] = make_pair(
			vertexBuffer.assignVertices(infos[i].vertices.size(), sizeof(Vertex)),
			indexBuffer.assignIndices(infos[i].indices.size(), sizeof(Index)));
		commands[i] = DrawCommand(
			vao, infos[i].mode, indexType,
			sizeof(Vertex), sizeof(Index),
			ranges[i].first,
			ranges[i].second);
	}

	vertexBuffer.validate();
	indexBuffer.validate();

	for (int i=0;i<infos.size();++i)
	{
		vertexBuffer.write(ranges[i].first, infos[i].vertices.data());
		indexBuffer.write(ranges[i].second, infos[i].indices.data());
	}

	return commands;
}

void RendererGL::createModels()
{
	vertexBuffer = Buffer(
		Buffer::Usage::STATIC,
		Buffer::Access::WRITE_ONLY);

	indexBuffer = Buffer(
		Buffer::Usage::STATIC, 
		Buffer::Access::WRITE_ONLY);

	createVertexArray();

	const int modelCount = 3;
	const int fsTriModelId  = 0;
	const int flareModelId  = 1;
	const int sphereModelId = 2;

	// Static vertex & index data
	vector<ModelInfo> modelInfos(modelCount);

	// Fullscreen tri
	modelInfos[fsTriModelId] = generateFullscreenTri();

	// Flare
	modelInfos[flareModelId] = generateFlareModel();

	// Sphere
	const int planetMeridians = 32;
	const int planetRings = 32;
	modelInfos[sphereModelId] = generateSphere(planetMeridians, planetRings);

	// Load custom models
	vector<int> planetModelId(planetCount, sphereModelId);
	vector<int> ringModelId(planetCount, -1);
	for (uint32_t i=0;i<planetCount;++i)
	{
		const Planet param = planetParams[i];
		// Rings
		if (param.hasRing())
		{
			ringModelId[i] = modelInfos.size();
			const float near = param.getRing().getInnerDistance();
			const float far = param.getRing().getOuterDistance();
			const int ringMeridians = 32;
			modelInfos.push_back(generateRingModel(ringMeridians, near, far));
		}
	}

	vector<DrawCommand> commands = getCommands(
		vertexArray, indexType(),
		vertexBuffer,
		indexBuffer,
		modelInfos);

	fullscreenTri = commands[fsTriModelId];
	flareModel    = commands[flareModelId];
	sphere        = commands[sphereModelId];

	for (uint32_t i=0;i<planetCount;++i)
	{
		auto &data = planetData[i];
		data.planetModel = commands[planetModelId[i]];
		data.ringModel   = commands[ringModelId[i]];
	}
}

void RendererGL::createUBO()
{
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
}

void RendererGL::init(
	const vector<Planet> &planetParams,
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

	createModels();
	createUBO();
	createShaders();
	createRendertargets();
	createTextures();
	createScreenshot();

	// Backface culling
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// Clip control
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// Depth test
	glEnable(GL_DEPTH_TEST);

	// Blending
	glEnable(GL_BLEND);

	// Patch primitives
	glPatchParameteri(GL_PATCH_VERTICES, 4);
}

float getAnisotropy(const int requestedAnisotropy)
{
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

	return (requestedAnisotropy > maxAnisotropy)?maxAnisotropy:requestedAnisotropy;
}

GLuint create1PixTex(const array<uint8_t, 4> pixColor)
{
	GLuint id;
	glCreateTextures(GL_TEXTURE_2D, 1, &id);
	glTextureStorage2D(id, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(id, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixColor.data());
	return id;
}

void RendererGL::createTextures()
{
	// Anisotropy
	const float requestedAnisotropy = 16.f;
	textureAnisotropy = getAnisotropy(requestedAnisotropy);

	// Streamer init
	streamer.init(textureAnisotropy, 512*512, 200, maxTexSize);

	// Default textures 
	diffuseTexDefault = create1PixTex({0,0,0,255});
	cloudTexDefault = create1PixTex({0,0,0,0});
	nightTexDefault = create1PixTex({0,0,0,0});
	specularTexDefault = create1PixTex({0,0,0,0});

	createFlare();

	// Samplers
	glCreateSamplers(1, &planetTexSampler);
	glSamplerParameterf(planetTexSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

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

	const GLenum hdrFormat = GL_RGB16F;

	// HDR MSAA Rendertarget
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &hdrMSRendertarget);
	glTextureStorage2DMultisample(hdrMSRendertarget, msaaSamples, hdrFormat,
		windowWidth, windowHeight, GL_FALSE);

	// Highpass rendertargets
	glCreateTextures(GL_TEXTURE_2D, 1, &highpassRendertargets);
	glTextureStorage2D(highpassRendertargets, bloomDepth+1, 
		hdrFormat, windowWidth, windowHeight);

	// Highpass views
	highpassViews.resize(bloomDepth+1);
	glGenTextures(highpassViews.size(), highpassViews.data());
	for (int i=0;i<highpassViews.size();++i)
	{
		glTextureView(highpassViews[i], GL_TEXTURE_2D, highpassRendertargets, 
			hdrFormat, i, 1, 0, 1);
	}

	// Bloom rendertargets
	glCreateTextures(GL_TEXTURE_2D, 1, &bloomRendertargets);
	glTextureStorage2D(bloomRendertargets, bloomDepth,
		hdrFormat, windowWidth/2, windowHeight/2);

	// Bloom views
	bloomViews.resize(bloomDepth);
	glGenTextures(bloomViews.size(), bloomViews.data());
	for (int i=0;i<bloomViews.size();++i)
	{
		glTextureView(bloomViews[i], GL_TEXTURE_2D, bloomRendertargets,
			hdrFormat, i, 1, 0, 1);
	}

	// Sampler
	glCreateSamplers(1, &rendertargetSampler);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Framebuffers
	glCreateFramebuffers(1, &hdrFBO);
	glNamedFramebufferTexture(hdrFBO, GL_COLOR_ATTACHMENT0, hdrMSRendertarget, 0);
	glNamedFramebufferTexture(hdrFBO, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);

	highpassFBOs.resize(bloomDepth+1);
	glCreateFramebuffers(highpassFBOs.size(), highpassFBOs.data());
	for (int i=0;i<highpassFBOs.size();++i)
		glNamedFramebufferTexture(highpassFBOs[i], GL_COLOR_ATTACHMENT0, highpassViews[i], 0);

	bloomFBOs.resize(bloomDepth);
	glCreateFramebuffers(bloomFBOs.size(), bloomFBOs.data());
	for (int i=0;i<bloomFBOs.size();++i)
		glNamedFramebufferTexture(bloomFBOs[i], GL_COLOR_ATTACHMENT0, bloomViews[i], 0);

	// Enable SRGB output
	glEnable(GL_FRAMEBUFFER_SRGB);
}

void RendererGL::createShaders()
{
	ShaderFactory factory;
	factory.setVersion(450);
	factory.setFolder("shaders/");
	factory.setSandbox("sandbox.shad");

	// Vert shaders
	const string planetVert = "planet.vert";
	const string flareVert = "flare.vert";
	const string deferred = "deferred.vert";

	// Tesc shaders
	const string planetTesc = "planet.tesc";

	// Tese shaders
	const string planetTese = "planet.tese";
	
	// Frag shaders
	const string planetFrag = "planet.frag";
	const string atmo = "atmo.frag";
	const string ringFrag = "ring.frag";
	const string highpass = "highpass.frag";
	const string downsample = "downsample.frag";
	const string blur = "blur.frag";
	const string bloomAdd = "bloom_add.frag";
	const string flareFrag = "flare.frag";
	const string tonemap = "tonemap.frag";

	// Defines
	const string isStar = "IS_STAR";
	const string hasAtmo = "HAS_ATMO";
	const string isAtmo = "IS_ATMO";
	const string isFarRing = "IS_FAR_RING";
	const string isNearRing = "IS_NEAR_RING";
	const string hasRing = "HAS_RING";

	const string blurW = "BLUR_W";
	const string blurH = "BLUR_H";

	const vector<GLenum> tessellated = {
		GL_VERTEX_SHADER, 
		GL_TESS_CONTROL_SHADER, 
		GL_TESS_EVALUATION_SHADER, 
		GL_FRAGMENT_SHADER
	};

	const vector<GLenum> notTessellated = {
		GL_VERTEX_SHADER,
		GL_FRAGMENT_SHADER
	};

	const vector<string> planetFilenames = {
		planetVert, planetTesc, planetTese, planetFrag
	};

	pipelinePlanetBare = factory.createPipeline(
		tessellated,
		planetFilenames);

	pipelinePlanetAtmo = factory.createPipeline(
		tessellated,
		planetFilenames,
		{hasAtmo});

	pipelinePlanetAtmoRing = factory.createPipeline(
		tessellated,
		planetFilenames,
		{hasAtmo, hasRing});

	pipelineAtmo = factory.createPipeline(
		tessellated,
		{planetVert, planetTesc, planetTese, atmo},
		{isAtmo});

	pipelineSun = factory.createPipeline(
		tessellated,
		planetFilenames,
		{isStar});

	const vector<string> ringFilenames = {
		planetVert, planetTesc, planetTese, ringFrag
	};

	pipelineRingFar = factory.createPipeline(
		tessellated,
		ringFilenames,
		{isFarRing});

	pipelineRingNear = factory.createPipeline(
		tessellated,
		ringFilenames,
		{isNearRing});

	auto deferredFrag = [&](string p) { return vector<string>{deferred, p}; };

	pipelineHighpass = factory.createPipeline(
		notTessellated,
		deferredFrag(highpass));

	pipelineDownsample = factory.createPipeline(
		notTessellated,
		deferredFrag(downsample));

	pipelineBlurW = factory.createPipeline(
		notTessellated,
		deferredFrag(blur),
		{blurW});

	pipelineBlurH = factory.createPipeline(
		notTessellated,
		deferredFrag(blur),
		{blurH});

	pipelineBloomAdd = factory.createPipeline(
		notTessellated,
		deferredFrag(bloomAdd));

	pipelineFlare = factory.createPipeline(
		notTessellated,
		{flareVert, flareFrag});

	pipelineTonemap = factory.createPipeline(
		notTessellated,
		deferredFrag(tonemap));
}

void RendererGL::createScreenshot()
{
	// Find best transfer format
	glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_READ_PIXELS_FORMAT,
		1, (GLint*)&screenBestFormatGL);
	if (screenBestFormatGL != GL_BGRA) screenBestFormatGL = GL_RGBA;

	if (screenBestFormatGL == GL_RGBA) 
		screenBestFormat = Screenshot::Format::RGBA8;
	else if (screenBestFormatGL == GL_BGRA) 
		screenBestFormat = Screenshot::Format::BGRA8;
}

void RendererGL::destroy()
{

}

void RendererGL::takeScreenshot(const string &filename)
{
	takeScreen = true;
	screenFilename = filename;
}

void RendererGL::render(
		const dvec3 &viewPos, 
		const float fovy,
		const dvec3 &viewCenter,
		const vec3 &viewUp,
		const float exposure,
		const float ambientColor,
		const bool wireframe,
		const vector<PlanetState> &planetStates)
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
	const mat4 projMat = perspective(fovy, windowWidth/(float)windowHeight, 0.f,1.f);
	const mat4 viewMat = lookAt(vec3(0), (vec3)(viewCenter-viewPos), viewUp);

	// Planet classification
	vector<uint32_t> closePlanets;
	vector<uint32_t> farPlanets;
	vector<uint32_t> translucentPlanets;

	vector<uint32_t> texLoadPlanets;
	vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<planetStates.size();++i)
	{
		auto &data = planetData[i];
		const auto &param = planetParams[i];
		const auto &state = planetStates[i];
		const float radius = param.getBody().getRadius();
		const float maxRadius = radius+(param.hasRing()?
			param.getRing().getOuterDistance():0);
		const dvec3 pos = state.getPosition();
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
		if ((viewMat*vec4(pos - viewPos,1.0)).z < maxRadius)
		{
			if (dist < closePlanetMaxDistance)
			{
				// Detailed model
				closePlanets.push_back(i);
				// Planet atmospheres
				if (param.hasAtmo() || param.hasRing())
				{
					translucentPlanets.push_back(i);
				}
			}
			if (dist > farPlanetMinDistance || param.isStar())
			{
				// Flares
				farPlanets.push_back(i);
			}
		}
	}

	// Manage stream textures
	profiler.begin("Texture management");
	loadTextures(texLoadPlanets);
	unloadTextures(texUnloadPlanets);
	uploadLoadedTextures();
	profiler.end();

	const float exp = pow(2, exposure);

	// Scene uniform update
	SceneDynamicUBO sceneUBO{};
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.viewPos = vec4(0.0,0.0,0.0,1.0);
	sceneUBO.ambientColor = ambientColor;
	sceneUBO.exposure = exp;
	sceneUBO.logDepthFarPlane = (1.0/log2(logDepthC*logDepthFarPlane + 1.0));
	sceneUBO.logDepthC = logDepthC;

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
	sort(translucentPlanets.begin(), translucentPlanets.end(), [&](int i, int j)
	{
		const float distI = distance(planetStates[i].getPosition(), viewPos);
		const float distJ = distance(planetStates[j].getPosition(), viewPos);
		return distI > distJ;
	});

	if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	profiler.begin("Planets");
	renderHdr(closePlanets, currentData);
	profiler.end();
	profiler.begin("Translucent objects");
	renderTranslucent(translucentPlanets, currentData);
	profiler.end();
	if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	profiler.begin("Highpass");
	renderHighpass(currentData);
	profiler.end();
	profiler.begin("Downsample");
	renderDownsample(currentData);
	profiler.end();
	profiler.begin("Bloom");
	renderBloom(currentData);
	profiler.end();
	if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	profiler.begin("Flares");
	renderFlares(farPlanets, currentData);
	profiler.end();
	if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	profiler.begin("Tonemapping");
	renderTonemap(currentData);
	profiler.end();

	if (takeScreen)
	{
		saveScreenshot();
		takeScreen = false;
	}

	profiler.end();

	fences[frameId].lock();

	frameId = (frameId+1)%bufferFrames;

}

void RendererGL::saveScreenshot()
{
	// Cancel if already saving screenshot
	if (screenshot.isSaving()) return;
	// Read screen
	vector<uint8_t> buffer(4*windowWidth*windowHeight);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, windowWidth, windowHeight, 
		screenBestFormatGL, GL_UNSIGNED_BYTE, buffer.data());
	screenshot.save(
		screenFilename,
		windowWidth, windowHeight, 
		screenBestFormat, buffer);
}

void RendererGL::renderHdr(
	const vector<uint32_t> &closePlanets,
	const DynamicData &ddata)
{
	// Wiewport
	glViewport(0,0, windowWidth, windowHeight);

	// Depth test/write
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	// No blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);

	// Invalidating
	const vector<GLenum> attachments = {
		GL_COLOR_ATTACHMENT0, 
		GL_DEPTH_STENCIL_ATTACHMENT};
	glInvalidateNamedFramebufferData(hdrFBO, 
		attachments.size(), attachments.data());
	// Clearing
	vector<float> clearColor = {0.f,0.f,0.f,0.f};
	vector<float> clearDepth = {1.f};
	glClearNamedFramebufferfv(hdrFBO, GL_COLOR, 0, clearColor.data());
	glClearNamedFramebufferfv(hdrFBO, GL_DEPTH, 0, clearDepth.data());
	// Bind FBO for rendering
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

	// Planet rendering
	for (uint32_t i : closePlanets)
	{
		auto &data = planetData[i];
		const bool star = planetParams[i].isStar();
		const bool hasAtmo = planetParams[i].hasAtmo();
		const bool hasRing = planetParams[i].hasRing();
		if (star) pipelineSun.bind();
		else if (hasAtmo)
		{
			if (hasRing) pipelinePlanetAtmoRing.bind();
			else pipelinePlanetAtmo.bind();
		}
		else pipelinePlanetBare.bind();

		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			ddata.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		// Bind samplers
		const vector<GLuint> samplers = {
			streamer.getTex(data.diffuse).getSamplerId(planetTexSampler),
			streamer.getTex(data.cloud).getSamplerId(planetTexSampler),
			streamer.getTex(data.night).getSamplerId(planetTexSampler),
			streamer.getTex(data.specular).getSamplerId(planetTexSampler),
			atmoSampler,
			ringSampler
		};
		// Bind textures
		const vector<GLuint> texs = {
			streamer.getTex(data.diffuse).getCompleteTextureId(diffuseTexDefault),
			streamer.getTex(data.cloud).getCompleteTextureId(cloudTexDefault),
			streamer.getTex(data.night).getCompleteTextureId(nightTexDefault),
			streamer.getTex(data.specular).getCompleteTextureId(specularTexDefault),
			data.atmoLookupTable,
			data.ringTex2,
		};

		glBindSamplers(2, samplers.size(), samplers.data());
		glBindTextures(2, texs.size(), texs.data());
		
		data.planetModel.draw();
	}
}

void RendererGL::renderTranslucent(
	const vector<uint32_t> &translucentPlanets,
	const DynamicData &data)
{
	// Wiewport
	glViewport(0,0, windowWidth, windowHeight);
	// Only depth test
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// Blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_SRC_ALPHA);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

	for (uint32_t i : translucentPlanets)
	{
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));
		// Bind Planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			data.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		bool hasRing = planetParams[i].hasRing();
		bool hasAtmo = planetParams[i].hasAtmo();

		auto &data = planetData[i];
		DrawCommand ringModel = data.ringModel;

		const vector<GLuint> samplers = {
			atmoSampler,
			ringSampler,
			ringSampler
		};

		const vector<GLuint> texs = {
			data.atmoLookupTable,
			data.ringTex1,
			data.ringTex2
		};

		glBindSamplers(2, samplers.size(), samplers.data());
		glBindTextures(2, texs.size(), texs.data());

		// Far rings
		if (hasRing)
		{
			pipelineRingFar.bind();
			ringModel.draw();
		}

		// Atmosphere
		if (hasAtmo)
		{
			pipelineAtmo.bind();
			data.planetModel.draw();
		}

		// Near rings
		if (hasRing)
		{
			pipelineRingNear.bind();
			ringModel.draw();
		}
	}
}

void RendererGL::renderHighpass(const DynamicData &data)
{
	// Wiewport
	glViewport(0,0, windowWidth, windowHeight);

	// No depth test/write
	glDepthFunc(GL_ALWAYS);
	glDepthMask(GL_FALSE);

	// No blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);

	const vector<GLuint> attachments = {GL_COLOR_ATTACHMENT0};
	glInvalidateNamedFramebufferData(highpassFBOs[0], attachments.size(), attachments.data());

	glBindFramebuffer(GL_FRAMEBUFFER, highpassFBOs[0]);

	pipelineHighpass.bind();

	const vector<GLuint> samplers = {rendertargetSampler};
	const vector<GLuint> texs = {hdrMSRendertarget};
	glBindSamplers(1, samplers.size(), samplers.data());
	glBindTextures(1, texs.size(), texs.data());

	// Bind scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

	fullscreenTri.draw();
}
	
void RendererGL::renderDownsample(const DynamicData &data)
{
	const vector<GLenum> invalidateAttach = {GL_COLOR_ATTACHMENT0};
	glBindSampler(0, rendertargetSampler);
	pipelineDownsample.bind();
	for (int i=0;i<bloomDepth;++i)
	{
		// Wiewport
		glViewport(0,0, windowWidth>>(i+1), windowHeight>>(i+1));
		glInvalidateNamedFramebufferData(highpassFBOs[i+1], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, highpassFBOs[i+1]);
		glBindTextureUnit(0, highpassViews[i]);
		fullscreenTri.draw();
	}
}

void RendererGL::renderBloom(const DynamicData &data)
{
	const vector<GLenum> invalidateAttach = {GL_COLOR_ATTACHMENT0};
	glCopyImageSubData(
		highpassViews[bloomDepth], GL_TEXTURE_2D, 0,
		0, 0, 0, 
		bloomViews[bloomDepth-1], GL_TEXTURE_2D, 0,
		0, 0, 0,
		mipmapSize(windowWidth,  bloomDepth),
		mipmapSize(windowHeight, bloomDepth),
		1);

	const vector<GLuint> samplers = {rendertargetSampler, rendertargetSampler};
	glBindSamplers(0, samplers.size(), samplers.data());
	for (int i=bloomDepth;i>=1;--i)
	{
		// Wiewport
		glViewport(0,0, windowWidth>>i, windowHeight>>i);
		// Blur horizontally
		pipelineBlurW.bind();
		glInvalidateNamedFramebufferData(highpassFBOs[i], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, highpassFBOs[i]);
		glBindTextureUnit(0, bloomViews[i-1]);
		fullscreenTri.draw();

		// Blur vertically
		pipelineBlurH.bind();
		glInvalidateNamedFramebufferData(bloomFBOs[i-1], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFBOs[i-1]);
		glBindTextureUnit(0, highpassViews[i]);
		fullscreenTri.draw();

		// Add blur with higher res
		if (i>1)
		{
			// Wiewport
			glViewport(0,0, windowWidth>>(i-1), windowHeight>>(i-1));
			pipelineBloomAdd.bind();
			glInvalidateNamedFramebufferData(bloomFBOs[i-2], invalidateAttach.size(), invalidateAttach.data());
			glBindFramebuffer(GL_FRAMEBUFFER, bloomFBOs[i-2]);
			const vector<GLuint> texs = {bloomViews[i-1], highpassViews[i-1]};
			glBindTextures(0, texs.size(), texs.data());
			fullscreenTri.draw();
		}
	}
}

void RendererGL::renderFlares(
	const vector<uint32_t> &farPlanets, 
	const DynamicData &data)
{
	// Wiewport
	glViewport(0,0, windowWidth, windowHeight);
	// Only depth test
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);

	// Blending add
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

	pipelineFlare.bind();

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
		const vector<GLuint> samplers = {0,0,0};
		const vector<GLuint> texs = {
			flareIntensityTex, flareLinesTex, flareHaloTex
		};
		glBindSamplers(2, samplers.size(), samplers.data());
		glBindTextures(2, texs.size(), texs.data());

		flareModel.draw();
	}
}

void RendererGL::renderTonemap(const DynamicData &data)
{
	// Wiewport
	glViewport(0,0, windowWidth, windowHeight);
	// No depth test/write
	glDepthFunc(GL_ALWAYS);
	glDepthMask(GL_FALSE);

	// No blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);

	// Invalidate
	const vector<GLuint> attachments = {GL_COLOR};
	glInvalidateNamedFramebufferData(0, attachments.size(), attachments.data());

	// Bind default FBO
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	pipelineTonemap.bind();

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

	// Bind image after bloom is done
	const vector<GLuint> samplers = {rendertargetSampler, rendertargetSampler};
	const vector<GLuint> texs = {hdrMSRendertarget, bloomViews[0]};
	glBindSamplers(1, samplers.size(), samplers.data());
	glBindTextures(1, texs.size(), texs.data());

	fullscreenTri.draw();
}

void RendererGL::loadTextures(const vector<uint32_t> &texLoadPlanets)
{
	// Texture loading
	for (uint32_t i : texLoadPlanets)
	{
		const Planet param = planetParams[i];
		auto &data = planetData[i];
		// Textures & samplers
		data.diffuse = streamer.createTex(param.getBody().getDiffuseFilename());
		if (param.hasClouds())
			data.cloud = streamer.createTex(param.getClouds().getFilename());
		if (param.hasNight())
			data.night = streamer.createTex(param.getNight().getFilename());
		if (param.hasSpecular())
			data.specular = streamer.createTex(param.getSpecular().getFilename());

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

void RendererGL::unloadTextures(const vector<uint32_t> &texUnloadPlanets)
{
	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		auto &data = planetData[i];

		if (data.atmoLookupTable)
			glDeleteTextures(1, &data.atmoLookupTable);

		if (data.ringTex1)
			glDeleteTextures(1, &data.ringTex1);

		if (data.ringTex2)
			glDeleteTextures(2, &data.ringTex2);

		// Reset variables
		data.texLoaded = false;
		streamer.deleteTex(data.diffuse);
		streamer.deleteTex(data.cloud);
		streamer.deleteTex(data.night);
		streamer.deleteTex(data.specular);
		data.diffuse = 0;
		data.cloud = 0;
		data.night = 0;
		data.specular = 0;
		data.atmoLookupTable = 0;
		data.ringTex1 = 0;
		data.ringTex2 = 0;
	}
}

void RendererGL::uploadLoadedTextures()
{
	// Texture uploading
	streamer.update();
}

RendererGL::PlanetDynamicUBO RendererGL::getPlanetUBO(
	const dvec3 &viewPos, const mat4 &viewMat,
	const PlanetState &state, const Planet &params,
	const PlanetData &data)
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

	const mat3 viewNormalMat = transpose(inverse(mat3(viewMat)));
	

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

	if (params.hasSpecular())
	{
		auto &spec = params.getSpecular();
		ubo.mask0ColorHardness = vec4(spec.getMask0().color, spec.getMask0().hardness);
		ubo.mask1ColorHardness = vec4(spec.getMask1().color, spec.getMask1().hardness);
	}

	if (params.hasRing())
	{
		auto &ring = params.getRing();
		ubo.ringNormal = vec4(viewNormalMat*ring.getNormal(), 0.0);
		ubo.ringInner = ring.getInnerDistance();
		ubo.ringOuter = ring.getOuterDistance();
	}

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
	const dvec3 &viewPos, const mat4 &projMat,
	const mat4 &viewMat, const float fovy, const float exp, 
	const PlanetState &state, const Planet &params)
{
	const vec3 planetPos = vec3(state.getPosition() - viewPos);
	const vec4 clip = projMat*viewMat*vec4(planetPos,1.0);
	const vec2 screen = vec2(clip)/clip.w;
	const float FLARE_SIZE_SCREEN = 0.5;
	const mat4 modelMat = 
		translate(mat4(), vec3(screen, 0.999))*
		scale(mat4(), 
			vec3(windowHeight/(float)windowWidth,1.0,0.0)*FLARE_SIZE_SCREEN);

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

	FlareDynamicUBO ubo{};
	ubo.modelMat = modelMat;
	ubo.color = vec4(params.getBody().getMeanColor(),1.0);
	ubo.brightness = brightness;

	return ubo;
}

vector<pair<string,uint64_t>> RendererGL::getProfilerTimes()
{
	return profiler.get();
}