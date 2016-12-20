#pragma once

#include "renderer.hpp"
#include "shader.hpp"
#include "fence.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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
	uint32_t skyboxUBO;
	std::vector<uint32_t> planetUBOs;
};

class RendererGL : public Renderer
{
public:
	void init(
		std::vector<PlanetParameters> planetParams, 
		SkyboxParameters skyboxParam,
		int msaa,
		int windowWidth,
		int windowHeight);
	void render(
		glm::dvec3 viewPos, 
		float fovy,
		glm::dvec3 viewCenter,
		glm::vec3 viewUp,
		std::vector<PlanetState> planetStates);
	void destroy();
private:
	void createTextures();
	void createBuffers();
	void createVertexArray();
	void createRenderTargets();
	void createSkybox(SkyboxParameters skyboxParam);
	void createShaders();

	void render(GLuint vertexArray, Model m);
	void renderGBuffer(
		std::vector<uint32_t> closePlanets, 
		DynamicOffsets currentDynamicOffsets);
	void renderHdr(
		std::vector<uint32_t> closePlanets, 
		DynamicOffsets currentDynamicOffsets);

	void loadDDSTexture(GLuint &id, std::string filename);
	void unloadDDSTexture(GLuint &id, GLuint defaultId);

	void renderResolve();

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

	// RenderTargets : 
	GLuint attachmentSampler;
	GLuint depthStencilTex;
	// Gbuffer
	std::vector<GLuint> gbufferTex;
	GLuint gbufferFbo;
	// HDR rendertarget
	GLuint hdrTex;
	GLuint hdrFbo;

	// Shaders
	ShaderProgram programPlanetGbuffer;
	ShaderProgram programPlanetDeferred;
	ShaderProgram programSkyboxGbuffer;
	ShaderProgram programSkyboxDeferred;
	ShaderProgram programResolve;

	// Triple buffering
	uint32_t frameId;

	std::vector<PlanetParameters> planetParams; // Static parameters
	std::vector<Model> planetModels; // Offsets and count in buffer
	std::vector<bool> planetTexLoaded; // indicates if the texture are loaded for a planet
	std::vector<GLuint> planetDiffuseTextures; // Diffuse texture for each planet
	std::vector<GLuint> planetCloudTextures;
	std::vector<GLuint> planetNightTextures;

	Model sphere;
	glm::mat4 skyboxModelMat;

	Model fullscreenTri;

	// Textures and samplers
	GLuint diffuseTexDefault;
	GLuint cloudTexDefault;
	GLuint nightTexDefault;
	GLuint skyboxTex;
	GLuint diffuseSampler;
};