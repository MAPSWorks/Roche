#pragma once

#include "renderer.hpp"
#include "graphics_api.hpp"
#include "fence.hpp"
#include "ddsloader.hpp"
#include "gl_util.hpp"
#include "gl_profiler.hpp"
#include "dds_stream.hpp"
#include "screenshot.hpp"
#include "shader_pipeline.hpp"
#include "gui_gl.hpp"

#include <vector>
#include <map>
#include <memory>
#include <utility>

/**
 * OpenGL implementation of Renderer
 * @see Renderer
 */
class RendererGL : public Renderer
{
public:
	RendererGL() = default;
	void windowHints() override;
	void init(const InitInfo &info) override;
	void render(const RenderInfo &info) override;
	void takeScreenshot(const std::string &filename) override;
	void destroy() override;

	std::vector<std::pair<std::string,uint64_t>> getProfilerTimes() override;
private:
	/// Buffer ranges of dynamic data
	struct DynamicData
	{
		BufferRange sceneUBO;
		std::map<EntityHandle, BufferRange> bodyUBOs;
	};

	/// Dynamic parameters for the scene to be loaded in a UBO
	struct SceneUBO
	{
		/// Projection matrix
		glm::mat4 projMat;
		/// View matrix
		glm::mat4 viewMat;
		/// Star map matrix
		glm::mat4 starMapMat;
		/// Star map intensity
		float starMapIntensity;
		/// Ambient light coefficient
		float ambientColor;
		/// Exposure factor
		float exposure;
		/// Far plane coefficient for log depth
		float logDepthFarPlane;
		/// C precision balance coefficient for log depth
		float logDepthC;
	};

	/// Dynamic parameters for a single body to be loaded in a UBO
	struct BodyUBO
	{
		/// Model matrix of the body
		glm::mat4 modelMat;
		/// Model matrix of the atmosphere
		glm::mat4 atmoMat;
		/// Model matrix of the far half ring
		glm::mat4 ringFarMat;
		/// Model matrix of the near half ring
		glm::mat4 ringNearMat;
		/// flare matrix
		glm::mat4 flareMat;
		/// flare color
		glm::vec4 flareColor;
		/// Entity position in view space
		glm::vec4 bodyPos;
		/// Light direction in view space
		glm::vec4 lightDir;
		/// Scattering constants
		glm::vec4 K;
		/// Specular reflection parameters (xyz color w hardness) of mask 0
		glm::vec4 mask0ColorHardness;
		/// Specular reflection parameters (xyz color w hardness) of mask 1
		glm::vec4 mask1ColorHardness;
		/// Ring plane normal vector
		glm::vec4 ringNormal;
		/// Ring inner edge distance from center of body
		float ringInner;
		/// Ring outer edge distance from center of body
		float ringOuter;
		/// Brightness coefficient if body is a star
		float starBrightness;
		/// Rate of cloud displacement
		float cloudDisp;
		/// Intensity factor of night emission texture
		float nightTexIntensity;
		/// Radius of body
		float radius;
		/// Atmospheric height
		float atmoHeight;
	};

	/// Generates the vertex and index data and fill the static VBOs
	void createMeshes();
	/// Creates the UBO buffers and assigns buffer ranges for UBO structures
	void createUBO();
	/// Creates default textures, flare textures and samplers
	void createTextures();
	/// Creates flare textures
	void createFlare();
	/// Create VAOs
	void createVertexArray();
	/// Create FBOs and attachments
	void createRendertargets();
	/// Create and load shaders
	void createShaders();
	/// Create Screenshot object
	void createScreenshot();
	/// Create atmo lookup textures for all entities with atmosphere
	void createAtmoLookups();
	/// Create ring textures for all entities with rings
	void createRingTextures();

