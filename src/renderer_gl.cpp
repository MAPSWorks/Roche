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
	float cloudDisp;
};

void generateSphere(
	const int meridians, 
	const int rings, 
	const bool exterior, 
	std::vector<Vertex> &vertices, 
	std::vector<uint32_t> &indices)
{
	// Vertices
	vertices.resize((meridians+1)*(rings+1));
	size_t offset = 0;
	for (int i=0;i<=rings;++i)
	{
		const float phi = PI*((float)i/(float)rings-0.5);
		const float cp = cos(phi);
		const float sp = sin(phi);
		for (int j=0;j<=meridians;++j)
		{
			const float theta = 2*PI*((float)j/(float)meridians);
			const float ct = cos(theta);
			const float st = sin(theta);
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
			const uint32_t i1 = i+1;
			const uint32_t j1 = j+1;
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
}

void generateFullscreenTri(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{
	vertices.resize(3);
	vertices[0].position = glm::vec4(-2,-1,0,1);
	vertices[1].position = glm::vec4( 2,-1,0,1);
	vertices[2].position = glm::vec4( 0, 4,0,1);

	indices = {0,1,2};
}

uint32_t align(const uint32_t offset, const uint32_t minAlign)
{
	const uint32_t remainder = offset%minAlign;
	if (remainder) return offset + (minAlign-remainder);
	return offset;
}

void RendererGL::init(
	const std::vector<PlanetParameters> planetParams, 
	const SkyboxParameters skyboxParam,
	const int msaa,
	const int windowWidth,
	const int windowHeight)
{
	this->planetParams = planetParams;
	this->planetCount = planetParams.size();
	this->msaaSamples = msaa;
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	// Various alignments
	uint32_t uboMinAlign;
	uint32_t ssboMinAlign;
	const uint32_t minAlign = 32;

	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (int*)&uboMinAlign);
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, (int*)&ssboMinAlign);

	std::vector<std::vector<Vertex>> modelsVertices;
	std::vector<std::vector<uint32_t>> modelsIndices;

	modelsVertices.resize(2);
	modelsIndices.resize(2);

	const size_t FULLSCREEN_TRI_INDEX = 0;
	const size_t SPHERE_INDEX = 1;

	// Fullscreen tri
	generateFullscreenTri(
		modelsVertices[FULLSCREEN_TRI_INDEX],
		modelsIndices[FULLSCREEN_TRI_INDEX]);

	// Generate models
	const int planetMeridians = 64;
	const int planetRings = 64;
	generateSphere(planetMeridians, planetRings, false, 
		modelsVertices[SPHERE_INDEX], 
		modelsIndices[SPHERE_INDEX]);

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
			modelNumber[i] = SPHERE_INDEX;
		}
	}

	// Once we have all the models, find the offsets
	uint32_t currentOffset = 0;

	// Offsets in static buffer
	std::vector<Model> models(modelsVertices.size());
	// Vertex buffer offsets
	vertexOffset = currentOffset;
	for (uint32_t i=0;i<models.size();++i)
	{
		models[i].vertexOffset = currentOffset;
		models[i].count = modelsIndices[i].size();
		currentOffset += modelsVertices[i].size()*sizeof(Vertex);
	}	
	// Index buffer offsets
	indexOffset = currentOffset;
	for (uint32_t i=0;i<models.size();++i)
	{
		models[i].indexOffset = currentOffset;
		currentOffset += modelsIndices[i].size()*sizeof(uint32_t);
	}

	fullscreenTri = models[FULLSCREEN_TRI_INDEX];
	sphere = models[SPHERE_INDEX];

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

	dynamicBufferSize = currentOffset;

	// Assign models to planets
	planetModels.resize(planetCount);
	for (uint32_t i=0;i<modelNumber.size();++i)
	{
		planetModels[i] = models[modelNumber[i]];
	}

	// Create static & dynamic buffers
	createBuffers();

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

	// Dynamic buffer fences
	fences.resize(dynamicOffsets.size());

	// Vertex array creation
	createVertexArray();

	// Shader loading
	createShaders();

	createRenderTargets();

	createTextures();

	createSkybox(skyboxParam);
}

