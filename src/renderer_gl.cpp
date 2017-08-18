#include "renderer_gl.hpp"
#include "ddsloader.hpp"
#include "mesh.hpp"

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

GLenum indexType()
{
	switch (sizeof(Index))
	{
		case 2: return GL_UNSIGNED_SHORT;
		case 1: return GL_UNSIGNED_BYTE;
		default: return GL_UNSIGNED_INT;
	}
}

vector<DrawCommand> getCommands(
	GLuint vao, 
	GLenum indexType,
	Buffer &vertexBuffer,
	Buffer &indexBuffer,
	const vector<Model> &infos)
{
	vector<DrawCommand> commands(infos.size());
	vector<pair<BufferRange, BufferRange>> ranges(infos.size());

	for (size_t i=0;i<infos.size();++i)
	{
		BufferRange vertexRange = vertexBuffer.assignVertices(infos[i].getVertices().size(), sizeof(Vertex));
		BufferRange indexRange = indexBuffer.assignIndices(infos[i].getIndices().size(), sizeof(Index));
		ranges[i] = make_pair(vertexRange, indexRange);
		commands[i] = DrawCommand(
			vao, GL_TRIANGLES, indexType,
			{
				{0, vertexBuffer.getId(), vertexRange, sizeof(Vertex)}
			},
			{indexBuffer.getId(), indexRange, infos[i].getIndices().size()});
	}

	vertexBuffer.validate();
	indexBuffer.validate();

	for (size_t i=0;i<infos.size();++i)
	{
		vertexBuffer.write(ranges[i].first, infos[i].getVertices().data());
		indexBuffer.write(ranges[i].second, infos[i].getIndices().data());
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

	const int modelCount = 3;
	const int fsTriModelId  = 0;
	const int flareModelId  = 1;
	const int sphereModelId = 2;

	// Static vertex & index data
	vector<Model> modelInfos(modelCount);

	// Fullscreen tri
	modelInfos[fsTriModelId] = generateFullscreenTri();

	// Flare
	const int detail = 8;
	modelInfos[flareModelId] = generateFlareModel(detail);

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
		int ringId = ringModelId[i];
		if (ringId != -1)
			data.ringModel   = commands[ringId];
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
	}

	uboBuffer.validate();
}

void RendererGL::init(const InitInfo &info)
{
	this->planetParams = info.planetParams;
	this->planetCount = info.planetParams.size();
	this->msaaSamples = info.msaa;
	this->maxTexSize = info.maxTexSize;
	this->windowWidth = info.windowWidth;
	this->windowHeight = info.windowHeight;

	// Find the sun
	for (size_t i=0;i<planetParams.size();++i)
	{
		if (planetParams[i].isStar()) sunId = i; 
	}

	this->bufferFrames = 3; // triple-buffering

	this->planetData.resize(planetCount);
	this->fences.resize(bufferFrames);

	createVertexArray();
	createModels();
	createUBO();
	createShaders();
	createRendertargets();
	createTextures();
	createFlare();
	createScreenshot();
	createAtmoLookups();
	createRingTextures();

	// Streamer init
	streamer.init(!info.syncTexLoading, 512*512, 200, maxTexSize);

	// Create starMap texture
	starMapTexHandle = streamer.createTex(info.starMapFilename);
	starMapIntensity = info.starMapIntensity;

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
	// Default patch values
	float outerLevel[] = {1.0,1.0,1.0,1.0};
	glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevel);
	float innerLevel[] = {1.0,1.0};
	glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevel);

	// Sun Occlusion query for flare
	glCreateQueries(GL_SAMPLES_PASSED, 2, sunOcclusionQueries);
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

	// Default textures 
	diffuseTexDefault = create1PixTex({0,0,0,255});
	cloudTexDefault = create1PixTex({0,0,0,0});
	nightTexDefault = create1PixTex({0,0,0,0});
	specularTexDefault = create1PixTex({0,0,0,0});

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
	DDSLoader flareFile("tex/star_glow.DDS");
	const int mips = flareFile.getMipmapCount();
	glCreateTextures(GL_TEXTURE_2D, 1, &flareTex);
	glTextureStorage2D(
		flareTex, 
		mips, 
		DDSFormatToGL(flareFile.getFormat()),
		flareFile.getWidth(0),
		flareFile.getHeight(0));
	glTextureParameteri(flareTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	for (int i=0;i<mips;++i)
	{
		glCompressedTextureSubImage2D(flareTex,
			i, 0, 0, flareFile.getWidth(i), flareFile.getHeight(i), 
			DDSFormatToGL(flareFile.getFormat()),
			flareFile.getImageSize(i), flareFile.getImageData(i).data());
	}
}

