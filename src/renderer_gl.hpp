#pragma once

#include "renderer.hpp"
#include "shader.hpp"
#include "fence.hpp"
#include "ddsloader.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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
	struct Model
	{
		uint32_t vertexOffset;
		uint32_t indexOffset;
		uint32_t count;
	};

	struct DynamicOffsets
	{
		uint32_t offset;
		uint32_t size;

		uint32_t sceneUBO;
		std::vector<uint32_t> planetUBOs;
		std::vector<uint32_t> flareUBOs;
	};

	typedef int32_t TexHandle;

	void createTextures();
	void createFlare();
	void createBuffers();
	void createVertexArray();
	void createRendertargets();
	void createShaders();

	void render(GLuint vertexArray, Model m);
	void renderHdr(
		std::vector<uint32_t> closePlanets, 
		DynamicOffsets currentDynamicOffsets);
	void renderAtmo(
		std::vector<uint32_t> atmoPlanets,
		DynamicOffsets currentDynamicOffsets);
	void renderBloom();
	void renderFlares(
		std::vector<uint32_t> farPlanets, 
		DynamicOffsets currentDynamicOffsets);
	void renderTonemap(DynamicOffsets currentDynamicOffsets);

	TexHandle createStreamTexture(GLuint tex);
	bool getStreamTexture(TexHandle tex, GLuint &id);
	void removeStreamTexture(TexHandle tex);

	TexHandle loadDDSTexture(std::string filename, glm::vec4 defaultColor);
	void unloadDDSTexture(TexHandle texId);

	GPUProfilerGL profiler;

	uint32_t planetCount;
	int msaaSamples;
	int windowWidth;
	int windowHeight;

	// Buffer holding static data (uploaded once at init)
	GLuint staticBuffer;
	uint32_t staticBufferSize;
	uint32_t vertexOffset;
	uint32_t indexOffset;

	// Buffer holding dynamic data (replaced each frame)
	GLuint dynamicBuffer;
	uint32_t dynamicBufferSize;
	void *dynamicBufferPtr;
	
	// Offsets in dynamic buffer
	std::vector<DynamicOffsets> dynamicOffsets;
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

	// Current frame % 3 for triple buffering
	uint32_t frameId;
	uint32_t bufferFrames;

	std::vector<PlanetParameters> planetParams; // Static parameters
	std::vector<uint32_t> previousFrameClosePlanets;
	std::vector<uint32_t> previousFrameFarPlanets;
	std::vector<uint32_t> previousFrameAtmoPlanets;
	std::vector<Model> planetModels; // Offsets and count in buffer
	std::vector<Model> ringModels;
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
	Model sphere;
	Model flareModel;
	Model fullscreenTri;

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