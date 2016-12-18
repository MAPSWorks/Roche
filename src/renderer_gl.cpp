#include "renderer_gl.hpp"
#include "util.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define PI 3.14159265358979323846264338327950288

const bool USE_COHERENT_MEMORY = false;

struct Vertex
{
	glm::vec4 position;
	glm::vec4 uv;
};

struct SceneDynamicUBO
{
	glm::mat4 projMat;
	glm::mat4 viewMat;
	glm::vec4 viewPos;
	float invGamma;
};

struct PlanetDynamicUBO
{
	glm::mat4 modelMat;
	glm::vec4 lightDir;
};

void generateSphere(
	int meridians, 
	int rings, 
	bool exterior, 
	std::vector<Vertex> &vertices, 
	std::vector<uint32_t> &indices)
{
	// Vertices
	vertices.resize((meridians+1)*(rings+1));
	size_t offset = 0;
	for (int i=0;i<=rings;++i)
	{
		float phi = PI*((float)i/(float)rings-0.5);
		float cp = cos(phi);
		float sp = sin(phi);
		for (int j=0;j<=meridians;++j)
		{
			float theta = 2*PI*((float)j/(float)meridians);
			float ct = cos(theta);
			float st = sin(theta);
			vertices[offset] = {
				glm::vec4(cp*ct,cp*st,sp,1), 
				glm::vec4((float)j/(float)meridians, 1.f-(float)i/(float)rings,0.0,0.0)};
			offset++;
		}
	}

	// Indices
	indices.resize(meridians*rings*6);
	offset = 0;
	for (int i=0;i<rings;++i)
	{
		for (int j=0;j<meridians;++j)
		{
			uint32_t i1 = i+1;
			uint32_t j1 = j+1;
			std::vector<uint32_t> ind = {
				(uint32_t)(i *(rings+1)+j),
				(uint32_t)(i1*(rings+1)+j),
				(uint32_t)(i1*(rings+1)+j1),
				(uint32_t)(i1*(rings+1)+j1),
				(uint32_t)(i *(rings+1)+j1),
				(uint32_t)(i *(rings+1)+j)
			};
			// Remove back face culling
			if (!exterior)
				std::swap(ind[1], ind[4]);

			// Copy to indices
			memcpy(&indices[offset*6], ind.data(), ind.size()*sizeof(uint32_t));
			offset++;
		}
	}
};

uint32_t align(uint32_t offset, uint32_t minAlign)
{
	uint32_t remainder = offset%minAlign;
	if (remainder) return offset + (minAlign-remainder);
	return offset;
}

