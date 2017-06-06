#pragma once

#include "renderer.hpp"
#include "graphics_api.hpp"
#include "fence.hpp"
#include "ddsloader.hpp"
#include "gl_util.hpp"
#include "gl_profiler.hpp"
#include "screenshot.hpp"
#include "tex_stream.hpp"

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
	void init(
		const std::vector<Planet> &planetParams, 
		int msaa,
		int maxTexSize,
		int windowWidth,
		int windowHeight) override;
	void render(
		const glm::dvec3 &viewPos, 
		float fovy,
		const glm::dvec3 &viewCenter,
		const glm::vec3 &viewUp,
		float exposure,
		float ambientColor,
		const std::vector<PlanetState> &planetStates) override;
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
		float logDepthCoef;
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

	void createTextures();
	void createFlare();
	void createVertexArray();
	void createRendertargets();
	void createShaders();
	void createScreenshot();

	void renderHdr(
		const std::vector<uint32_t> &closePlanets, 
		const DynamicData &data);
	void renderAtmo(
		const std::vector<uint32_t> &atmoPlanets,
		const DynamicData &data);
	void renderBloom();
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
	int windowWidth = 0;
	int windowHeight = 0;
	float farPlane = 5e10;

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
	GLuint highpassRendertargets[5];
	GLuint bloomRendertargets[4];

	// Rendertarget sampler
	GLuint rendertargetSampler;

	// HDR framebuffer
	GLuint hdrFbo;

	// Vertex Shaders
	GLuint shaderVertPlanetBare;
	GLuint shaderVertPlanetAtmo;
	GLuint shaderVertAtmo;
	GLuint shaderVertSun;
	GLuint shaderVertRingFar;
	GLuint shaderVertRingNear;
	GLuint shaderVertFlare;
	GLuint shaderVertTonemap;

	// Fragment Shaders
	GLuint shaderFragPlanetBare;
	GLuint shaderFragPlanetAtmo;
	GLuint shaderFragAtmo;
	GLuint shaderFragSun;
	GLuint shaderFragRingFar;
	GLuint shaderFragRingNear;
	GLuint shaderFragFlare;
	GLuint shaderFragTonemap;

	// Compute Shaders
	GLuint shaderCompHighpass;
	GLuint shaderCompDownsample;
	GLuint shaderCompBlurW;
	GLuint shaderCompBlurH;
	GLuint shaderCompBloomAdd;

	// Pipelines
	GLuint pipelinePlanetBare;
	GLuint pipelinePlanetAtmo;
	GLuint pipelineAtmo;
	GLuint pipelineSun;
	GLuint pipelineRingFar;
	GLuint pipelineRingNear;
	GLuint pipelineFlare;
	GLuint pipelineTonemap;

	// Compute pipelines
	GLuint pipelineHighpass;
	GLuint pipelineDownsample;
	GLuint pipelineBlurW;
	GLuint pipelineBlurH;
	GLuint pipelineBloomAdd;

	// Multiple buffering
	uint32_t frameId;
	uint32_t bufferFrames;

	std::vector<Planet> planetParams; // Static parameters

	struct PlanetData
	{
		DrawCommand planetModel;
		DrawCommand ringModel;
		bool texLoaded = false;

		StreamTex diffuse{};
		StreamTex cloud{};
		StreamTex night{};

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