void RendererGL::createVertexArray()
{
	// Vertex Array Object creation
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &vertexArray);

	const int VERTEX_ATTRIB_POS     = 0;
	const int VERTEX_ATTRIB_UV      = 1;
	const int VERTEX_ATTRIB_NORMAL  = 2;

	// Position
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_POS, 3, GL_FLOAT, false, offsetof(Vertex, position));

	// UVs
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_UV, 2, GL_FLOAT, false, offsetof(Vertex, uv));

	// Normals
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_NORMAL);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_NORMAL, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_NORMAL, 3, GL_FLOAT, false, offsetof(Vertex, normal));
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
	for (size_t i=0;i<highpassViews.size();++i)
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
	for (size_t i=0;i<bloomViews.size();++i)
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
	for (size_t i=0;i<highpassFBOs.size();++i)
		glNamedFramebufferTexture(highpassFBOs[i], GL_COLOR_ATTACHMENT0, highpassViews[i], 0);

	bloomFBOs.resize(bloomDepth);
	glCreateFramebuffers(bloomFBOs.size(), bloomFBOs.data());
	for (size_t i=0;i<bloomFBOs.size();++i)
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

	typedef pair<GLenum,string> shader;

	// Vert shaders
	const shader planetVert = {GL_VERTEX_SHADER, "planet.vert"};
	const shader starMapVert = {GL_VERTEX_SHADER, "starmap.vert"};
	const shader flareVert = {GL_VERTEX_SHADER, "flare.vert"};
	const shader deferred = {GL_VERTEX_SHADER, "deferred.vert"};

	// Tesc shaders
	const shader planetTesc = {GL_TESS_CONTROL_SHADER, "planet.tesc"};

	// Tese shaders
	const shader planetTese = {GL_TESS_EVALUATION_SHADER, "planet.tese"};
	const shader starMapTese = {GL_TESS_EVALUATION_SHADER, "starmap.tese"};
	
	// Frag shaders
	const shader planetFrag = {GL_FRAGMENT_SHADER, "planet.frag"};
	const shader starMapFrag = {GL_FRAGMENT_SHADER, "starmap.frag"};
	const shader atmo = {GL_FRAGMENT_SHADER, "atmo.frag"};
	const shader ringFrag = {GL_FRAGMENT_SHADER, "ring.frag"};
	const shader highpass = {GL_FRAGMENT_SHADER, "highpass.frag"};
	const shader downsample = {GL_FRAGMENT_SHADER, "downsample.frag"};
	const shader blur = {GL_FRAGMENT_SHADER, "blur.frag"};
	const shader bloomAdd = {GL_FRAGMENT_SHADER, "bloom_add.frag"};
	const shader flareFrag = {GL_FRAGMENT_SHADER, "flare.frag"};
	const shader tonemap = {GL_FRAGMENT_SHADER, "tonemap.frag"};

	// Defines
	const string isStar = "IS_STAR";
	const string hasAtmo = "HAS_ATMO";
	const string isAtmo = "IS_ATMO";
	const string isFarRing = "IS_FAR_RING";
	const string isNearRing = "IS_NEAR_RING";
	const string hasRing = "HAS_RING";

	const string blurW = "BLUR_W";
	const string blurH = "BLUR_H";

	const string bloom = "USE_BLOOM";

	const vector<shader> planetFilenames = {
		planetVert, planetTesc, planetTese, planetFrag
	};

	pipelinePlanetBare = factory.createPipeline(
		planetFilenames);

	pipelinePlanetAtmo = factory.createPipeline(
		planetFilenames,
		{hasAtmo});

	pipelinePlanetAtmoRing = factory.createPipeline(
		planetFilenames,
		{hasAtmo, hasRing});

	pipelineStarMap = factory.createPipeline(
		{starMapVert, starMapTese, starMapFrag});

	pipelineAtmo = factory.createPipeline(
		{planetVert, planetTesc, planetTese, atmo},
		{isAtmo});

	pipelineSun = factory.createPipeline(
		planetFilenames,
		{isStar});

	const vector<shader> ringFilenames = {
		planetVert, planetTesc, planetTese, ringFrag
	};

	pipelineRingFar = factory.createPipeline(
		ringFilenames,
		{isFarRing});

	pipelineRingNear = factory.createPipeline(
		ringFilenames,
		{isNearRing});

	pipelineHighpass = factory.createPipeline(
		{deferred, highpass});

	pipelineDownsample = factory.createPipeline(
		{deferred, downsample});

	pipelineBlurW = factory.createPipeline(
		{deferred, blur},
		{blurW});

	pipelineBlurH = factory.createPipeline(
		{deferred, blur},
		{blurH});

	pipelineBloomAdd = factory.createPipeline(
		{deferred, bloomAdd});

	pipelineFlare = factory.createPipeline(
		{flareVert, flareFrag});

	pipelineTonemapBloom = factory.createPipeline(
		{deferred, tonemap},
		{bloom});

	pipelineTonemapNoBloom = factory.createPipeline(
		{deferred, tonemap});
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