void RendererGL::init(std::vector<PlanetParameters> planetParams, SkyboxParameters skyboxParam)
{
	this->planetParams = planetParams;

	planetCount = planetParams.size();

	// Various alignments
	uint32_t uboMinAlign;
	uint32_t ssboMinAlign;
	uint32_t minAlign = 16;

	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (int*)&uboMinAlign);
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, (int*)&ssboMinAlign);

	// Generate models
	const int planetMeridians = 64;
	const int planetRings = 64;

	std::vector<std::vector<Vertex>> modelsVertices;
	std::vector<std::vector<uint32_t>> modelsIndices;

	modelsVertices.resize(1);
	modelsIndices.resize(1);

	generateSphere(planetMeridians, planetRings, false, modelsVertices[0], modelsIndices[0]);

	// Keep track of model number for each planet
	std::vector<uint32_t> modelNumber(planetCount);
	// Load custom models
	for (uint32_t i=0;i<planetCount;++i)
	{
		if (planetParams[i].assetPaths.modelFilename != "")
		{
			throw std::runtime_error("Custom model not supported");
		}
		else
		{
			modelNumber[i] = 0;
		}
	}

	// Once we have all the models, find the offsets
	uint32_t currentOffset = 0;

	// Offsets in static buffer
	std::vector<Model> models(modelsVertices.size());
	// Vertex buffer offsets
	currentOffset = align(currentOffset, minAlign);
	vertexOffset = currentOffset;
	for (uint32_t i=0;i<models.size();++i)
	{
		models[i].vertexOffset = currentOffset;
		models[i].count = modelsIndices[i].size();
		currentOffset += modelsVertices[i].size()*sizeof(Vertex);
		currentOffset = align(currentOffset, minAlign);
	}	
	// Index buffer offsets
	currentOffset = align(currentOffset, minAlign);
	indexOffset = currentOffset;
	for (uint32_t i=0;i<models.size();++i)
	{
		models[i].indexOffset = currentOffset;
		currentOffset += modelsIndices[i].size()*sizeof(uint32_t);
		currentOffset = align(currentOffset, minAlign);
	}

	sphere = models[0];

	staticBufferSize = currentOffset;

	// Offsets in dynamic buffer
	currentOffset = 0;
	dynamicOffsets.resize(3); // triple buffering
	for (uint32_t i=0;i<dynamicOffsets.size();++i)
	{
		currentOffset = align(currentOffset, uboMinAlign);
		dynamicOffsets[i].offset = currentOffset;
		// Scene UBO
		dynamicOffsets[i].sceneUBO = currentOffset;
		currentOffset += sizeof(SceneDynamicUBO);
		// Skybox UBO
		currentOffset = align(currentOffset, uboMinAlign);
		dynamicOffsets[i].skyboxUBO = currentOffset;
		currentOffset += sizeof(PlanetDynamicUBO);
		// Planet UBOs
		dynamicOffsets[i].planetUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			currentOffset = align(currentOffset, uboMinAlign);
			dynamicOffsets[i].planetUBOs[j] = currentOffset;
			currentOffset += sizeof(PlanetDynamicUBO);
		}
		dynamicOffsets[i].size = currentOffset - dynamicOffsets[i].offset;
	}

	// Assign models to planets
	planetModels.resize(planetCount);
	for (uint32_t i=0;i<modelNumber.size();++i)
	{
		planetModels[i] = models[modelNumber[i]];
	}

	dynamicBufferSize = currentOffset;

	// Create static buffer
	glCreateBuffers(1, &staticBuffer);
	glNamedBufferStorage(
		staticBuffer, staticBufferSize, nullptr,
		GL_DYNAMIC_STORAGE_BIT);

	// Fill static buffer
	for (uint32_t i=0;i<models.size();++i) // Vertices
	{
		glNamedBufferSubData(
			staticBuffer, 
			models[i].vertexOffset, 
			modelsVertices[i].size()*sizeof(Vertex), 
			modelsVertices[i].data());
	}
	for (uint32_t i=0;i<models.size();++i) // Indices
	{
		glNamedBufferSubData(
			staticBuffer,
			models[i].indexOffset,
			modelsIndices[i].size()*sizeof(uint32_t),
			modelsIndices[i].data());
	}

	// Create dynamic Buffer
	glCreateBuffers(1, &dynamicBuffer);
	GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
	if (USE_COHERENT_MEMORY) flags += GL_MAP_COHERENT_BIT;

	glNamedBufferStorage(dynamicBuffer, dynamicBufferSize, nullptr, flags);

	if (!USE_COHERENT_MEMORY) flags += GL_MAP_FLUSH_EXPLICIT_BIT;

	dynamicBufferPtr = glMapNamedBufferRange(
		dynamicBuffer, 0, dynamicBufferSize, flags);

	// Dynamic buffer fences
	fences.resize(dynamicOffsets.size());

	// Vertex Array Object creation
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &vertexArray);
	glVertexArrayVertexBuffer(vertexArray, VERTEX_BINDING, staticBuffer, vertexOffset, sizeof(Vertex));
	glVertexArrayElementBuffer(vertexArray, staticBuffer);

	const int VERTEX_ATTRIB_POS = 0;
	const int VERTEX_ATTRIB_UV = 1;

	// Position
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_POS, 4, GL_FLOAT, false, offsetof(Vertex, position));

	// UVs
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_UV, 4, GL_FLOAT, false, offsetof(Vertex, uv));

	// Shader loading
	programPlanetBare.source(GL_VERTEX_SHADER, "shaders/planet.vert");
	programPlanetBare.source(GL_FRAGMENT_SHADER, "shaders/planet_bare.frag");
	programPlanetBare.link();

	float anisotropy = 16.f;
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
	if (anisotropy > maxAnisotropy) anisotropy = maxAnisotropy;

	// Sampler init
	glCreateSamplers(1, &diffuseSampler);
	glSamplerParameterf(diffuseSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	glSamplerParameteri(diffuseSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// Texture init
	planetTexLoaded.resize(planetCount);

	// Default diffuse tex
	uint8_t data[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &diffuseTexDefault);
	glTextureStorage2D(diffuseTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(diffuseTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &data);

	planetDiffuseTextures.resize(planetCount, diffuseTexDefault);

	// Misc.
	glEnable(GL_MULTISAMPLE);

	// Skybox model matrix
	glm::quat q = glm::rotate(glm::quat(), skyboxParam.inclination, glm::vec3(1,0,0));
	skyboxModelMat = glm::scale(glm::mat4_cast(q), glm::vec3(-5e9));
	// Skybox texture
	try
	{
		DDSLoader loader(skyboxParam.textureFilename);
		int mipmapCount = loader.getMipmapCount();

		glCreateTextures(GL_TEXTURE_2D, 1, &skyboxTex);
		glTextureStorage2D(skyboxTex, 
			mipmapCount, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, loader.getWidth(0), loader.getHeight(0));
		for (int j=0;j<mipmapCount;++j)
		{
			std::vector<uint8_t> imageData;
			loader.getImageData(j, imageData);
			glCompressedTextureSubImage2D(
				skyboxTex,
				j, 
				0, 0, 
				loader.getWidth(j), loader.getHeight(j), 
				GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
				imageData.size(), imageData.data());
		}
	}
	catch (...)
	{
		skyboxTex = diffuseTexDefault;
	}
}

void RendererGL::destroy()
{
	glDeleteSamplers(1, &diffuseSampler);
	glDeleteTextures(1, &diffuseTexDefault);

	if (glUnmapNamedBuffer(dynamicBuffer) == GL_FALSE)
	{
		throw std::runtime_error("Staging buffer memory corruption");
	}
	glDeleteBuffers(1, &staticBuffer);
	glDeleteBuffers(1, &dynamicBuffer);
}

void RendererGL::render(
		glm::dvec3 viewPos, 
		glm::mat4 projMat, 
		glm::mat4 viewMat,
		std::vector<PlanetState> planetStates)
{
	const float closePlanetMaxDistance = 500;
	const float texLoadDistance = 800;
	const float texUnloadDistance = 1000;

	// Triple buffer of dynamic UBO
	uint32_t nextFrameId = (frameId+1)%3;
	auto &currentDynamicOffsets = dynamicOffsets[frameId];
	auto &nextDynamicOffsets = dynamicOffsets[nextFrameId]; 

	// Planet classification
	std::vector<uint32_t> closePlanets;
	std::vector<uint32_t> farPlanets;

	std::vector<uint32_t> texLoadPlanets;
	std::vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<planetStates.size();++i)
	{
		float radius = planetParams[i].bodyParam.radius;
		double dist = glm::distance(viewPos, planetStates[i].position)/radius;
		if (dist < texLoadDistance && !planetTexLoaded[i])
		{
			// Textures need to be loaded
			texLoadPlanets.push_back(i);
		}
		else if (dist > texUnloadDistance && planetTexLoaded[i])
		{
			// Textures need to be unloaded
			texUnloadPlanets.push_back(i);
		}

		if (dist < closePlanetMaxDistance)
			closePlanets.push_back(i);
		else
			farPlanets.push_back(i);
	}

	// Scene uniform update
	SceneDynamicUBO sceneUBO;
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.viewPos = glm::vec4(0.0,0.0,0.0,1.0);
	sceneUBO.invGamma = 1.f/gamma;

	// Skybox uniform update
	PlanetDynamicUBO skyboxUBO;
	skyboxUBO.modelMat = skyboxModelMat;
	skyboxUBO.lightDir = glm::vec4(0.0);

	// Planet uniform update
	std::vector<PlanetDynamicUBO> planetUBOs(planetCount);
	for (uint32_t i : closePlanets)
	{
		glm::vec3 planetPos = planetStates[i].position - viewPos;

		// Planet rotation
		glm::vec3 north = glm::vec3(0,0,1);
		glm::vec3 rotAxis = planetParams[i].bodyParam.rotationAxis;
		glm::quat q = glm::rotate(glm::quat(), 
			(float)acos(glm::dot(north, rotAxis)), glm::cross(north, rotAxis));
		q = glm::rotate(q, planetStates[i].rotationAngle, north);

		// Model matrix
		glm::mat4 modelMat = glm::translate(glm::mat4(), planetPos);
		modelMat *= glm::mat4_cast(q);
		modelMat = glm::scale(modelMat, glm::vec3(planetParams[i].bodyParam.radius));

		// Light direction
		glm::vec3 lightDir = glm::vec3(0.f);
		if (glm::length(planetStates[i].position) > 0.1) 
			lightDir = glm::normalize(-planetStates[i].position);

		planetUBOs[i].modelMat = modelMat;
		planetUBOs[i].lightDir = glm::vec4(lightDir,0.0);
	}

	// Dynamic data upload
	fences[nextFrameId].wait();

	memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.sceneUBO, 
		&sceneUBO, sizeof(SceneDynamicUBO));
	memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.skyboxUBO,
		&skyboxUBO, sizeof(PlanetDynamicUBO));
	for (uint32_t i=0;i<planetUBOs.size();++i)
		memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.planetUBOs[i], 
			&planetUBOs[i], sizeof(PlanetDynamicUBO));

	if (!USE_COHERENT_MEMORY)
		glFlushMappedNamedBufferRange(dynamicBuffer, nextDynamicOffsets.offset, nextDynamicOffsets.size);

	fences[nextFrameId].lock();

	// Texture loading
	for (uint32_t i : texLoadPlanets)
	{
		// Diffuse texture
		try
		{
			DDSLoader loader(planetParams[i].assetPaths.diffuseFilename);
			int mipmapCount = loader.getMipmapCount();

			glCreateTextures(GL_TEXTURE_2D, 1, &planetDiffuseTextures[i]);
			glTextureStorage2D(planetDiffuseTextures[i], 
				mipmapCount, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, loader.getWidth(0), loader.getHeight(0));
			for (int j=0;j<mipmapCount;++j)
			{
				std::vector<uint8_t> imageData;
				loader.getImageData(j, imageData);
				glCompressedTextureSubImage2D(
					planetDiffuseTextures[i],
					j, 
					0, 0, 
					loader.getWidth(j), loader.getHeight(j), 
					GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
					imageData.size(), imageData.data());
			}
		}
		catch (...)
		{

		}
		planetTexLoaded[i] = true;
	}

	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		if (planetDiffuseTextures[i] != diffuseTexDefault)
			glDeleteTextures(1, &planetDiffuseTextures[i]);
		planetDiffuseTextures[i] = diffuseTexDefault;
		planetTexLoaded[i] = false;
	}

	// Planet sorting from front to back
	std::sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		/*
		float distI = (viewMat*planetStates[i].position).z;
		float distJ = (viewMat*planetStates[j].position).z;
		*/
		float distI = glm::distance(planetStates[i].position, viewPos);
		float distJ = glm::distance(planetStates[j].position, viewPos);
		return distI < distJ;
	});

	// Clearing
	float clearColor[] = {0.f,0.f,0.f,1.0f};
	glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor);
	glClearNamedFramebufferfi(0, GL_DEPTH_STENCIL, 0, 1.f, 0);

	// Planet rendering
	glUseProgram(programPlanetBare.getId());

	// Bind Scene UBO 
	glBindBufferRange(
		GL_UNIFORM_BUFFER, 0, dynamicBuffer,
		currentDynamicOffsets.sceneUBO, sizeof(SceneDynamicUBO));

	// Diffuse sampler
	glBindSampler(2, diffuseSampler);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_CLAMP);
	glDepthFunc(GL_LESS);

	for (uint32_t i : closePlanets)
	{
		// Bind planet UBO
		glBindBufferRange(
			GL_UNIFORM_BUFFER, 1, dynamicBuffer,
			currentDynamicOffsets.planetUBOs[i],
			sizeof(PlanetDynamicUBO));
		// Bind Textures
		glBindTextureUnit(2, planetDiffuseTextures[i]);
		// Draw
		glBindVertexArray(vertexArray);
		glDrawElements(GL_TRIANGLES, planetModels[i].count, GL_UNSIGNED_INT, 
			(void*)(intptr_t)planetModels[i].indexOffset);
	}

	// Skybox rendering
	glEnable(GL_DEPTH_CLAMP);
	glDepthFunc(GL_LEQUAL);
	
	glBindBufferRange(
		GL_UNIFORM_BUFFER, 1, dynamicBuffer,
		currentDynamicOffsets.skyboxUBO,
		sizeof(PlanetDynamicUBO));
	glBindTextureUnit(2, skyboxTex);
	glDrawElements(GL_TRIANGLES, sphere.count, GL_UNSIGNED_INT,
		(void*)(intptr_t)sphere.indexOffset);

	frameId = (frameId+1)%3;
}