	/** Renders opaque parts of detailed entities to HDR rendertarget
	 * @param closeEntities id of entities to render
	 * @param buffer ranges to use for rendering
	 */
	void renderHdr(
		const std::vector<EntityHandle> &closeEntities, 
		const DynamicData &data);
	/** Renders flares to HDR rendertarget
	 * @param flares id of entities to render as flares
	 * @param buffer ranges to use for rendering
	 */
	void renderEntityFlares(
		const std::vector<EntityHandle> &flares,
		const DynamicData &data);
	/** Renders translucent parts of detailed entities to HDR rendertarget
	 * @param translucentEntities id of entities to render
	 * @param buffer ranges to use for rendering
	 */
	void renderTranslucent(
		const std::vector<EntityHandle> &translucentEntities,
		const DynamicData &data);
	/** Generates highpass from HDR rendertarget
	 * @param data buffer ranges to use for rendering
	 */
	void renderHighpass(const DynamicData &data);
	/** Generates multiple smaller highpass rendertargets
	 * @param data buffer ranges to use for rendering
	 */
	void renderDownsample(const DynamicData &data);
	/** Generates bloom rendertarget from highpass targets
	 * @param data buffer ranges to use for rendering
	 */
	void renderBloom(const DynamicData &data);
	/** Tonemaps and resolves HDR rendertarget to screen
	 * @param data buffer ranges to use for rendering
	 * @param bloom whether to use bloom or not
	 */
	void renderTonemap(const DynamicData &data, bool bloom);
	/** Renders sun flare on top of the screen
	 * @param data buffer ranges to use for rendering
	 */
	void renderSunFlare(const DynamicData &data);
	/** Renders Gui elements */
	void renderGui();
	/** Sets the textures of entities to be loaded asynchronouly
	 * @param entities entities whose textures to load
	 */
	void loadTextures(const std::vector<EntityHandle> &entities);
	/** Sets the textures of entities to be unloaded asynchronouly
	 * @param entities palanets whose textures to unload
	 */
	void unloadTextures(const std::vector<EntityHandle> &entities);
	/// Sets loaded textures to be uploaded to the GL
	void uploadLoadedTextures();

	/// Saves the current screen to a file
	void saveScreenshot();

	/// Measures time between GL calls
	GPUProfilerGL _profiler;

	// Screenshot info
	/// Signals Screenshot object to save
	bool _takeScreen = false;
	/// Screenshot filename
	std::string _screenFilename = "";
	/// Preferred GL screenshot format
	Screenshot::Format _screenBestFormat = Screenshot::Format::RGBA8;
	/// Preferred GL screenshot format (GL enum)
	GLenum _screenBestFormatGL = GL_RGBA;
	/// Screenshot object
	Screenshot _screenshot;

	/// Samples per pixel of HDR rendertarget
	int _msaaSamples = 1;
	/// Max texture width/height to be loaded and displayed (-1 means no limit)
	int _maxTexSize = -1;
	/// Window width in pixels
	int _windowWidth = 1;
	/// Window height in pixels
	int _windowHeight = 1;
	/// Far plane distance
	float _logDepthFarPlane = 5e9;
	/// Logarithmic depth balance coefficient
	float _logDepthC = 1.0;

	// Constants for distance based loading
	/// Max distance at which a body is considered 'close' (detailed render)
	float _closeBodyMaxDistance;
	/// Min distance at which a body is considered 'far' (flare render)
	float _flareMinDistance;
	/// Optimal distance at which a body is considered 'far' (no flare fade in)
	float _flareOptimalDistance;
	/// Distance at which a body's textures will be loaded
	float _texLoadDistance;
	/// Distance at which a body's textures will be unloaded
	float _texUnloadDistance;

	/// Buffer containing vertex data
	Buffer _vertexBuffer;
	/// Buffer containing index data
	Buffer _indexBuffer;
	/// Buffer containing UBO data
	Buffer _uboBuffer;
	
	/// Buffer ranges of each frame (multiple buffering)
	std::vector<DynamicData> _dynamicData;
	/// Data writing fences of each frame (multiple buffering)
	std::vector<Fence> _fences;

	/// Vertex Array Object of entities, flares and deferred tris
	GLuint _vertexArray;

	// Rendertargets : 
	/// Depth stencil attachment of HDR rendertarget
	GLuint _depthStencilTex;
	/// HDR MS rendertarget
	GLuint _hdrMSRendertarget;
	/// Highpass rendertargets (multiple mips)
	GLuint _highpassRendertargets;
	/// Bloom rendertargets (multiple mips)
	GLuint _bloomRendertargets;

	/// Number of bloom downsample steps (bigger blurs)
	int _bloomDepth = 8;
	/// Texture views to individual highpass rendertarget mipmaps
	std::vector<GLuint> _highpassViews;
	/// Texture views to individual bloom rendertarget mipmaps
	std::vector<GLuint> _bloomViews;

	/// Rendertarget sampler
	GLuint _rendertargetSampler;

	// FBOs
	/// HDR FBO
	GLuint _hdrFBO;
	/// Highpass FBOs
	std::vector<GLuint> _highpassFBOs;
	/// Bloom FBOs
	std::vector<GLuint> _bloomFBOs;