void RendererGL::createTextures()
{
	// Anisotropy
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

	const float requestedAnisotropy = 16.f;
	const float anisotropy = (requestedAnisotropy > maxAnisotropy)?maxAnisotropy:requestedAnisotropy;

	// Sampler init
	glCreateSamplers(1, &diffuseSampler);
	glSamplerParameterf(diffuseSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	glSamplerParameteri(diffuseSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(diffuseSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// Texture init
	planetTexLoaded.resize(planetCount);

	// Default diffuse tex
	const uint8_t diffuseData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &diffuseTexDefault);
	glTextureStorage2D(diffuseTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(diffuseTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &diffuseData);

	planetDiffuseTextures.resize(planetCount, diffuseTexDefault);

	// Default cloud tex
	const uint8_t cloudData[] = {255, 255, 255, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &cloudTexDefault);
	glTextureStorage2D(cloudTexDefault, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(cloudTexDefault, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cloudData);

	planetCloudTextures.resize(planetCount, cloudTexDefault);

	// Default night tex
	const uint8_t nightData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &nightTexDefault);
	glTextureStorage2D(nightTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(nightTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &nightData);

	planetNightTextures.resize(planetCount, nightTexDefault);
}

void RendererGL::createBuffers()
{
	// Create static buffer
	glCreateBuffers(1, &staticBuffer);
	glNamedBufferStorage(
		staticBuffer, staticBufferSize, nullptr,
		GL_DYNAMIC_STORAGE_BIT);

	// Create dynamic Buffer
	glCreateBuffers(1, &dynamicBuffer);

	// Storage & map dynamic buffer
	const GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | 
		((USE_COHERENT_MEMORY)?GL_MAP_COHERENT_BIT:0);
	const GLbitfield mapFlags = storageFlags | 
	((USE_COHERENT_MEMORY)?0:GL_MAP_FLUSH_EXPLICIT_BIT);

	glNamedBufferStorage(dynamicBuffer, dynamicBufferSize, nullptr, storageFlags);
	dynamicBufferPtr = glMapNamedBufferRange(
		dynamicBuffer, 0, dynamicBufferSize, mapFlags);

	if (!dynamicBufferPtr) throw std::runtime_error("Can't map dynamic buffer");
}

void RendererGL::createVertexArray()
{
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
}

void RendererGL::createRenderTargets()
{
	// Sampler
	glCreateSamplers(1, &attachmentSampler);
	glSamplerParameteri(attachmentSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(attachmentSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Depth stencil texture
	glCreateTextures(GL_TEXTURE_2D, 1, &depthStencilTex);
	glTextureStorage2D(depthStencilTex, 1, GL_DEPTH24_STENCIL8, windowWidth, windowHeight);

	// Gbuffer
	std::vector<GLenum> formats = {
		GL_R32F,             // Linear depth
		GL_RG16,             // UVs
		GL_RGBA16F,          // UV derivatives
		GL_RG16F,            // Normals (Spheremap transform)
	};

	std::vector<GLenum> attachments = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
	};

// Textures
	gbufferTex.resize(formats.size());
	glCreateTextures(GL_TEXTURE_2D, gbufferTex.size(), gbufferTex.data());
	for (uint32_t i=0;i<gbufferTex.size();++i)
	{
		glTextureStorage2D(gbufferTex[i], 1, formats[i], 
			windowWidth, windowHeight);
	}

	// Framebuffer
	glCreateFramebuffers(1, &gbufferFbo);
	glNamedFramebufferDrawBuffers(gbufferFbo, attachments.size(), attachments.data());
	glNamedFramebufferTexture(gbufferFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);
	for (uint32_t i=0;i<gbufferTex.size();++i)
		glNamedFramebufferTexture(gbufferFbo, attachments[i], gbufferTex[i], 0);

	// HDR
	// Texture
	glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
	glTextureStorage2D(hdrTex, 1, GL_RGB16F, 
		windowWidth, windowHeight);

	// Framebuffer
	glCreateFramebuffers(1, &hdrFbo);
	glNamedFramebufferTexture(hdrFbo, GL_COLOR_ATTACHMENT0, hdrTex, 0);
	glNamedFramebufferTexture(hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);
}

void RendererGL::createShaders()
{
	programPlanetGbuffer.source(GL_VERTEX_SHADER, "shaders/planet.vert");
	programPlanetGbuffer.source(GL_FRAGMENT_SHADER, "shaders/planet_gbuffer.frag");
	programPlanetGbuffer.link();

	programSkyboxGbuffer.source(GL_VERTEX_SHADER, "shaders/planet.vert");
	programSkyboxGbuffer.source(GL_FRAGMENT_SHADER, "shaders/skybox_gbuffer.frag");
	programSkyboxGbuffer.link();

	programSkyboxDeferred.source(GL_VERTEX_SHADER, "shaders/deferred.vert");
	programSkyboxDeferred.source(GL_FRAGMENT_SHADER, "shaders/skybox_deferred.frag");
	programSkyboxDeferred.link();

	programPlanetDeferred.source(GL_VERTEX_SHADER, "shaders/deferred.vert");
	programPlanetDeferred.source(GL_FRAGMENT_SHADER, "shaders/planet_deferred.frag");
	programPlanetDeferred.link();

	programResolve.source(GL_VERTEX_SHADER, "shaders/deferred.vert");
	programResolve.source(GL_FRAGMENT_SHADER, "shaders/resolve.frag");
	programResolve.link();
}

void RendererGL::createSkybox(SkyboxParameters skyboxParam)
{
	// Skybox model matrix
	const glm::quat q = glm::rotate(glm::quat(), skyboxParam.inclination, glm::vec3(1,0,0));
	skyboxModelMat = glm::scale(glm::mat4_cast(q), glm::vec3(-5e9));
	// Skybox texture
	skyboxTex = diffuseTexDefault;
	loadDDSTexture(skyboxTex, skyboxParam.textureFilename);
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

void RendererGL::loadDDSTexture(GLuint &id, const std::string filename)
{
	DDSLoader loader;
	if (loader.open(filename))
	{
		const int mipmapCount = loader.getMipmapCount();

		glCreateTextures(GL_TEXTURE_2D, 1, &id);
		glTextureStorage2D(id, 
			mipmapCount, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 
			loader.getWidth(0), loader.getHeight(0));
		for (int j=0;j<mipmapCount;++j)
		{
			std::vector<uint8_t> imageData;
			loader.getImageData(j, imageData);
			glCompressedTextureSubImage2D(
				id,
				j, 
				0, 0, 
				loader.getWidth(j), loader.getHeight(j), 
				GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
				imageData.size(), imageData.data());
		}
	}
}

void RendererGL::unloadDDSTexture(GLuint &id, const GLuint defaultId)
{
	if (id != defaultId)
		glDeleteTextures(1, &id);
	id = defaultId;
}

void RendererGL::render(
		const glm::dvec3 viewPos, 
		const float fovy,
		const glm::dvec3 viewCenter,
		const glm::vec3 viewUp,
		const std::vector<PlanetState> planetStates)
{
	const float closePlanetMinSizePixels = 1;
	const float closePlanetMaxDistance = windowHeight/(closePlanetMinSizePixels*tan(fovy/2));
	const float texLoadDistance = closePlanetMaxDistance*1.4;
	const float texUnloadDistance = closePlanetMaxDistance*1.6;

	// Triple buffer of dynamic UBO
	const uint32_t nextFrameId = (frameId+1)%3;
	const auto &currentDynamicOffsets = dynamicOffsets[frameId];
	const auto &nextDynamicOffsets = dynamicOffsets[nextFrameId]; 

	// Planet classification
	std::vector<uint32_t> closePlanets;
	std::vector<uint32_t> farPlanets;

	std::vector<uint32_t> texLoadPlanets;
	std::vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<planetStates.size();++i)
	{
		const float radius = planetParams[i].bodyParam.radius;
		const double dist = glm::distance(viewPos, planetStates[i].position)/radius;
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

	const glm::mat4 projMat = glm::perspective(fovy, windowWidth/(float)windowHeight, 1.f, (float)5e6);
	const glm::mat4 viewMat = glm::lookAt(glm::vec3(0), (glm::vec3)(viewCenter-viewPos), viewUp);

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
		const glm::vec3 planetPos = planetStates[i].position - viewPos;

		// Planet rotation
		const glm::vec3 north = glm::vec3(0,0,1);
		const glm::vec3 rotAxis = planetParams[i].bodyParam.rotationAxis;
		const glm::quat q = glm::rotate(glm::quat(), 
			(float)acos(glm::dot(north, rotAxis)), 
			glm::cross(north, rotAxis))*
			glm::rotate(glm::quat(), planetStates[i].rotationAngle, north);

		// Model matrix
		const glm::mat4 modelMat = 
			glm::translate(glm::mat4(), planetPos)*
			glm::mat4_cast(q)*
			glm::scale(glm::mat4(), glm::vec3(planetParams[i].bodyParam.radius));

		// Light direction
		const glm::vec3 lightDir = 
			(glm::length(planetStates[i].position) > 0.1)?
				glm::vec3(glm::normalize(-planetStates[i].position)):
				glm::vec3(0.f);

		planetUBOs[i].modelMat = modelMat;
		planetUBOs[i].lightDir = viewMat*glm::vec4(lightDir,0.0);
		planetUBOs[i].cloudDisp = planetStates[i].cloudDisp;
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
		loadDDSTexture(planetDiffuseTextures[i], planetParams[i].assetPaths.diffuseFilename);
		loadDDSTexture(planetCloudTextures[i]  , planetParams[i].assetPaths.cloudFilename);
		loadDDSTexture(planetNightTextures[i]  , planetParams[i].assetPaths.nightFilename);

		planetTexLoaded[i] = true;
	}

	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		unloadDDSTexture(planetDiffuseTextures[i], diffuseTexDefault);
		unloadDDSTexture(planetCloudTextures[i]  , cloudTexDefault);
		unloadDDSTexture(planetNightTextures[i]  , nightTexDefault);

		planetTexLoaded[i] = false;
	}

	// Planet sorting from front to back
	std::sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		const float distI = glm::distance(planetStates[i].position, viewPos);
		const float distJ = glm::distance(planetStates[j].position, viewPos);
		return distI < distJ;
	});

	renderGBuffer(closePlanets, currentDynamicOffsets);
	renderHdr(closePlanets, currentDynamicOffsets);
	renderResolve();

	frameId = (frameId+1)%3;
}

void RendererGL::renderGBuffer(
	const std::vector<uint32_t> closePlanets, 
	const DynamicOffsets currentDynamicOffsets)
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_CLAMP);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	// Clearing
	glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
	glClearNamedFramebufferfi(gbufferFbo, GL_DEPTH_STENCIL, 0, 1.f, 0);

	// Planet rendering
	glUseProgram(programPlanetGbuffer.getId());

	// Bind Scene UBO 
	glBindBufferRange(
		GL_UNIFORM_BUFFER, 0, dynamicBuffer,
		currentDynamicOffsets.sceneUBO, sizeof(SceneDynamicUBO));

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	for (uint32_t i : closePlanets)
	{
		// Stencil value for planet
		const uint8_t stencilValue = ((i+1)&0xFF);

		glStencilFunc(GL_ALWAYS, stencilValue, 0xFF);

		// Bind planet UBO
		glBindBufferRange(
			GL_UNIFORM_BUFFER, 1, dynamicBuffer,
			currentDynamicOffsets.planetUBOs[i],
			sizeof(PlanetDynamicUBO));
		// Draw
		render(vertexArray, planetModels[i]);
	}

	// Skybox rendering
	glUseProgram(programSkyboxGbuffer.getId());
	
	glEnable(GL_DEPTH_CLAMP);
	glDepthFunc(GL_LEQUAL);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	
	glBindBufferRange(
		GL_UNIFORM_BUFFER, 1, dynamicBuffer,
		currentDynamicOffsets.skyboxUBO,
		sizeof(PlanetDynamicUBO));
	render(vertexArray, sphere);

	glDisable(GL_DEPTH_CLAMP);
}

