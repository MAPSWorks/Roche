#pragma once

#include "renderer.hpp"
#include "graphics_api.hpp"
#include "shader.hpp"
#include "fence.hpp"
#include "ddsloader.hpp"
#include "gl_util.hpp"

#include <condition_variable>
#include <thread>
#include <mutex>
#include <memory>
#include <queue>
#include <map>
#include <stack>

class GPUProfilerGL
{
public:
	void begin(std::string name);
	void end();
	std::vector<std::pair<std::string,uint64_t>> get();
private:
	std::map<std::string, std::pair<GLuint, GLuint>> queries[2];
	std::stack<std::string> names;
	std::vector<std::string> orderedNames[2];
	int bufferId;
	GLuint lastQuery;
};

class RendererGL : public Renderer
{
public:
	void windowHints();
	void init(
		std::vector<PlanetParameters> planetParams, 
		int msaa,
		bool ssaa,
		int windowWidth,
		int windowHeight);
	void render(
		glm::dvec3 viewPos, 
		float fovy,
		glm::dvec3 viewCenter,
		glm::vec3 viewUp,
		float gamma,
		float exposure,
		float ambientColor,
		std::vector<PlanetState> planetStates);
	void destroy();

	std::vector<std::pair<std::string,uint64_t>> getProfilerTimes();
private:
	struct DynamicData
	{
		Fence fence;
		BufferSection sceneUBO;

		std::vector<BufferSection> planetUBOs;
		std::vector<BufferSection> flareUBOs;
	};

	struct SceneDynamicUBO
	{
		glm::mat4 projMat;
		glm::mat4 viewMat;
		glm::vec4 viewPos;
		float ambientColor;
		float invGamma;
		float exposure;
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
		float albedo;
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

	typedef int32_t TexHandle;

	void createTextures();
	void createFlare();
	void createVertexArray();
	void createRendertargets();
	void createShaders();

	void renderHdr(
		std::vector<uint32_t> closePlanets, 
		DynamicData data);
	void renderAtmo(
		std::vector<uint32_t> atmoPlanets,
		DynamicData data);
	void renderBloom();
	void renderFlares(
		std::vector<uint32_t> farPlanets, 
		DynamicData data);
	void renderTonemap(DynamicData data);

	TexHandle createStreamTexture(GLuint tex);
	bool getStreamTexture(TexHandle tex, GLuint &id);
	void removeStreamTexture(TexHandle tex);

	TexHandle loadDDSTexture(std::string filename, glm::vec4 defaultColor);
	void unloadDDSTexture(TexHandle texId);

	void loadTextures(std::vector<uint32_t> planets);
	void unloadTextures(std::vector<uint32_t> planets);
	void uploadLoadedTextures();

	PlanetDynamicUBO getPlanetUBO(glm::dvec3 viewPos, glm::mat4 viewMat,
	PlanetState state, PlanetParameters params);
	FlareDynamicUBO getFlareUBO(glm::dvec3 viewPos, glm::mat4 projMat,
	glm::mat4 viewMat, float fovy, float exp, 
	PlanetState state, PlanetParameters params);

	void initThread();

	GPUProfilerGL profiler;

	uint32_t planetCount;
	int msaaSamples;
	int windowWidth;
	int windowHeight;

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

	GLuint hdrFbo;

	// Shaders
	ShaderProgram programPlanet;
	ShaderProgram programPlanetAtmo;
	ShaderProgram programAtmo;
	ShaderProgram programSun;
	ShaderProgram programRingFar;
	ShaderProgram programRingNear;
	ShaderProgram programHighpass;
	ShaderProgram programDownsample;
	ShaderProgram programBlurW;
	ShaderProgram programBlurH;
	ShaderProgram programBloomAdd;
	ShaderProgram programFlare;
	ShaderProgram programTonemap;

	// Multiple buffering
	uint32_t frameId;
	uint32_t bufferFrames;

	std::vector<PlanetParameters> planetParams; // Static parameters
	std::vector<DrawCommand> planetModels; // Offsets and count in buffer
	std::vector<DrawCommand> ringModels;
	std::vector<bool> planetTexLoaded; // indicates if the texture are loaded for a planet

	// Contains all streamed textures
	TexHandle nextHandle;
	std::map<TexHandle, GLuint> streamTextures;

	std::vector<TexHandle> planetDiffuseTextures; // Diffuse texture for each planet
	std::vector<TexHandle> planetCloudTextures;
	std::vector<TexHandle> planetNightTextures;
	std::vector<GLuint> atmoLookupTables;
	std::vector<GLuint> ringTextures1;
	std::vector<GLuint> ringTextures2;

	// Textures
	GLuint diffuseTexDefault;
	GLuint cloudTexDefault;
	GLuint nightTexDefault;

	GLuint flareIntensityTex;
	GLuint flareLinesTex;
	GLuint flareHaloTex;

	float textureAnisotropy;

	// Models
	DrawCommand sphere;
	DrawCommand flareModel;
	DrawCommand fullscreenTri;

	// Texture load threading
	struct TexWait // Texture waiting to be loaded
	{
		TexHandle tex;
		int mipmap;
		int mipmapCount;
		DDSLoader loader;
	};

	struct TexLoaded // Texture that have finished loading
	{
		TexHandle tex;
		GLenum format;
		int mipmap;
		int mipmapOffset;
		int width, height;
		int imageSize;
		std::shared_ptr<std::vector<uint8_t>> data;
	};

	std::mutex texWaitMutex;
	std::condition_variable texWaitCondition;
	std::queue<TexWait> texWaitQueue; // Queue where texture are waiting for loading
	std::mutex texLoadedMutex;
	std::queue<TexLoaded> texLoadedQueue; // Queue of textures that have finished loading
	std::thread texLoadThread;
	bool killThread;
};