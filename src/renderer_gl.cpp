#include "renderer_gl.hpp"
#include "ddsloader.hpp"
#include "mesh.hpp"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <array>
#include <functional>

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

DrawCommand getCommand(
	GLuint vao, 
	GLenum indexType,
	Buffer &vertexBuffer,
	Buffer &indexBuffer,
	const Mesh &mesh)
{
	BufferRange vertexRange = vertexBuffer.assignVertices(
		mesh.getVertices().size(), sizeof(Vertex), mesh.getVertices().data());
	BufferRange indexRange = indexBuffer.assignIndices(
		mesh.getIndices().size(), sizeof(Index), mesh.getIndices().data());

	return DrawCommand(vao, GL_TRIANGLES,
		{
			{0, vertexBuffer.getId(), vertexRange, sizeof(Vertex)}
		},
		{indexType, indexBuffer.getId(), indexRange, mesh.getIndices().size()});
}

void RendererGL::createMeshes()
{
	_vertexBuffer = Buffer(
		Buffer::Usage::STATIC,
		Buffer::Access::WRITE_ONLY);

	_indexBuffer = Buffer(
		Buffer::Usage::STATIC, 
		Buffer::Access::WRITE_ONLY);

	// Fullscreen tri (no vertex data)
	_fullscreenTri = DrawCommand(_vertexArray, GL_TRIANGLES, 3, {});

	// Flare
	const int detail = 8;
	auto flareMesh = generateFlareMesh(detail);

	// Sphere
	const int entityMeridians = 32;
	const int entityRings = 32;
	auto sphereMesh = generateSphere(entityMeridians, entityRings);

	// Load ring models
	map<EntityHandle, Mesh> ringMeshes;
	for (const auto &h: _entityCollection->getBodies())
	{
		const EntityParam param = h.getParam();
		// Rings
		if (param.hasRing())
		{
			const float near = param.getRing().getInnerDistance();
			const float far = param.getRing().getOuterDistance();
			const int ringMeridians = 32;
			ringMeshes[h] = generateRingMesh(ringMeridians, near, far);
		}
	}

	// Shorter mesh->command function
	auto command = bind(getCommand, _vertexArray,
		indexType(), ref(_vertexBuffer), ref(_indexBuffer), std::placeholders::_1);

	// Get commands
	_flareDraw  = command(flareMesh);
	_sphereDraw = command(sphereMesh);

	// Get ring commands
	map<EntityHandle, DrawCommand> ringCommands;
	for (const auto &p : ringMeshes)
	{
		ringCommands[p.first] = command(p.second);
	}

	// Validate & write buffers
	_vertexBuffer.validate();
	_indexBuffer.validate();

	// Assign commands to bodies
	for (const auto &h: _entityCollection->getBodies())
	{
		auto &data = _bodyData[h];
		data.bodyDraw = _sphereDraw;
		auto it = ringCommands.find(h);
		if (it != ringCommands.end()) data.ringDraw = it->second;
	}
}

void RendererGL::createUBO()
{
	// Dynamic UBO buffer assigning
	_uboBuffer = Buffer(
		Buffer::Usage::DYNAMIC, 
		Buffer::Access::WRITE_ONLY);

	_dynamicData.resize(_bufferFrames); // multiple buffering
	for (auto &data : _dynamicData)
	{
		// Scene UBO
		data.sceneUBO = _uboBuffer.assignUBO(sizeof(SceneUBO));
		// Entity UBOs
		for (const auto &h: _entityCollection->getBodies())
		{
			data.bodyUBOs[h] = _uboBuffer.assignUBO(sizeof(BodyUBO));
		}
	}

	_uboBuffer.validate();
}