void RendererGL::renderHdr(
	const std::vector<uint32_t> closePlanets,
	const DynamicOffsets currentDynamicOffsets)
{
	// No depth test/write
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Samplers
	glBindSampler(6, diffuseSampler);
	glBindSampler(7, diffuseSampler);
	glBindSampler(8, diffuseSampler);

	// Clearing
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
	float clearColor[] = {0.f,0.f,0.f,0.f};
	glClearNamedFramebufferfv(hdrFbo, GL_COLOR, 0, clearColor);

	// Deferred output bind
	const std::vector<GLuint> samplers(gbufferTex.size(), attachmentSampler);
	glBindSamplers(2, samplers.size(), samplers.data());
	glBindTextures(2, gbufferTex.size(), gbufferTex.data());

	// Skybox Rendering
	glBindTextureUnit(6, skyboxTex);
	glStencilFunc(GL_EQUAL, 0, 0xFF);
	glUseProgram(programSkyboxDeferred.getId());

	render(vertexArray, fullscreenTri);

	// Planet rendering
	for (uint32_t i : closePlanets)
	{
		glStencilFunc(GL_EQUAL, ((i+1)&0xFF), 0xFF);
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, dynamicBuffer,
			currentDynamicOffsets.planetUBOs[i],
			sizeof(PlanetDynamicUBO));
		glBindTextureUnit(6, planetDiffuseTextures[i]);
		glBindTextureUnit(7, planetCloudTextures[i]);
		glBindTextureUnit(8, planetNightTextures[i]);
		// If the planet is a star, use same shader as skybox
		glUseProgram(
			((planetParams[i].bodyParam.isStar)?
				programSkyboxDeferred:
				programPlanetDeferred).getId());

		render(vertexArray, fullscreenTri);
	}
}

void RendererGL::renderResolve()
{
	// No stencil test/write
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// No depth test/write
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindSampler(1, attachmentSampler);
	glBindTextureUnit(1, hdrTex);

	glUseProgram(programResolve.getId());
	render(vertexArray, fullscreenTri);
}

void RendererGL::render(GLuint vertexArray, const Model m)
{
	glBindVertexArray(vertexArray);
	glDrawElementsBaseVertex(GL_TRIANGLES, m.count, GL_UNSIGNED_INT,
		(void*)(intptr_t)m.indexOffset, m.vertexOffset/sizeof(Vertex));
}