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

#include <condition_variable>
#include <thread>
#include <mutex>
#include <memory>
#include <queue>
#include <map>
#include <stack>
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

		std::vector<BufferRange> planetUBOs;
	};

	/// Dynamic parameters for the scene to be loaded in a UBO
	struct SceneDynamicUBO
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

	/// Dynamic parameters for a single planet to be loaded in a UBO
	struct PlanetDynamicUBO
	{
		/// Model matrix of the planet
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
		/// Planet position in view space
		glm::vec4 planetPos;
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
		/// Ring inner edge distance from center of planet
		float ringInner;
		/// Ring outer edge distance from center of planet
		float ringOuter;
		/// Brightness coefficient if body is a star
		float starBrightness;
		/// Rate of cloud displacement
		float cloudDisp;
		/// Intensity factor of night emission texture
		float nightTexIntensity;
		/// Radius of planet
		float radius;
		/// Atmospheric height
		float atmoHeight;
	};

	/// Generates the vertex and index data and fill the static VBOs
	void createModels();
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
	/// Create atmo lookup textures for all planets with atmosphere
	void createAtmoLookups();
	/// Create ring textures for all planets with rings
	void createRingTextures();

	/** Renders opaque parts of detailed planets to HDR rendertarget
	 * @param closePlanets id of planets to render
	 * @param buffer ranges to use for rendering
	 */
	void renderHdr(
		const std::vector<uint32_t> &closePlanets, 
		const DynamicData &data);
	/** Renders flares to HDR rendertarget
	 * @param flares id of planets to render as flares
	 * @param buffer ranges to use for rendering
	 */
	void renderPlanetFlares(
		const std::vector<uint32_t> &flares,
		const DynamicData &data);
	/** Renders translucent parts of detailed planets to HDR rendertarget
	 * @param translucentPlanets id of planets to render
	 * @param buffer ranges to use for rendering
	 */
	void renderTranslucent(
		const std::vector<uint32_t> &translucentPlanets,
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
	 *
	 */
	void renderSunFlare(const DynamicData &data);
	/** Sets the textures of planets to be loaded asynchronouly
	 * @param planets planets whose textures to load
	 */
	void loadTextures(const std::vector<uint32_t> &planets);
	/** Sets the textures of planets to be unloaded asynchronouly
	 * @param planets palanets whose textures to unload
	 */
	void unloadTextures(const std::vector<uint32_t> &planets);
	/// Sets loaded textures to be uploaded to the GL
	void uploadLoadedTextures();

	/// Saves the current screen to a file
	void saveScreenshot();

	/// Measures time between GL calls
	GPUProfilerGL profiler;

	// Screenshot info
	/// Signals Screenshot object to save
	bool takeScreen = false;
	/// Screenshot filename
	std::string screenFilename = "";
	/// Preferred GL screenshot format
	Screenshot::Format screenBestFormat = Screenshot::Format::RGBA8;
	/// Preferred GL screenshot format (GL enum)
	GLenum screenBestFormatGL = GL_RGBA;
	/// Screenshot object
	Screenshot screenshot;

	/// Number of planets in the scene
	uint32_t planetCount = 0;
	/// Samples per pixel of HDR rendertarget
	int msaaSamples = 1;
	/// Max texture width/height to be loaded and displayed (-1 means no limit)
	int maxTexSize = -1;
	/// Window width in pixels
	int windowWidth = 1;
	/// Window height in pixels
	int windowHeight = 1;
	/// Far plane distance
	float logDepthFarPlane = 5e9;
	/// Logarithmic depth balance coefficient
	float logDepthC = 1.0;

	// Constants for distance based loading
	/// Max distance at which a planet is considered 'close' (detailed render)
	float closePlanetMaxDistance;
	/// Min distance at which a planet is considered 'far' (flare render)
	float flareMinDistance;
	/// Optimal distance at which a planet is considered 'far' (no flare fade in)
	float flareOptimalDistance;
	/// Distance at which a planet's textures will be loaded
	float texLoadDistance;
	/// Distance at which a planet's textures will be unloaded
	float texUnloadDistance;

	/// Buffer containing vertex data
	Buffer vertexBuffer;
	/// Buffer containing index data
	Buffer indexBuffer;
	/// Buffer containing UBO data
	Buffer uboBuffer;
	
	/// Buffer ranges of each frame (multiple buffering)
	std::vector<DynamicData> dynamicData;
	/// Data writing fences of each frame (multiple buffering)
	std::vector<Fence> fences;

	/// Vertex Array Object of planets, flares and deferred tris
	GLuint vertexArray;

	// Rendertargets : 
	/// Depth stencil attachment of HDR rendertarget
	GLuint depthStencilTex;
	/// HDR MS rendertarget
	GLuint hdrMSRendertarget;
	/// Highpass rendertargets (multiple mips)
	GLuint highpassRendertargets;
	/// Bloom rendertargets (multiple mips)
	GLuint bloomRendertargets;

	/// Number of bloom downsample steps (bigger blurs)
	int bloomDepth = 8;
	/// Texture views to individual highpass rendertarget mipmaps
	std::vector<GLuint> highpassViews;
	/// Texture views to individual bloom rendertarget mipmaps
	std::vector<GLuint> bloomViews;

	/// Rendertarget sampler
	GLuint rendertargetSampler;

	// FBOs
	/// HDR FBO
	GLuint hdrFBO;
	/// Highpass FBOs
	std::vector<GLuint> highpassFBOs;
	/// Bloom FBOs
	std::vector<GLuint> bloomFBOs;

	// Pipelines
	/// Planet without atmo
	ShaderPipeline pipelinePlanetBare;
	/// Planet with atmo
	ShaderPipeline pipelinePlanetAtmo;
	/// Planet with atmo and rings
	ShaderPipeline pipelinePlanetAtmoRing;
		/// Star map
	ShaderPipeline pipelineStarMap;
	/// Atmosphere
	ShaderPipeline pipelineAtmo;
	/// Star
	ShaderPipeline pipelineSun;
	/// Far half ring
	ShaderPipeline pipelineRingFar;
	/// Near half ring
	ShaderPipeline pipelineRingNear;
	/// Highpass for bloom
	ShaderPipeline pipelineHighpass;
	/// Downsample for bloom
	ShaderPipeline pipelineDownsample;
	/// Horizontal blur for bloom
	ShaderPipeline pipelineBlurW;
	/// Vertical blur for bloom
	ShaderPipeline pipelineBlurH;
	/// Bloom reconstitution
	ShaderPipeline pipelineBloomAdd;
	/// Flares
	ShaderPipeline pipelineFlare;
	/// Tonemap and resolve with bloom
	ShaderPipeline pipelineTonemapBloom;
	/// Tonemap and resolve without bloom
	ShaderPipeline pipelineTonemapNoBloom;

	// Multiple buffering
	/// Number of frame modulo bufferFrames
	uint32_t frameId;
	/// Number of frames to multi-buffer
	uint32_t bufferFrames;

	/// Planets' fixed parameters
	std::vector<Planet> planetParams;

	/// Planet data only for rendering
	struct PlanetData
	{
		/// Planet model
		DrawCommand planetModel;
		/// Ring model
		DrawCommand ringModel;
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
		PlanetData() = default;
	};

	/** Fills DynamicPlanetUBO structure from planet parameters and state
	 * @param viewPos World space eye position
	 * @param viewMat View matrix (not accounting translation)
	 * @param state Dynamic planet state
	 * @param params Fixed planet parameters
	 * @param data Planet data for rendering
	 * @returns UBO data
	 */
	PlanetDynamicUBO getPlanetUBO(
		float fovy,
		float exp,
		const glm::dvec3 &viewPos,
		const glm::mat4 &projMat, 
		const glm::mat4 &viewMat,
		const PlanetState &state, 
		const Planet &params, 
		const PlanetData &data);

	float getSunVisibility();

	/// Rendering data for all planets
	std::vector<PlanetData> planetData;
	/// Index of sun in main planet collection
	size_t sunId = 0;

	GLuint sunOcclusionQueries[2] = {0, 0};
	int occlusionQueryResults[2] = {0, 1};

	DDSStreamer::Handle starMapTexHandle{};
	float starMapIntensity = 1.0;

	// Textures
	/// Default diffuse texture
	GLuint diffuseTexDefault;
	/// Default cloud texture
	GLuint cloudTexDefault;
	/// Default emissive night texture
	GLuint nightTexDefault;
	/// Default specular mask texture
	GLuint specularTexDefault;

	/// Flare texture (white dot)
	GLuint flareTex;

	// Samplers
	/// Sampler for planet textures
	GLuint planetTexSampler;
	/// Sampler for atmospheric lookup table
	GLuint atmoSampler;
	/// Sampler for ring textures
	GLuint ringSampler;

	/// Max anisotropy for texture sampling
	float textureAnisotropy;

	// Models
	/// Sphere model (for default planets and atmospheres)
	DrawCommand sphere;
	/// Flare model (Circle)
	DrawCommand flareModel;
	/// Fullscreen triangle for covering the whole screen
	DrawCommand fullscreenTri;

	/// Stream texture loader
	DDSStreamer streamer;
};