void RendererGL::createAtmoLookups()
{
	for (uint32_t i=0;i<planetParams.size();++i)
	{
		const Planet param = planetParams[i];
		auto &data = planetData[i];

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
	}
}

void RendererGL::createRingTextures()
{
	for (uint32_t i=0;i<planetParams.size();++i)
	{
		const Planet param = planetParams[i];
		auto &data = planetData[i];

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
			for (size_t i=0;i<size;++i)
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
	}
}

void RendererGL::destroy()
{

}

void RendererGL::takeScreenshot(const string &filename)
{
	takeScreen = true;
	screenFilename = filename;
}

bool testSpherePlane(const vec3 &sphereCenter, float radius, const vec4 &plane)
{
	return dot(sphereCenter, vec3(plane))+plane.w < radius;
}

void RendererGL::render(const RenderInfo &info)
{
	const float closePlanetMinSizePixels = 1;
	this->closePlanetMaxDistance =windowHeight/(closePlanetMinSizePixels*tan(info.fovy/2));
	this->flareMinDistance = closePlanetMaxDistance*0.35;
	this->flareOptimalDistance = closePlanetMaxDistance*1.0;
	this->texLoadDistance = closePlanetMaxDistance*1.4;
	this->texUnloadDistance = closePlanetMaxDistance*1.6;

	profiler.begin("Full frame");

	auto &currentData = dynamicData[frameId];

	// Projection and view matrices
	const mat4 projMat = perspective(info.fovy, windowWidth/(float)windowHeight, 0.f,1.f);
	const mat4 viewMat = mat4(info.viewDir);

	// Frustum construction
	const float f = tan(info.fovy/2.0);
	const float aspect = windowWidth/(float)windowHeight;

	// (Don't need far plane)
	array<vec4, 5> frustum = {
		vec4(0,0,1,0), // near plane
		vec4(normalize(vec3( 1, 0, f*aspect)), 0), // Side planes
		vec4(normalize(vec3(-1, 0, f*aspect)), 0),
		vec4(normalize(vec3(0,  1, f)), 0),
		vec4(normalize(vec3(0, -1, f)), 0)
	};

	// Planet classification
	vector<uint32_t> closePlanets;
	vector<uint32_t> translucentPlanets;
	vector<uint32_t> flares;

	vector<uint32_t> texLoadPlanets;
	vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<info.planetStates.size();++i)
	{
		auto &data = planetData[i];
		const auto &param = planetParams[i];
		const auto &state = info.planetStates[i];
		const float radius = param.getBody().getRadius();
		const float maxRadius = radius+(param.hasRing()?
			param.getRing().getOuterDistance():0);
		const dvec3 pos = state.getPosition();
		const double dist = distance(info.viewPos, pos)/radius;
		bool focused = count(
			info.focusedPlanetsId.begin(), 
			info.focusedPlanetsId.end(), i)>0;

		if ((focused || dist < texLoadDistance) && !data.texLoaded)
		{
			texLoadPlanets.push_back(i);
		}
		else if (!focused && data.texLoaded && dist > texUnloadDistance)
		{
			// Textures need to be unloaded
			texUnloadPlanets.push_back(i);
		}

		// Frustum test
		const vec3 viewSpacePos = vec3(viewMat*vec4(pos - info.viewPos,1.0));
		bool visible = true;
		for (vec4 plane : frustum)
		{
			visible = visible && testSpherePlane(viewSpacePos, maxRadius, plane);
		}

		// Render planets inside the frustum
		if (visible)
		{
			// Render if is range, always render sun
			if (dist < closePlanetMaxDistance || param.isStar())
			{
				closePlanets.push_back(i);
				// Planet atmospheres
				if (param.hasAtmo() || param.hasRing())
				{
					translucentPlanets.push_back(i);
				}
			}
		}

		if (dist > flareMinDistance && !param.isStar())
		{
			flares.push_back(i); 
		}
	}

	// Manage stream textures
	profiler.begin("Texture creation/deletion");
	loadTextures(texLoadPlanets);
	unloadTextures(texUnloadPlanets);
	profiler.end();
	profiler.begin("Texture updating");
	uploadLoadedTextures();
	profiler.end();

	const float exp = pow(2, info.exposure);

	// Scene uniform update
	SceneDynamicUBO sceneUBO{};
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.starMapMat = viewMat*scale(mat4(), vec3(-1));
	sceneUBO.starMapIntensity = starMapIntensity;

	sceneUBO.ambientColor = info.ambientColor;
	sceneUBO.exposure = exp;
	sceneUBO.logDepthFarPlane = (1.0/log2(logDepthC*logDepthFarPlane + 1.0));
	sceneUBO.logDepthC = logDepthC;

	// Planet uniform update
	vector<PlanetDynamicUBO> planetUBOs(planetCount);
	for (uint32_t i=0;i<planetCount;++i)
	{
		planetUBOs[i] = getPlanetUBO(info.fovy, exp, info.viewPos, projMat, viewMat, 
			info.planetStates[i], planetParams[i], planetData[i]);
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

	// Planet sorting from front to back
	sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		const float distI = distance(info.planetStates[i].getPosition(), info.viewPos);
		const float distJ = distance(info.planetStates[j].getPosition(), info.viewPos);
		return distI < distJ;
	});

	// Atmosphere sorting from back to front
	sort(translucentPlanets.begin(), translucentPlanets.end(), [&](int i, int j)
	{
		const float distI = distance(info.planetStates[i].getPosition(), info.viewPos);
		const float distJ = distance(info.planetStates[j].getPosition(), info.viewPos);
		return distI > distJ;
	});

	if (info.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	profiler.begin("Planets");
	renderHdr(closePlanets, currentData);
	profiler.end();
	profiler.begin("Flares");
	renderPlanetFlares(flares, currentData);
	profiler.end();
	profiler.begin("Translucent objects");
	renderTranslucent(translucentPlanets, currentData);
	profiler.end();
	if (info.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (info.bloom)
	{
		profiler.begin("Highpass");
		renderHighpass(currentData);
		profiler.end();
		profiler.begin("Downsample");
		renderDownsample(currentData);
		profiler.end();
		profiler.begin("Bloom");
		renderBloom(currentData);
		profiler.end();
	}
	profiler.begin("Tonemapping");
	renderTonemap(currentData, info.bloom);
	profiler.end();
	profiler.begin("Sun Flare");
	renderSunFlare(currentData);
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

float RendererGL::getSunVisibility()
{
	for (int i=0;i<2;++i)
	{
		glGetQueryObjectiv(sunOcclusionQueries[i], GL_QUERY_RESULT_NO_WAIT, 
			&occlusionQueryResults[i]);
	}

	return occlusionQueryResults[0]/(float)std::max(1,occlusionQueryResults[1]);
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
	// Viewport
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
			planetTexSampler,
			planetTexSampler,
			planetTexSampler,
			planetTexSampler,
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

		if (star) glBeginQuery(GL_SAMPLES_PASSED, sunOcclusionQueries[0]);
		data.planetModel.draw(true);
		if (star)
		{
			glEndQuery(GL_SAMPLES_PASSED);
			glDepthFunc(GL_ALWAYS);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDepthMask(GL_FALSE);
			glBeginQuery(GL_SAMPLES_PASSED, sunOcclusionQueries[1]);
			data.planetModel.draw(true);
			glEndQuery(GL_SAMPLES_PASSED);
			glDepthFunc(GL_LESS);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);
		}
	}

	// Star map rendering
	// Don't render if star map texture not loaded
	auto &starMapTex = streamer.getTex(starMapTexHandle);
	if (starMapTex.isComplete())
	{
		glDepthMask(GL_FALSE);
		pipelineStarMap.bind();
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));
		glBindSampler(1, planetTexSampler);
		glBindTextureUnit(1, starMapTex.getCompleteTextureId());
		sphere.draw(true);
	}
}

