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
	void init(std::vector<PlanetParameters> planetParams, SkyboxParameters skyboxParam);
	void render(
		glm::dvec3 viewPos, 
		glm::mat4 projMat, 
		glm::mat4 viewMat,
		std::vector<PlanetState> planetStates);
	void destroy();
private:

	void renderClosePlanet(uint32_t i);
	void renderFarPlanet(uint32_t i);

	uint32_t planetCount;

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

	// Shaders
	ShaderProgram programPlanetBare;

	// Triple buffering
	uint32_t frameId;

	std::vector<PlanetParameters> planetParams; // Static parameters
	std::vector<Model> planetModels; // Offsets and count in buffer
	std::vector<bool> planetTexLoaded; // indicates if the texture are loaded for a planet
	std::vector<GLuint> planetDiffuseTextures; // Diffuse texture for each planet

	Model sphere;
	glm::mat4 skyboxModelMat;

	// Textures and samplers
	GLuint diffuseTexDefault;
	GLuint skyboxTex;
	GLuint diffuseSampler;
};