void RendererGL::init(const InitInfo &info)
{
	this->_entityCollection = info.collection;
	this->_msaaSamples = info.msaa;
	this->_maxTexSize = info.maxTexSize;
	this->_windowWidth = info.windowWidth;
	this->_windowHeight = info.windowHeight;

	// Find the sun
	for (const auto &h : _entityCollection->getBodies())
	{
		if (h.getParam().isStar()) _sun = h;
	}

	this->_bufferFrames = 3; // triple-buffering

	for (const auto &h : _entityCollection->getBodies())
		this->_bodyData[h] = BodyData();

	this->_fences.resize(_bufferFrames);

	createVertexArray();
	createMeshes();
	createUBO();
	createShaders();
	createRendertargets();
	createTextures();
	createFlare();
	createScreenshot();
	createAtmoLookups();
	createRingTextures();

	// Gui init
	Gui::Font f = _gui.loadFont("fonts/Lato-Regular.ttf");
	_mainFontBig = _gui.loadFontSize(f, 40.f);
	_mainFontMedium = _gui.loadFontSize(f, 20.f);
	_gui.init();

	// Streamer init
	_streamer.init(!info.syncTexLoading, 512*512, 200, _maxTexSize);

	// Create starMap texture
	_starMapTexHandle = _streamer.createTex(info.starMapFilename);
	_starMapIntensity = info.starMapIntensity;

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
	glCreateQueries(GL_SAMPLES_PASSED, 2, _sunOcclusionQueries);
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
	_textureAnisotropy = getAnisotropy(requestedAnisotropy);

	// Default textures 
	_diffuseTexDefault = create1PixTex({0,0,0,255});
	_cloudTexDefault = create1PixTex({0,0,0,0});
	_nightTexDefault = create1PixTex({0,0,0,0});
	_specularTexDefault = create1PixTex({0,0,0,0});

	// Samplers
	glCreateSamplers(1, &_bodyTexSampler);
	glSamplerParameterf(_bodyTexSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, _textureAnisotropy);
	glSamplerParameteri(_bodyTexSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(_bodyTexSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(_bodyTexSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(_bodyTexSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glCreateSamplers(1, &_atmoSampler);
	glSamplerParameteri(_atmoSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(_atmoSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(_atmoSampler, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glSamplerParameteri(_atmoSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glCreateSamplers(1, &_ringSampler);
	glSamplerParameteri(_ringSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(_ringSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(_ringSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
}

void RendererGL::createFlare()
{
	DDSLoader flareFile("tex/star_glow.DDS");
	const int mips = flareFile.getMipmapCount();
	glCreateTextures(GL_TEXTURE_2D, 1, &_flareTex);
	glTextureStorage2D(
		_flareTex, 
		mips, 
		DDSFormatToGL(flareFile.getFormat()),
		flareFile.getWidth(0),
		flareFile.getHeight(0));
	glTextureParameteri(_flareTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	for (int i=0;i<mips;++i)
	{
		glCompressedTextureSubImage2D(_flareTex,
			i, 0, 0, flareFile.getWidth(i), flareFile.getHeight(i), 
			DDSFormatToGL(flareFile.getFormat()),
			flareFile.getImageSize(i), flareFile.getImageData(i).data());
	}
}

void RendererGL::createVertexArray()
{
	// Vertex Array Object creation
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &_vertexArray);

	const int VERTEX_ATTRIB_POS     = 0;
	const int VERTEX_ATTRIB_UV      = 1;
	const int VERTEX_ATTRIB_NORMAL  = 2;

	// Position
	glEnableVertexArrayAttrib(_vertexArray, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(_vertexArray, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vertexArray, VERTEX_ATTRIB_POS, 3, GL_FLOAT, false, offsetof(Vertex, position));

	// UVs
	glEnableVertexArrayAttrib(_vertexArray, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(_vertexArray, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vertexArray, VERTEX_ATTRIB_UV, 2, GL_FLOAT, false, offsetof(Vertex, uv));

	// Normals
	glEnableVertexArrayAttrib(_vertexArray, VERTEX_ATTRIB_NORMAL);
	glVertexArrayAttribBinding(_vertexArray, VERTEX_ATTRIB_NORMAL, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vertexArray, VERTEX_ATTRIB_NORMAL, 3, GL_FLOAT, false, offsetof(Vertex, normal));
}

void RendererGL::createRendertargets()
{
	// Depth stencil texture
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &_depthStencilTex);
	glTextureStorage2DMultisample(
		_depthStencilTex, _msaaSamples, GL_DEPTH24_STENCIL8, _windowWidth, _windowHeight, GL_FALSE);

	const GLenum hdrFormat = GL_RGB16F;

	// HDR MSAA Rendertarget
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &_hdrMSRendertarget);
	glTextureStorage2DMultisample(_hdrMSRendertarget, _msaaSamples, hdrFormat,
		_windowWidth, _windowHeight, GL_FALSE);

	// Highpass rendertargets
	glCreateTextures(GL_TEXTURE_2D, 1, &_highpassRendertargets);
	glTextureStorage2D(_highpassRendertargets, _bloomDepth+1, 
		hdrFormat, _windowWidth, _windowHeight);

	// Highpass views
	_highpassViews.resize(_bloomDepth+1);
	glGenTextures(_highpassViews.size(), _highpassViews.data());
	for (size_t i=0;i<_highpassViews.size();++i)
	{
		glTextureView(_highpassViews[i], GL_TEXTURE_2D, _highpassRendertargets, 
			hdrFormat, i, 1, 0, 1);
	}

	// Bloom rendertargets
	glCreateTextures(GL_TEXTURE_2D, 1, &_bloomRendertargets);
	glTextureStorage2D(_bloomRendertargets, _bloomDepth,
		hdrFormat, _windowWidth/2, _windowHeight/2);

	// Bloom views
	_bloomViews.resize(_bloomDepth);
	glGenTextures(_bloomViews.size(), _bloomViews.data());
	for (size_t i=0;i<_bloomViews.size();++i)
	{
		glTextureView(_bloomViews[i], GL_TEXTURE_2D, _bloomRendertargets,
			hdrFormat, i, 1, 0, 1);
	}

	// Sampler
	glCreateSamplers(1, &_rendertargetSampler);
	glSamplerParameteri(_rendertargetSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glSamplerParameteri(_rendertargetSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glSamplerParameteri(_rendertargetSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(_rendertargetSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Framebuffers
	glCreateFramebuffers(1, &_hdrFBO);
	glNamedFramebufferTexture(_hdrFBO, GL_COLOR_ATTACHMENT0, _hdrMSRendertarget, 0);
	glNamedFramebufferTexture(_hdrFBO, GL_DEPTH_STENCIL_ATTACHMENT, _depthStencilTex, 0);

	_highpassFBOs.resize(_bloomDepth+1);
	glCreateFramebuffers(_highpassFBOs.size(), _highpassFBOs.data());
	for (size_t i=0;i<_highpassFBOs.size();++i)
		glNamedFramebufferTexture(_highpassFBOs[i], GL_COLOR_ATTACHMENT0, _highpassViews[i], 0);

	_bloomFBOs.resize(_bloomDepth);
	glCreateFramebuffers(_bloomFBOs.size(), _bloomFBOs.data());
	for (size_t i=0;i<_bloomFBOs.size();++i)
		glNamedFramebufferTexture(_bloomFBOs[i], GL_COLOR_ATTACHMENT0, _bloomViews[i], 0);

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
	const shader bodyVert = {GL_VERTEX_SHADER, "body.vert"};
	const shader starMapVert = {GL_VERTEX_SHADER, "starmap.vert"};
	const shader flareVert = {GL_VERTEX_SHADER, "flare.vert"};
	const shader deferred = {GL_VERTEX_SHADER, "deferred.vert"};

	// Tesc shaders
	const shader bodyTesc = {GL_TESS_CONTROL_SHADER, "body.tesc"};

	// Tese shaders
	const shader bodyTese = {GL_TESS_EVALUATION_SHADER, "body.tese"};
	const shader starMapTese = {GL_TESS_EVALUATION_SHADER, "starmap.tese"};
	
	// Frag shaders
	const shader bodyFrag = {GL_FRAGMENT_SHADER, "body.frag"};
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

	const vector<shader> entityFilenames = {
		bodyVert, bodyTesc, bodyTese, bodyFrag
	};

	_pipelineBodyBare = factory.createPipeline(
		entityFilenames);

	_pipelineBodyAtmo = factory.createPipeline(
		entityFilenames,
		{hasAtmo});

	_pipelineBodyAtmoRing = factory.createPipeline(
		entityFilenames,
		{hasAtmo, hasRing});

	_pipelineStarMap = factory.createPipeline(
		{starMapVert, starMapTese, starMapFrag});

	_pipelineAtmo = factory.createPipeline(
		{bodyVert, bodyTesc, bodyTese, atmo},
		{isAtmo});

	_pipelineSun = factory.createPipeline(
		entityFilenames,
		{isStar});

	const vector<shader> ringFilenames = {
		bodyVert, bodyTesc, bodyTese, ringFrag
	};

	_pipelineRingFar = factory.createPipeline(
		ringFilenames,
		{isFarRing});

	_pipelineRingNear = factory.createPipeline(
		ringFilenames,
		{isNearRing});

	_pipelineHighpass = factory.createPipeline(
		{deferred, highpass});

	_pipelineDownsample = factory.createPipeline(
		{deferred, downsample});

	_pipelineBlurW = factory.createPipeline(
		{deferred, blur},
		{blurW});

	_pipelineBlurH = factory.createPipeline(
		{deferred, blur},
		{blurH});

	_pipelineBloomAdd = factory.createPipeline(
		{deferred, bloomAdd});

	_pipelineFlare = factory.createPipeline(
		{flareVert, flareFrag});

	_pipelineTonemapBloom = factory.createPipeline(
		{deferred, tonemap},
		{bloom});

	_pipelineTonemapNoBloom = factory.createPipeline(
		{deferred, tonemap});
}

void RendererGL::createScreenshot()
{
	// Find best transfer format
	glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_READ_PIXELS_FORMAT,
		1, (GLint*)&_screenBestFormatGL);
	if (_screenBestFormatGL != GL_BGRA) _screenBestFormatGL = GL_RGBA;

	if (_screenBestFormatGL == GL_RGBA) 
		_screenBestFormat = Screenshot::Format::RGBA8;
	else if (_screenBestFormatGL == GL_BGRA) 
		_screenBestFormat = Screenshot::Format::BGRA8;
}

void RendererGL::createAtmoLookups()
{
	for (const auto &h : _entityCollection->getBodies())
	{
		const EntityParam &param = h.getParam();
		auto &data = _bodyData[h];

		// Generate atmospheric scattering lookup texture
		if (param.hasAtmo())
		{
			const int size = 128;
			vector<float> table = param.getAtmo().
				generateLookupTable(size, param.getModel().getRadius());

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
	for (const auto &h : _entityCollection->getBodies())
	{
		const EntityParam &param = h.getParam();
		auto &data = _bodyData[h];

		// Load ring textures
		if (param.hasRing())
		{
			// Load files
			const Ring &ring = param.getRing();
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
	_takeScreen = true;
	_screenFilename = filename;
}

bool testSpherePlane(const vec3 &sphereCenter, float radius, const vec4 &plane)
{
	return dot(sphereCenter, vec3(plane))+plane.w < radius;
}

void RendererGL::render(const RenderInfo &info)
{
	// GUI
	const uint8_t textFade = clamp(info.entityNameFade,0.f,1.f)*255;
	_gui.setText(_mainFontBig, 5, 25, info.focusedEntityName, 
		textFade, textFade, textFade, textFade);
	_gui.setText(_mainFontMedium, 2, _windowHeight-8, info.currentTime, 
		255, 255, 255, 255);

	const float closeBodyMinSizePixels = 1;
	this->_closeBodyMaxDistance =_windowHeight/(closeBodyMinSizePixels*tan(info.fovy/2));
	this->_flareMinDistance = _closeBodyMaxDistance*0.35;
	this->_flareOptimalDistance = _closeBodyMaxDistance*1.0;
	this->_texLoadDistance = _closeBodyMaxDistance*1.4;
	this->_texUnloadDistance = _closeBodyMaxDistance*1.6;

	_profiler.begin("Full frame");

	auto &currentData = _dynamicData[_frameId];

	// Projection and view matrices
	const mat4 projMat = perspective(info.fovy, _windowWidth/(float)_windowHeight, 0.f,1.f);
	const mat4 viewMat = mat4(info.viewDir);

	// Frustum construction
	const float f = tan(info.fovy/2.0);
	const float aspect = _windowWidth/(float)_windowHeight;

	// (Don't need far plane)
	array<vec4, 5> frustum = {
		vec4(0,0,1,0), // near plane
		vec4(normalize(vec3( 1, 0, f*aspect)), 0), // Side planes
		vec4(normalize(vec3(-1, 0, f*aspect)), 0),
		vec4(normalize(vec3(0,  1, f)), 0),
		vec4(normalize(vec3(0, -1, f)), 0)
	};

	// Entity classification
	vector<EntityHandle> closeEntities;
	vector<EntityHandle> translucentEntities;
	vector<EntityHandle> flares;

	vector<EntityHandle> texLoadEntities;
	vector<EntityHandle> texUnloadEntities;

	for (const auto &h : _entityCollection->getBodies())
	{
		auto &data = _bodyData[h];
		const auto &param = h.getParam();
		const auto &state = h.getState();
		const float radius = param.getModel().getRadius();
		const float maxRadius = radius+(param.hasRing()?
			param.getRing().getOuterDistance():0);
		const dvec3 pos = state.getPosition();
		const double dist = distance(info.viewPos, pos)/radius;
		bool focused = count(
			info.focusedEntitiesId.begin(), 
			info.focusedEntitiesId.end(), h)>0;

		if ((focused || dist < _texLoadDistance) && !data.texLoaded)
		{
			texLoadEntities.push_back(h);
		}
		else if (!focused && data.texLoaded && dist > _texUnloadDistance)
		{
			// Textures need to be unloaded
			texUnloadEntities.push_back(h);
		}

		// Frustum test
		const vec3 viewSpacePos = vec3(viewMat*vec4(pos - info.viewPos,1.0));
		bool visible = true;
		for (vec4 plane : frustum)
		{
			visible = visible && testSpherePlane(viewSpacePos, maxRadius, plane);
		}

		// Render entities inside the frustum
		if (visible)
		{
			// Render if is range, always render sun
			if (dist < _closeBodyMaxDistance || param.isStar())
			{
				closeEntities.push_back(h);
				// Entity atmospheres
				if (param.hasAtmo() || param.hasRing())
				{
					translucentEntities.push_back(h);
				}
			}
		}

		if (dist > _flareMinDistance && !param.isStar())
		{
			flares.push_back(h); 
		}
	}

	// Manage stream textures
	_profiler.begin("Texture creation/deletion");
	loadTextures(texLoadEntities);
	unloadTextures(texUnloadEntities);
	_profiler.end();
	_profiler.begin("Texture updating");
	uploadLoadedTextures();
	_profiler.end();

	const float exp = pow(2, info.exposure);

	// Scene uniform update
	SceneUBO sceneUBO{};
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.starMapMat = viewMat*scale(mat4(), vec3(-1));
	sceneUBO.starMapIntensity = _starMapIntensity;

	sceneUBO.ambientColor = info.ambientColor;
	sceneUBO.exposure = exp;
	sceneUBO.logDepthFarPlane = (1.0/log2(_logDepthC*_logDepthFarPlane + 1.0));
	sceneUBO.logDepthC = _logDepthC;

	// Entity uniform update
	map<EntityHandle, BodyUBO> bodyUBOs;
	for (const auto &h : _entityCollection->getBodies())
	{
		bodyUBOs[h] = getBodyUBO(info.fovy, exp, info.viewPos, projMat, viewMat, 
			h.getState(), h.getParam(), _bodyData[h]);
	}

	// Dynamic data upload
	_profiler.begin("Sync wait");
	_fences[_frameId].waitClient();
	_profiler.end();

	_uboBuffer.write(currentData.sceneUBO, &sceneUBO);
	for (const auto &h : _entityCollection->getBodies())
	{
		_uboBuffer.write(currentData.bodyUBOs[h], &bodyUBOs[h]);
	}

	auto closerFun = [&](const EntityHandle &i, const EntityHandle &j)
	{
		const float distI = distance(i.getState().getPosition(), info.viewPos);
		const float distJ = distance(j.getState().getPosition(), info.viewPos);
		return distI < distJ;
	};

	auto fartherFun = [&](const EntityHandle &i, const EntityHandle &j)
	{
		return !closerFun(i,j);
	};

	// Entity sorting from front to back
	sort(closeEntities.begin(), closeEntities.end(), closerFun);

	// Atmosphere sorting from back to front
	sort(translucentEntities.begin(), translucentEntities.end(), fartherFun);

	if (info.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	_profiler.begin("Bodies");
	renderHdr(closeEntities, currentData);
	_profiler.end();
	_profiler.begin("Flares");
	renderEntityFlares(flares, currentData);
	_profiler.end();
	_profiler.begin("Translucent objects");
	renderTranslucent(translucentEntities, currentData);
	_profiler.end();
	if (info.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (info.bloom)
	{
		_profiler.begin("Highpass");
		renderHighpass(currentData);
		_profiler.end();
		_profiler.begin("Downsample");
		renderDownsample(currentData);
		_profiler.end();
		_profiler.begin("Bloom");
		renderBloom(currentData);
		_profiler.end();
	}
	_profiler.begin("Tonemapping");
	renderTonemap(currentData, info.bloom);
	_profiler.end();
	_profiler.begin("Sun Flare");
	renderSunFlare(currentData);
	_profiler.end();
	_profiler.begin("GUI");
	renderGui();
	_profiler.end();

	if (_takeScreen)
	{
		saveScreenshot();
		_takeScreen = false;
	}

	_profiler.end();

	_fences[_frameId].lock();

	_frameId = (_frameId+1)%_bufferFrames;
}

float RendererGL::getSunVisibility()
{
	for (int i=0;i<2;++i)
	{
		glGetQueryObjectiv(_sunOcclusionQueries[i], GL_QUERY_RESULT_NO_WAIT, 
			&_occlusionQueryResults[i]);
	}

	return _occlusionQueryResults[0]/(float)std::max(1,_occlusionQueryResults[1]);
}

void RendererGL::saveScreenshot()
{
	// Cancel if already saving screenshot
	if (_screenshot.isSaving()) return;
	// Read screen
	vector<uint8_t> buffer(4*_windowWidth*_windowHeight);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, _windowWidth, _windowHeight, 
		_screenBestFormatGL, GL_UNSIGNED_BYTE, buffer.data());
	_screenshot.save(
		_screenFilename,
		_windowWidth, _windowHeight, 
		_screenBestFormat, buffer);
}

void RendererGL::renderHdr(
	const vector<EntityHandle> &closeEntities,
	const DynamicData &ddata)
{
	// Viewport
	glViewport(0,0, _windowWidth, _windowHeight);

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
	glInvalidateNamedFramebufferData(_hdrFBO, 
		attachments.size(), attachments.data());
	// Clearing
	vector<float> clearColor = {0.f,0.f,0.f,0.f};
	vector<float> clearDepth = {1.f};
	glClearNamedFramebufferfv(_hdrFBO, GL_COLOR, 0, clearColor.data());
	glClearNamedFramebufferfv(_hdrFBO, GL_DEPTH, 0, clearDepth.data());
	// Bind FBO for rendering
	glBindFramebuffer(GL_FRAMEBUFFER, _hdrFBO);

	// Entity rendering
	for (const auto &h : closeEntities)
	{
		auto &data = _bodyData[h];
		const EntityParam &param = h.getParam();
		const bool star = param.isStar();
		const bool hasAtmo = param.hasAtmo();
		const bool hasRing = param.hasRing();
		if (star) _pipelineSun.bind();
		else if (hasAtmo)
		{
			if (hasRing) _pipelineBodyAtmoRing.bind();
			else _pipelineBodyAtmo.bind();
		}
		else _pipelineBodyBare.bind();

		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneUBO));

		// Bind entity UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, _uboBuffer.getId(),
			ddata.bodyUBOs.at(h).getOffset(),
			sizeof(BodyUBO));

		// Bind samplers
		const vector<GLuint> samplers = {
			_bodyTexSampler,
			_bodyTexSampler,
			_bodyTexSampler,
			_bodyTexSampler,
			_atmoSampler,
			_ringSampler
		};
		// Bind textures
		const vector<GLuint> texs = {
			_streamer.getTex(data.diffuse).getCompleteTextureId(_diffuseTexDefault),
			_streamer.getTex(data.cloud).getCompleteTextureId(_cloudTexDefault),
			_streamer.getTex(data.night).getCompleteTextureId(_nightTexDefault),
			_streamer.getTex(data.specular).getCompleteTextureId(_specularTexDefault),
			data.atmoLookupTable,
			data.ringTex2,
		};

		glBindSamplers(2, samplers.size(), samplers.data());
		glBindTextures(2, texs.size(), texs.data());

		if (star) glBeginQuery(GL_SAMPLES_PASSED, _sunOcclusionQueries[0]);
		data.bodyDraw.draw(true);
		if (star)
		{
			glEndQuery(GL_SAMPLES_PASSED);
			glDepthFunc(GL_ALWAYS);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDepthMask(GL_FALSE);
			glBeginQuery(GL_SAMPLES_PASSED, _sunOcclusionQueries[1]);
			data.bodyDraw.draw(true);
			glEndQuery(GL_SAMPLES_PASSED);
			glDepthFunc(GL_LESS);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);
		}
	}

	// Star map rendering
	// Don't render if star map texture not loaded
	auto &starMapTex = _streamer.getTex(_starMapTexHandle);
	if (starMapTex.isComplete())
	{
		glDepthMask(GL_FALSE);
		_pipelineStarMap.bind();
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneUBO));
		glBindSampler(1, _bodyTexSampler);
		glBindTextureUnit(1, starMapTex.getCompleteTextureId());
		_sphereDraw.draw(true);
	}
}

void RendererGL::renderEntityFlares(
	const vector<EntityHandle> &flares,
	const DynamicData &data)
{
	glViewport(0,0, _windowWidth, _windowHeight);
	// Only depth test
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// Blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, _hdrFBO);

	_pipelineFlare.bind();

	glBindSampler(1, 0);
	glBindTextureUnit(1, _flareTex);

	for (const auto &h : flares)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			data.bodyUBOs.at(h).getOffset(),
			sizeof(BodyUBO));

		_flareDraw.draw();
	}
}

void RendererGL::renderTranslucent(
	const vector<EntityHandle> &translucentEntities,
	const DynamicData &data)
{
	// Viewport
	glViewport(0,0, _windowWidth, _windowHeight);
	// Only depth test
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// Blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_SRC_ALPHA);

	glBindFramebuffer(GL_FRAMEBUFFER, _hdrFBO);

	for (const auto &h : translucentEntities)
	{
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneUBO));
		// Bind Entity UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, _uboBuffer.getId(),
			data.bodyUBOs.at(h).getOffset(),
			sizeof(BodyUBO));

		const bool hasRing = h.getParam().hasRing();
		const bool hasAtmo = h.getParam().hasAtmo();

		const auto &data = _bodyData[h];

		const vector<GLuint> samplers = {
			_atmoSampler,
			_ringSampler,
			_ringSampler
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
			_pipelineRingFar.bind();
			data.ringDraw.draw(true);
		}

		// Atmosphere
		if (hasAtmo)
		{
			_pipelineAtmo.bind();
			data.bodyDraw.draw(true);
		}

		// Near rings
		if (hasRing)
		{
			_pipelineRingNear.bind();
			data.ringDraw.draw(true);
		}
	}
}

void RendererGL::renderHighpass(const DynamicData &data)
{
	// Viewport
	glViewport(0,0, _windowWidth, _windowHeight);

	// No depth test/write
	glDepthFunc(GL_ALWAYS);
	glDepthMask(GL_FALSE);

	// No blending
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);

	const vector<GLuint> attachments = {GL_COLOR_ATTACHMENT0};
	glInvalidateNamedFramebufferData(_highpassFBOs[0], attachments.size(), attachments.data());

	glBindFramebuffer(GL_FRAMEBUFFER, _highpassFBOs[0]);

	_pipelineHighpass.bind();

	const vector<GLuint> samplers = {_rendertargetSampler};
	const vector<GLuint> texs = {_hdrMSRendertarget};
	glBindSamplers(1, samplers.size(), samplers.data());
	glBindTextures(1, texs.size(), texs.data());

	// Bind scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneUBO));

	_fullscreenTri.draw();
}
	
void RendererGL::renderDownsample(const DynamicData &data)
{
	const vector<GLenum> invalidateAttach = {GL_COLOR_ATTACHMENT0};
	glBindSampler(0, _rendertargetSampler);
	_pipelineDownsample.bind();
	for (int i=0;i<_bloomDepth;++i)
	{
		// Viewport
		glViewport(0,0, _windowWidth>>(i+1), _windowHeight>>(i+1));
		glInvalidateNamedFramebufferData(_highpassFBOs[i+1], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, _highpassFBOs[i+1]);
		glBindTextureUnit(0, _highpassViews[i]);
		_fullscreenTri.draw();
	}
}

void RendererGL::renderBloom(const DynamicData &data)
{
	const vector<GLenum> invalidateAttach = {GL_COLOR_ATTACHMENT0};
	glCopyImageSubData(
		_highpassViews[_bloomDepth], GL_TEXTURE_2D, 0,
		0, 0, 0, 
		_bloomViews[_bloomDepth-1], GL_TEXTURE_2D, 0,
		0, 0, 0,
		mipmapSize(_windowWidth,  _bloomDepth),
		mipmapSize(_windowHeight, _bloomDepth),
		1);

	const vector<GLuint> samplers = {_rendertargetSampler, _rendertargetSampler};
	glBindSamplers(0, samplers.size(), samplers.data());
	for (int i=_bloomDepth;i>=1;--i)
	{
		// Viewport
		glViewport(0,0, _windowWidth>>i, _windowHeight>>i);
		// Blur horizontally
		_pipelineBlurW.bind();
		glInvalidateNamedFramebufferData(_highpassFBOs[i], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, _highpassFBOs[i]);
		glBindTextureUnit(0, _bloomViews[i-1]);
		_fullscreenTri.draw();

		// Blur vertically
		_pipelineBlurH.bind();
		glInvalidateNamedFramebufferData(_bloomFBOs[i-1], invalidateAttach.size(), invalidateAttach.data());
		glBindFramebuffer(GL_FRAMEBUFFER, _bloomFBOs[i-1]);
		glBindTextureUnit(0, _highpassViews[i]);
		_fullscreenTri.draw();

		// Add blur with higher res
		if (i>1)
		{
			// Viewport
			glViewport(0,0, _windowWidth>>(i-1), _windowHeight>>(i-1));
			_pipelineBloomAdd.bind();
			glInvalidateNamedFramebufferData(_bloomFBOs[i-2], invalidateAttach.size(), invalidateAttach.data());
			glBindFramebuffer(GL_FRAMEBUFFER, _bloomFBOs[i-2]);
			const vector<GLuint> texs = {_bloomViews[i-1], _highpassViews[i-1]};
			glBindTextures(0, texs.size(), texs.data());
			_fullscreenTri.draw();
		}
	}
}

void RendererGL::renderTonemap(const DynamicData &data, const bool bloom)
{
	// Viewport
	glViewport(0,0, _windowWidth, _windowHeight);
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

	if (bloom) _pipelineTonemapBloom.bind();
	else _pipelineTonemapNoBloom.bind();

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneUBO));

	// Bind image after bloom is done
	const vector<GLuint> samplers = {_rendertargetSampler, _rendertargetSampler};
	const vector<GLuint> texs = {_hdrMSRendertarget, _bloomViews[0]};
	glBindSamplers(1, samplers.size(), samplers.data());
	glBindTextures(1, texs.size(), texs.data());

	_fullscreenTri.draw();
}

void RendererGL::renderSunFlare(
	const DynamicData &data)
{
	// Viewport
	glViewport(0,0, _windowWidth, _windowHeight);
	// No depth test/write
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_ALWAYS);

	// Blending add
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	_pipelineFlare.bind();

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, _uboBuffer.getId(),
		data.bodyUBOs.at(_sun).getOffset(),
		sizeof(BodyUBO));

	// Bind textures
	glBindSampler(1, 0);
	glBindTextureUnit(1, _flareTex);

	_flareDraw.draw();
}

void RendererGL::renderGui()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_gui.display(_windowWidth, _windowHeight);
}

void RendererGL::loadTextures(const vector<EntityHandle> &texLoadEntities)
{
	// Texture loading
	for (const auto &h : texLoadEntities)
	{
		const EntityParam param = h.getParam();
		auto &data = _bodyData[h];
		// Textures & samplers
		data.diffuse = _streamer.createTex(param.getModel().getDiffuseFilename());
		if (param.hasClouds())
			data.cloud = _streamer.createTex(param.getClouds().getFilename());
		if (param.hasNight())
			data.night = _streamer.createTex(param.getNight().getFilename());
		if (param.hasSpecular())
			data.specular = _streamer.createTex(param.getSpecular().getFilename());

		data.texLoaded = true;
	}
}

void RendererGL::unloadTextures(const vector<EntityHandle> &texUnloadEntities)
{
	// Texture unloading
	for (const auto &h : texUnloadEntities)
	{
		auto &data = _bodyData[h];

		// Reset variables
		data.texLoaded = false;
		_streamer.deleteTex(data.diffuse);
		_streamer.deleteTex(data.cloud);
		_streamer.deleteTex(data.night);
		_streamer.deleteTex(data.specular);
		data.diffuse = 0;
		data.cloud = 0;
		data.night = 0;
		data.specular = 0;
	}
}

void RendererGL::uploadLoadedTextures()
{
	// Texture uploading
	_streamer.update();
}

RendererGL::BodyUBO RendererGL::getBodyUBO(
	const float fovy, const float exp,
	const dvec3 &viewPos, const mat4 &projMat, const mat4 &viewMat,
	const EntityState &state, const EntityParam &params,
	const BodyData &data)
{
	const vec3 bodyPos = state.getPosition() - viewPos;

	// Entity rotation
	const vec3 north = vec3(0,0,1);
	const vec3 rotAxis = params.getModel().getRotationAxis();
	const quat q = rotate(quat(), 
		(float)acos(dot(north, rotAxis)), 
		cross(north, rotAxis))*
		rotate(quat(), state.getRotationAngle(), north);

	// Model matrix
	const mat4 modelMat = 
		translate(mat4(), bodyPos)*
		mat4_cast(q)*
		scale(mat4(), vec3(params.getModel().getRadius()));

	// Atmosphere matrix
	const mat4 atmoMat = (params.hasAtmo())?
		translate(mat4(), bodyPos)*
		mat4_cast(q)*
		scale(mat4(), -vec3(params.getModel().getRadius()+params.getAtmo().getMaxHeight())):
		mat4(0.0);

	// Ring matrices
	pair<mat4, mat4> ringMatrices = [&]{
		if (!params.hasRing()) return make_pair(mat4(0),mat4(0));

		const vec3 towards = normalize(bodyPos);
		const vec3 up = params.getRing().getNormal();
		const float sideflip = (dot(towards, up)<0)?1.f:-1.f;
		const vec3 right = normalize(cross(towards, up));
		const vec3 newTowards = cross(right, up);

		const mat4 lookAtFar = mat4(mat3(sideflip*right, -newTowards, up));
		const mat4 lookAtNear = mat4(mat3(-sideflip*right, newTowards, up));

		const mat4 ringFarMat = 
			translate(mat4(), bodyPos)*
			lookAtFar;

		const mat4 ringNearMat =
			translate(mat4(), bodyPos)*
			lookAtNear;

		return make_pair(ringFarMat, ringNearMat);
	}();

	// Flare
	const vec4 clip = projMat*viewMat*vec4(bodyPos,1.0);
	const vec3 screen = vec3(vec2((clip)/clip.w),0.999);
	const bool visible = clip.w > 0;

	mat4 flareMat = mat4(0);
	vec4 flareColor = vec4(0);

	if (visible)
	{
		const float dist = length(bodyPos);
		const float radius = params.getModel().getRadius();
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
			// Smooth transition to detailed entity to flare
			const float fadeIn = clamp((dist/radius-_flareMinDistance)/
				(_flareOptimalDistance-_flareMinDistance),0.f,1.f);
			flareSize = fadeIn*(4.f/(float)_windowHeight);

			// Angle between view and light 
			const float phaseAngle = acos(dot(
				(vec3)normalize(state.getPosition()), 
				normalize(bodyPos)));
			// Illumination compared to fully lit disk
			const float phase = 
				(1-phaseAngle/pi<float>())*cos(phaseAngle)+
				(1/pi<float>())*sin(phaseAngle);
			
			const float cutDist = dist*0.00008f;
			
			flareColor = vec4(
				clamp(20.f*radius*radius*phase/(cutDist*cutDist),0.f,10.f)*
				params.getModel().getMeanColor()
				,1.0);
		}
		flareMat = translate(mat4(), screen)*
			scale(mat4(), vec3(_windowHeight/(float)_windowWidth,1.0,0.0)*flareSize);
	}

	const mat3 viewNormalMat = transpose(inverse(mat3(viewMat)));
	

	// Light direction
	const vec3 lightDir = vec3(normalize(-state.getPosition()));

	BodyUBO ubo{};
	ubo.modelMat = modelMat;
	ubo.atmoMat = atmoMat;
	ubo.ringFarMat = ringMatrices.first;
	ubo.ringNearMat = ringMatrices.second;
	ubo.flareMat = flareMat;
	ubo.flareColor = flareColor;
	ubo.bodyPos = viewMat*vec4(bodyPos, 1.0);
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
	ubo.radius = params.getModel().getRadius();
	ubo.atmoHeight = params.hasAtmo()?params.getAtmo().getMaxHeight():0.0;

	return ubo;
}

vector<pair<string,uint64_t>> RendererGL::getProfilerTimes()
{
	return _profiler.get();
}