	// Pipelines
	/// Body without atmo
	ShaderPipeline _pipelineBodyBare;
	/// Body with atmo
	ShaderPipeline _pipelineBodyAtmo;
	/// Body with atmo and rings
	ShaderPipeline _pipelineBodyAtmoRing;
	/// Star map
	ShaderPipeline _pipelineStarMap;
	/// Atmosphere
	ShaderPipeline _pipelineAtmo;
	/// Star
	ShaderPipeline _pipelineSun;
	/// Far half ring
	ShaderPipeline _pipelineRingFar;
	/// Near half ring
	ShaderPipeline _pipelineRingNear;
	/// Highpass for bloom
	ShaderPipeline _pipelineHighpass;
	/// Downsample for bloom
	ShaderPipeline _pipelineDownsample;
	/// Horizontal blur for bloom
	ShaderPipeline _pipelineBlurW;
	/// Vertical blur for bloom
	ShaderPipeline _pipelineBlurH;
	/// Bloom reconstitution
	ShaderPipeline _pipelineBloomAdd;
	/// Flares
	ShaderPipeline _pipelineFlare;
	/// Tonemap and resolve with bloom
	ShaderPipeline _pipelineTonemapBloom;
	/// Tonemap and resolve without bloom
	ShaderPipeline _pipelineTonemapNoBloom;

	// Multiple buffering
	/// Number of frame modulo bufferFrames
	uint32_t _frameId;
	/// Number of frames to multi-buffer
	uint32_t _bufferFrames;

	const EntityCollection* _entityCollection;

	/// Entity data only for rendering
	struct BodyData
	{
		/// Body draw command
		DrawCommand bodyDraw;
		/// Ring draw command
		DrawCommand ringDraw;
		/// Whether the textures have been loaded or onot
		bool texLoaded = false;

		/// Diffuse texture
		DDSStreamer::Handle diffuse{};
		/// Cloud texture
		DDSStreamer::Handle cloud{};
		/// Emissive night texture
		DDSStreamer::Handle night{};
		/// Specular mask texture
		DDSStreamer::Handle specular{};

		/// Atmospheric lookup table GL texture
		GLuint atmoLookupTable = 0;
		/// Ring texture 1
		GLuint ringTex1 = 0;
		/// Ring texture 2
		GLuint ringTex2 = 0;
		BodyData() = default;
	};

	/** Fills DynamicEntityUBO structure from entity parameters and state
	 * @param viewPos World space eye position
	 * @param viewMat View matrix (not accounting translation)
	 * @param state Dynamic entity state
	 * @param params Fixed entity parameters
	 * @param data Entity data for rendering
	 * @returns UBO data
	 */
	BodyUBO getBodyUBO(
		float fovy,
		float exp,
		const glm::dvec3 &viewPos,
		const glm::mat4 &projMat, 
		const glm::mat4 &viewMat,
		const EntityState &state, 
		const EntityParam &params, 
		const BodyData &data);

	float getSunVisibility();

	/// Rendering data for all bodies
	std::map<EntityHandle, BodyData> _bodyData;
	/// Index of sun in main entity collection
	EntityHandle _sun;

	GLuint _sunOcclusionQueries[2] = {0, 0};
	int _occlusionQueryResults[2] = {0, 1};

	DDSStreamer::Handle _starMapTexHandle{};
	float _starMapIntensity = 1.0;

	// Textures
	/// Default diffuse texture
	GLuint _diffuseTexDefault;
	/// Default cloud texture
	GLuint _cloudTexDefault;
	/// Default emissive night texture
	GLuint _nightTexDefault;
	/// Default specular mask texture
	GLuint _specularTexDefault;

	/// Flare texture (white dot)
	GLuint _flareTex;

	// Samplers
	/// Sampler for body textures
	GLuint _bodyTexSampler;
	/// Sampler for atmospheric lookup table
	GLuint _atmoSampler;
	/// Sampler for ring textures
	GLuint _ringSampler;

	/// Max anisotropy for texture sampling
	float _textureAnisotropy;

	// Meshes
	/// Sphere draw command (for celestial bodies and atmospheres)
	DrawCommand _sphereDraw;
	/// Flare mesh (Circle)
	DrawCommand _flareDraw;
	/// Fullscreen triangle for covering the whole screen
	DrawCommand _fullscreenTri;

	/// Stream texture loader
	DDSStreamer _streamer;

	GuiGL _gui;
	Gui::FontSize _mainFontBig;
	Gui::FontSize _mainFontMedium;
};