void RendererGL::renderPlanetFlares(
	const vector<uint32_t> &flares,
	const DynamicData &data)
{
	glViewport(0,0, windowWidth, windowHeight);
	// Only depth test
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// Blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

	pipelineFlare.bind();

	glBindSampler(1, 0);
	glBindTextureUnit(1, flareTex);

	for (uint32_t i : flares)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		flareModel.draw();
	}
}

void RendererGL::renderTranslucent(
	const vector<uint32_t> &translucentPlanets,
	const DynamicData &data)
{
	// Viewport
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
			ringModel.draw(true);
		}

		// Atmosphere
		if (hasAtmo)
		{
			pipelineAtmo.bind();
			data.planetModel.draw(true);
		}

		// Near rings
		if (hasRing)
		{
			pipelineRingNear.bind();
			ringModel.draw(true);
		}
	}
}

void RendererGL::renderHighpass(const DynamicData &data)
{
	// Viewport
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
		// Viewport
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
		// Viewport
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
			// Viewport
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

void RendererGL::renderTonemap(const DynamicData &data, const bool bloom)
{
	// Viewport
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

	if (bloom) pipelineTonemapBloom.bind();
	else pipelineTonemapNoBloom.bind();

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

void RendererGL::renderSunFlare(
	const DynamicData &data)
{
	// Viewport
	glViewport(0,0, windowWidth, windowHeight);
	// No depth test/write
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_ALWAYS);

	// Blending add
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	pipelineFlare.bind();

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
		data.planetUBOs[sunId].getOffset(),
		sizeof(PlanetDynamicUBO));

	// Bind textures
	glBindSampler(1, 0);
	glBindTextureUnit(1, flareTex);

	flareModel.draw();
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

		data.texLoaded = true;
	}
}

