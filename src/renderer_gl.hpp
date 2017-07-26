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
	struct DynamicData
	{
		BufferRange sceneUBO;

		std::vector<BufferRange> planetUBOs;
		std::vector<BufferRange> flareUBOs;
	};

	struct SceneDynamicUBO
	{
		glm::mat4 projMat;
		glm::mat4 viewMat;
		glm::vec4 viewPos;
		float ambientColor;
		float exposure;
		float logDepthFarPlane;
		float logDepthC;
	};

	struct PlanetDynamicUBO
	{
		glm::mat4 modelMat;
		glm::mat4 atmoMat;
		glm::mat4 ringFarMat;
		glm::mat4 ringNearMat;
		glm::vec4 planetPos;
		glm::vec4 lightDir;
		glm::vec4 K;
		glm::vec4 mask0ColorHardness;
		glm::vec4 mask1ColorHardness;
		glm::vec4 ringNormal;
		float ringInner;
		float ringOuter;
		float starBrightness;
		float cloudDisp;
		float nightTexIntensity;
		float radius;
		float atmoHeight;
	};

	struct FlareDynamicUBO
	{
		glm::mat4 modelMat;
		glm::vec4 color;
		float brightness;
	};

	void createModels();
	void createUBO();
	void createTextures();
	void createFlare();
	void createVertexArray();
	void createRendertargets();
	void createShaders();
	void createScreenshot();

	void renderHdr(
		const std::vector<uint32_t> &closePlanets, 
		const DynamicData &data);
	void renderTranslucent(
		const std::vector<uint32_t> &translucentPlanets,
		const DynamicData &data);
	void renderHighpass(const DynamicData &data);
	void renderDownsample(const DynamicData &data);
	void renderBloom(const DynamicData &data);
	void renderFlares(
		const std::vector<uint32_t> &farPlanets, 
		const DynamicData &data);
	void renderTonemap(const DynamicData &data);

	void loadTextures(const std::vector<uint32_t> &planets);
	void unloadTextures(const std::vector<uint32_t> &planets);
	void uploadLoadedTextures();

	void saveScreenshot();

	GPUProfilerGL profiler;

	// Screenshot info
	bool takeScreen = false;
	std::string screenFilename = "";
	Screenshot::Format screenBestFormat = Screenshot::Format::RGBA8;
	GLenum screenBestFormatGL = GL_RGBA;
	Screenshot screenshot;

	uint32_t planetCount = 0;
	int msaaSamples = 1;
	int maxTexSize = -1;
	int windowWidth = 1;
	int windowHeight = 1;
	float logDepthFarPlane = 5e9;
	float logDepthC = 1.0;

	// Constants for distance based loading
	float closePlanetMaxDistance;
	float farPlanetMinDistance;
	float farPlanetOptimalDistance;
	float texLoadDistance;
	float texUnloadDistance;

	Buffer vertexBuffer;
	Buffer indexBuffer;

	Buffer uboBuffer;
	
	// Offsets in dynamic buffer
	std::vector<DynamicData> dynamicData;
	std::vector<Fence> fences;

	// VAO
	GLuint vertexArray;

	// Rendertargets : 
	GLuint depthStencilTex;
	// Gbuffer
	std::vector<GLuint> gbufferTex;
	GLuint gbufferFbo;
	// HDR rendertarget
	GLuint hdrMSRendertarget;
	GLuint highpassRendertargets;
	GLuint bloomRendertargets;

	// Number of bloom steps
	int bloomDepth = 8;
	// Texture views to mipmaps
	std::vector<GLuint> highpassViews;
	std::vector<GLuint> bloomViews;

	// Rendertarget sampler
	GLuint rendertargetSampler;

	// FBOs
	GLuint hdrFBO;
	std::vector<GLuint> highpassFBOs;
	std::vector<GLuint> bloomFBOs;

	// Pipelines
	ShaderPipeline pipelinePlanetBare;
	ShaderPipeline pipelinePlanetAtmo;
	ShaderPipeline pipelinePlanetAtmoRing;
	ShaderPipeline pipelineAtmo;
	ShaderPipeline pipelineSun;
	ShaderPipeline pipelineRingFar;
	ShaderPipeline pipelineRingNear;
	ShaderPipeline pipelineHighpass;
	ShaderPipeline pipelineDownsample;
	ShaderPipeline pipelineBlurW;
	ShaderPipeline pipelineBlurH;
	ShaderPipeline pipelineBloomAdd;
	ShaderPipeline pipelineFlare;
	ShaderPipeline pipelineTonemap;

	// Multiple buffering
	uint32_t frameId;
	uint32_t bufferFrames;

	std::vector<Planet> planetParams; // Static parameters

	struct PlanetData
	{
		DrawCommand planetModel;
		DrawCommand ringModel;
		bool texLoaded = false;

		DDSStreamer::Handle diffuse{};
		DDSStreamer::Handle cloud{};
		DDSStreamer::Handle night{};
		DDSStreamer::Handle specular{};

		GLuint atmoLookupTable = 0;
		GLuint ringTex1 = 0;
		GLuint ringTex2 = 0;
		PlanetData() = default;
	};

	PlanetDynamicUBO getPlanetUBO(
		const glm::dvec3 &viewPos, 
		const glm::mat4 &viewMat,
		const PlanetState &state, 
		const Planet &params, 
		const PlanetData &data);

	FlareDynamicUBO getFlareUBO(
		const glm::dvec3 &viewPos, 
		const glm::mat4 &projMat,
		const glm::mat4 &viewMat, 
		float fovy,
		float exp, 
		const PlanetState &state, 
		const Planet &params);

	std::vector<PlanetData> planetData;

	// Textures
	GLuint diffuseTexDefault;
	GLuint cloudTexDefault;
	GLuint nightTexDefault;
	GLuint specularTexDefault;

	GLuint flareIntensityTex;
	GLuint flareLinesTex;
	GLuint flareHaloTex;

	// Samplers
	GLuint planetTexSampler;
	GLuint atmoSampler;
	GLuint ringSampler;

	float textureAnisotropy;

	// Models
	DrawCommand sphere;
	DrawCommand flareModel;
	DrawCommand fullscreenTri;

	// Stream loading
	DDSStreamer streamer;
};