void RendererGL::unloadTextures(const vector<uint32_t> &texUnloadPlanets)
{
	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		auto &data = planetData[i];

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
	}
}

void RendererGL::uploadLoadedTextures()
{
	// Texture uploading
	streamer.update();
}

RendererGL::PlanetDynamicUBO RendererGL::getPlanetUBO(
	const float fovy, const float exp,
	const dvec3 &viewPos, const mat4 &projMat, const mat4 &viewMat,
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

	// Flare
	const vec4 clip = projMat*viewMat*vec4(planetPos,1.0);
	const vec3 screen = vec3(vec2((clip)/clip.w),0.999);
	const bool visible = clip.w > 0;

	mat4 flareMat = mat4(0);
	vec4 flareColor = vec4(0);

	if (visible)
	{
		const float dist = length(planetPos);
		const float radius = params.getBody().getRadius();
		float flareSize = 0.0;
		if (params.isStar())
		{
			const float visibility = getSunVisibility();
			const auto star = params.getStar();
			flareSize = clamp(radius*radius/(dist*dist)*
				star.getBrightness()/star.getFlareAttenuation(),
				star.getFlareMinSize(), star.getFlareMaxSize()*exp)*
				visibility;

			flareColor = vec4(vec3(clamp(
					(dist/radius-star.getFlareFadeInStart())/
					(star.getFlareFadeInEnd()-star.getFlareFadeInStart()),
					0.f,1.f)), 1.f);
		}
		else
		{
			// Smooth transition to detailed planet to flare
			const float fadeIn = clamp((dist/radius-flareMinDistance)/
				(flareOptimalDistance-flareMinDistance),0.f,1.f);
			flareSize = fadeIn*(4.f/(float)windowHeight);

			// Angle between view and light 
			const float phaseAngle = acos(dot(
				(vec3)normalize(state.getPosition()), 
				normalize(planetPos)));
			// Illumination compared to fully lit disk
			const float phase = 
				(1-phaseAngle/pi<float>())*cos(phaseAngle)+
				(1/pi<float>())*sin(phaseAngle);
			
			const float cutDist = dist*0.00008f;
			
			flareColor = vec4(
				clamp(20.f*radius*radius*phase/(cutDist*cutDist),0.f,10.f)*
				params.getBody().getMeanColor()
				,1.0);
		}
		flareMat = translate(mat4(), screen)*
			scale(mat4(), vec3(windowHeight/(float)windowWidth,1.0,0.0)*flareSize);
	}

	const mat3 viewNormalMat = transpose(inverse(mat3(viewMat)));
	

	// Light direction
	const vec3 lightDir = vec3(normalize(-state.getPosition()));

	PlanetDynamicUBO ubo{};
	ubo.modelMat = modelMat;
	ubo.atmoMat = atmoMat;
	ubo.ringFarMat = ringMatrices.first;
	ubo.ringNearMat = ringMatrices.second;
	ubo.flareMat = flareMat;
	ubo.flareColor = flareColor;
	ubo.planetPos = viewMat*vec4(planetPos, 1.0);
	ubo.lightDir = viewMat*vec4(lightDir,0.0);
	ubo.K = params.hasAtmo()
		?params.getAtmo().getScatteringConstant()
		:vec4(0.0);

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

vector<pair<string,uint64_t>> RendererGL::getProfilerTimes()
{
	return profiler.get();
}