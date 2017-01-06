#include "renderer_gl.hpp"
#include "util.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define PI 3.14159265358979323846264338327950288

const bool USE_COHERENT_MEMORY = false;

// Debug output callback
#ifdef USE_KHR_DEBUG
#define objectLabel(identifier, name) glObjectLabel(identifier, name, 0, #name)

#include <fstream>
#include <sstream>
std::ofstream debugLog("gl_log.txt", std::ios::out | std::ios::binary | std::ios::trunc);

void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
   GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	std::stringstream ss;
	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR: ss << "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: ss << "Decrecated behavior"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: ss << "Undefined behavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY: ss << "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE: ss << "Performance"; break;
		case GL_DEBUG_TYPE_MARKER: ss << "Marker"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP: ss << "Push group"; break;
		case GL_DEBUG_TYPE_POP_GROUP: ss << "Pop group"; break;
		case GL_DEBUG_TYPE_OTHER: ss << "Other"; break;
	}

	ss << " (";

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH: ss << "high"; break;
		case GL_DEBUG_SEVERITY_MEDIUM: ss << "medium"; break;
		case GL_DEBUG_SEVERITY_LOW: ss << "low"; break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: ss << "notification"; break;
	}

	ss << "): " << std::string(message) << std::endl;

	debugLog << ss.str();
}
#endif


void RendererGL::windowHints()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
}

struct Vertex
{
	glm::vec4 position;
	glm::vec4 uv;
	glm::vec4 normal;
	glm::vec4 tangent;
};

struct SceneDynamicUBO
{
	glm::mat4 projMat;
	glm::mat4 viewMat;
	glm::vec4 viewPos;
	float invGamma;
	float exposure;
};

struct PlanetDynamicUBO
{
	glm::mat4 modelMat;
	glm::vec4 lightDir;
	float albedo;
	float cloudDisp;
	float nightTexIntensity;
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
			glm::vec3 pos = glm::vec3(cp*ct,cp*st,sp);
			glm::vec3 normal = glm::normalize(pos);
			glm::vec3 tangent = glm::cross(normal, glm::vec3(0,0,1));
			vertices[offset] = {
				glm::vec4(pos,1), 
				glm::vec4((float)j/(float)meridians, 1.f-(float)i/(float)rings,0.0,0.0),
				glm::vec4(normal,0),
				glm::vec4(tangent,0)
			};
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

GLenum DDSFormatToGL(DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
		case DDSLoader::Format::BC2: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
		case DDSLoader::Format::BC3: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
	}
	return 0;
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
	const int planetMeridians = 128;
	const int planetRings = 128;
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

	createVertexArray();
	createShaders();
	createRenderTargets();
	createTextures();
	createSkybox(skyboxParam);

#ifdef USE_KHR_DEBUG
	// Debug output
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugCallback, nullptr);
	glDebugMessageInsert(
		GL_DEBUG_SOURCE_APPLICATION, 
		GL_DEBUG_TYPE_OTHER, 0, 
		GL_DEBUG_SEVERITY_NOTIFICATION, 
		0, "Debug callback enabled");

	// Object labels
	objectLabel(GL_BUFFER, staticBuffer);
	objectLabel(GL_BUFFER, dynamicBuffer);
	objectLabel(GL_VERTEX_ARRAY, vertexArray);
	objectLabel(GL_SAMPLER, attachmentSampler);
	objectLabel(GL_TEXTURE, depthStencilTex);
	objectLabel(GL_TEXTURE, hdrTex);
	objectLabel(GL_FRAMEBUFFER, hdrFbo);
	objectLabel(GL_PROGRAM, programPlanet.getId());
	objectLabel(GL_PROGRAM, programSkybox.getId());
	objectLabel(GL_PROGRAM, programResolve.getId());
	objectLabel(GL_TEXTURE, diffuseTexDefault);
	objectLabel(GL_TEXTURE, cloudTexDefault);
	objectLabel(GL_TEXTURE, nightTexDefault);
#endif

	// Texture loading thread
	texLoadThread = std::thread([&,this]()
	{
		while (true)
		{
			// Wait for kill or queue not empty
			{
				std::unique_lock<std::mutex> lk(texWaitMutex);
				texWaitCondition.wait(lk, [&]{ return killThread || !texWaitQueue.empty();});
			}

			if (killThread) return;

			// Get info about texture were are going to load
			TexWait texWait;
			{
				std::lock_guard<std::mutex> lk(texWaitMutex);
				texWait = texWaitQueue.front();
				texWaitQueue.pop();
			}

			std::shared_ptr<std::vector<uint8_t>> imageData = std::make_shared<std::vector<uint8_t>>();

			// Load image data
			size_t imageSize;
			texWait.loader.getImageData(texWait.mipmap, texWait.mipmapCount, &imageSize, nullptr);
			imageData->resize(imageSize);
			texWait.loader.getImageData(texWait.mipmap, texWait.mipmapCount, &imageSize, imageData->data());

			std::vector<TexLoaded> texLoaded(texWait.mipmapCount);
			size_t offset = 0;

			for (unsigned int i=0;i<texLoaded.size();++i)
			{
				auto &tl = texLoaded[i];
				tl.tex = texWait.tex;
				tl.mipmap = texWait.mipmap+i;
				tl.mipmapOffset = offset;
				tl.format = DDSFormatToGL(texWait.loader.getFormat());
				tl.width  = texWait.loader.getWidth (texWait.mipmap);
				tl.height = texWait.loader.getHeight(texWait.mipmap);
				tl.data = imageData;
				// Compute offset of next mipmap
				size_t size0;
				texWait.loader.getImageData(texWait.mipmap+i, 1, &size0, nullptr);
				offset += size0;
			}
			
			// Push loaded texture into queue
			{
				std::lock_guard<std::mutex> lk(texLoadedMutex);
				for (auto tl : texLoaded)
					texLoadedQueue.push(tl);
			}
		}
	});
}

void RendererGL::createTextures()
{
	// Anisotropy
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

	const float requestedAnisotropy = 16.f;
	textureAnisotropy = (requestedAnisotropy > maxAnisotropy)?maxAnisotropy:requestedAnisotropy;

	// Texture init
	planetTexLoaded.resize(planetCount);

	// All default textures
	planetDiffuseTextures.resize(planetCount, -1);
	planetCloudTextures.resize(planetCount, -1);
	planetNightTextures.resize(planetCount, -1);

	// Default diffuse tex
	const uint8_t diffuseData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &diffuseTexDefault);
	glTextureStorage2D(diffuseTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(diffuseTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &diffuseData);
	glTextureParameteri(diffuseTexDefault, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// Default cloud tex
	const uint8_t cloudData[] = {255, 255, 255, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &cloudTexDefault);
	glTextureStorage2D(cloudTexDefault, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(cloudTexDefault, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cloudData);
	glTextureParameteri(cloudTexDefault, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Default night tex
	const uint8_t nightData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &nightTexDefault);
	glTextureStorage2D(nightTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(nightTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &nightData);
	glTextureParameteri(nightTexDefault, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	
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

	const int VERTEX_ATTRIB_POS     = 0;
	const int VERTEX_ATTRIB_UV      = 1;
	const int VERTEX_ATTRIB_NORMAL  = 2;
	const int VERTEX_ATTRIB_TANGENT = 3;

	// Position
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_POS, 4, GL_FLOAT, false, offsetof(Vertex, position));

	// UVs
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_UV, 4, GL_FLOAT, false, offsetof(Vertex, uv));

	// Normals
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_NORMAL);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_NORMAL, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_NORMAL, 4, GL_FLOAT, false, offsetof(Vertex, normal));

	// Tangents
	glEnableVertexArrayAttrib(vertexArray, VERTEX_ATTRIB_TANGENT);
	glVertexArrayAttribBinding(vertexArray, VERTEX_ATTRIB_TANGENT, VERTEX_BINDING);
	glVertexArrayAttribFormat(vertexArray, VERTEX_ATTRIB_TANGENT, 4, GL_FLOAT, false, offsetof(Vertex, tangent));
}

void RendererGL::createRenderTargets()
{
	// Sampler
	glCreateSamplers(1, &attachmentSampler);
	glSamplerParameteri(attachmentSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(attachmentSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Depth stencil texture
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthStencilTex);
	glTextureStorage2DMultisample(
		depthStencilTex, msaaSamples, GL_DEPTH24_STENCIL8, windowWidth, windowHeight, GL_FALSE);

	// HDR
	// Texture
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &hdrTex);
	glTextureStorage2DMultisample(hdrTex, msaaSamples, GL_RGB16F,
		windowWidth, windowHeight, GL_FALSE);

	// Framebuffer
	glCreateFramebuffers(1, &hdrFbo);
	glNamedFramebufferTexture(hdrFbo, GL_COLOR_ATTACHMENT0, hdrTex, 0);
	glNamedFramebufferTexture(hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);
}

void RendererGL::createShaders()
{
	programSkybox.source(GL_VERTEX_SHADER, "shaders/planet.vert");
	programSkybox.source(GL_FRAGMENT_SHADER, "shaders/skybox_forward.frag");
	programSkybox.link();

	programPlanet.source(GL_VERTEX_SHADER, "shaders/planet.vert");
	programPlanet.source(GL_FRAGMENT_SHADER, "shaders/planet_forward.frag");
	programPlanet.link();

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
	skyboxTex = -1;
	skyboxTex = loadDDSTexture(skyboxParam.textureFilename, glm::vec4(0,0,0,1));
	skyboxIntensity = skyboxParam.intensity;
}

void RendererGL::destroy()
{
	if (glUnmapNamedBuffer(dynamicBuffer) == GL_FALSE)
	{
		throw std::runtime_error("Staging buffer memory corruption");
	}
	glDeleteBuffers(1, &staticBuffer);
	glDeleteBuffers(1, &dynamicBuffer);

	// Kill thread
	{
		std::lock_guard<std::mutex> lk(texWaitMutex);
		killThread = true;
	}
	texWaitCondition.notify_one();
}

RendererGL::TexHandle RendererGL::createStreamTexture(const GLuint id)
{
	TexHandle handle = nextHandle;
	streamTextures[handle] = id;
	nextHandle++;
	if (nextHandle == -1) nextHandle = 0;
	return handle;
}

bool RendererGL::getStreamTexture(const TexHandle tex, GLuint &id)
{
	auto it = streamTextures.find(tex);
	if (it != streamTextures.end())
	{
		id = it->second;
		return true;
	}
	return false;
}

void RendererGL::removeStreamTexture(const TexHandle tex)
{
	auto it = streamTextures.find(tex);
	if (it != streamTextures.end())
	{
		glDeleteTextures(1, &(it->second));
		streamTextures.erase(it);
	}
}

/// Converts a RGBA color to a BC1, BC2 or BC3 block (roughly, without interpolation)
void RGBAToBC(const glm::vec4 color, DDSLoader::Format format, std::vector<uint8_t> &block)
{
	// RGB8 to 5_6_5
	uint16_t color0 = 
		(((int)(color.r*0x1F)&0x1F)<<11) | 
		(((int)(color.g*0x3F)&0x3F)<<5) | 
		((int)(color.b*0x1F)&0x1F);

	if (format == DDSLoader::Format::BC1)
	{
		block.resize(8);
	}
	else 
	{
		block.resize(16);
		if (format == DDSLoader::Format::BC2)
		{
			int alpha = (color.a*0xF);
			uint8_t alphaCodes = (alpha << 4) | alpha;
			for (int i=8;i<16;++i) block[i] = alphaCodes;
		}
		else if (format == DDSLoader::Format::BC3)
		{
			uint8_t alpha = (color.a*0xFF);
			block[8] = alpha;
			block[9] = alpha;
		}
	}

	block[0] = color0&0xFF;
	block[1] = (color0>>8)&0xFF;
	block[2] = color0&0xFF;
	block[3] = (color0>>8)&0xFF;
	char codes = (color.a > 0.5)?0x00:0xFF;
	for (int i=4;i<8;++i) block[i] = codes;
}

RendererGL::TexHandle RendererGL::loadDDSTexture(
	const std::string filename, 
	const glm::vec4 defaultColor)
{
	DDSLoader loader;
	if (loader.open(filename))
	{
		const int mipmapCount = loader.getMipmapCount();
		// Create texture
		GLuint id;
		glCreateTextures(GL_TEXTURE_2D, 1, &id);
		glTextureStorage2D(id, 
			mipmapCount, DDSFormatToGL(loader.getFormat()), 
			loader.getWidth(0), loader.getHeight(0));
		glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
		glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		// Default color at highest mipmap
		glTextureParameterf(id, GL_TEXTURE_MIN_LOD, (float)(mipmapCount-1));
		std::vector<uint8_t> block;
		RGBAToBC(defaultColor, loader.getFormat(), block);
		glCompressedTextureSubImage2D(id, mipmapCount-1, 0, 0, 1, 1, 
			DDSFormatToGL(loader.getFormat()), block.size(), block.data());
		// Create texture handle
		TexHandle handle = createStreamTexture(id);

		// Create jobs
		const int groupLoadMipmap = 8; // Number of highest mipmaps to load together
		int jobCount = std::max(mipmapCount-groupLoadMipmap+1, 1);
		std::vector<TexWait> texWait(jobCount);
		for (int i=0;i<texWait.size();++i)
		{
			texWait[i] = {handle, jobCount-i-1, (i)?1:std::min(groupLoadMipmap,mipmapCount), loader};
		}

		// Push jobs to queue
		{
			std::lock_guard<std::mutex> lk(texWaitMutex);
			for (auto tw : texWait)
			{
				texWaitQueue.push(tw);
			}
		}
		// Wake up loading thread
		texWaitCondition.notify_one();
		return handle;
	}
	return -1;
}

void RendererGL::unloadDDSTexture(TexHandle tex)
{
	GLuint id;
	if (getStreamTexture(tex, id))
		glDeleteTextures(1, &id);
}

void RendererGL::render(
		const glm::dvec3 viewPos, 
		const float fovy,
		const glm::dvec3 viewCenter,
		const glm::vec3 viewUp,
		const float gamma,
		const float exposure,
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
	sceneUBO.exposure = glm::pow(2, exposure);

	// Skybox uniform update
	PlanetDynamicUBO skyboxUBO;
	skyboxUBO.modelMat = skyboxModelMat;
	skyboxUBO.lightDir = glm::vec4(0.0);
	skyboxUBO.albedo = skyboxIntensity;

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
		planetUBOs[i].nightTexIntensity = planetParams[i].bodyParam.nightTexIntensity;
		planetUBOs[i].albedo = planetParams[i].bodyParam.albedo;
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
		planetDiffuseTextures[i] = loadDDSTexture(planetParams[i].assetPaths.diffuseFilename, glm::vec4(1,1,1,1));
		planetCloudTextures[i]   = loadDDSTexture(planetParams[i].assetPaths.cloudFilename  , glm::vec4(0,0,0,0));
		planetNightTextures[i]   = loadDDSTexture(planetParams[i].assetPaths.nightFilename  , glm::vec4(0,0,0,0));

		planetTexLoaded[i] = true;
	}

	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		unloadDDSTexture(planetDiffuseTextures[i]);
		unloadDDSTexture(planetCloudTextures[i]);
		unloadDDSTexture(planetNightTextures[i]);

		planetDiffuseTextures[i] = -1;
		planetCloudTextures[i] = -1;
		planetNightTextures[i] = -1;

		planetTexLoaded[i] = false;
	}

	// Texture uploading
	{
		std::lock_guard<std::mutex> lk(texLoadedMutex);
		while (!texLoadedQueue.empty())
		{
			TexLoaded texLoaded = texLoadedQueue.front();
			texLoadedQueue.pop();
			GLuint id;
			if (getStreamTexture(texLoaded.tex, id))
			{
				glCompressedTextureSubImage2D(
					id,
					texLoaded.mipmap, 
					0, 0, 
					texLoaded.width, texLoaded.height, 
					texLoaded.format,
					texLoaded.data->size(), texLoaded.data->data());
				glTextureParameterf(id, GL_TEXTURE_MIN_LOD, (float)texLoaded.mipmap);
			}
		}
	}

	// Planet sorting from front to back
	std::sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		const float distI = glm::distance(planetStates[i].position, viewPos);
		const float distJ = glm::distance(planetStates[j].position, viewPos);
		return distI < distJ;
	});

	renderHdr(closePlanets, currentDynamicOffsets);
	renderResolve(currentDynamicOffsets);

	frameId = (frameId+1)%3;
}

void RendererGL::renderHdr(
	const std::vector<uint32_t> closePlanets,
	const DynamicOffsets currentDynamicOffsets)
{
	// Depth test/write
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Clearing
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
	float clearColor[] = {0.f,0.f,0.f,0.f};
	float clearDepth[] = {1.f};
	glClearNamedFramebufferfv(hdrFbo, GL_COLOR, 0, clearColor);
	glClearNamedFramebufferfv(hdrFbo, GL_DEPTH, 0, clearDepth);

	// Chose default or custom textures
	std::vector<GLuint> diffuseTextures(planetCount);
	std::vector<GLuint> cloudTextures(planetCount);
	std::vector<GLuint> nightTextures(planetCount);

	for (uint32_t i : closePlanets)
	{
		GLuint id;
		diffuseTextures[i] = getStreamTexture(planetDiffuseTextures[i], id)?id:diffuseTexDefault;
		cloudTextures[i]   = getStreamTexture(planetCloudTextures[i]  , id)?id:cloudTexDefault;
		nightTextures[i]   = getStreamTexture(planetNightTextures[i]  , id)?id:nightTexDefault;
	}

	// Planet rendering
	for (uint32_t i : closePlanets)
	{
		const bool star = planetParams[i].bodyParam.isStar;
		glUseProgram(
			(star?
			programSkybox:
			programPlanet).getId());

		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, dynamicBuffer,
			currentDynamicOffsets.sceneUBO,
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, dynamicBuffer,
			currentDynamicOffsets.planetUBOs[i],
			sizeof(PlanetDynamicUBO));

		// Bind textures
		glBindTextureUnit(2, diffuseTextures[i]);
		if (!star)
		{
			glBindTextureUnit(3, cloudTextures[i]);
			glBindTextureUnit(4, nightTextures[i]);
		}

		render(vertexArray, planetModels[i]);
	}

	// Skybox Rendering
	glEnable(GL_DEPTH_CLAMP);
	glDepthFunc(GL_LEQUAL);

	glUseProgram(programSkybox.getId());

	// Bind skybox UBO
	glBindBufferRange(
		GL_UNIFORM_BUFFER, 1, dynamicBuffer,
		currentDynamicOffsets.skyboxUBO,
		sizeof(PlanetDynamicUBO));

	// Bind skybox texture
	GLuint skyTexId;
	GLuint skyTex = getStreamTexture(skyboxTex, skyTexId)?skyTexId:diffuseTexDefault;

	glBindTextureUnit(2, skyTex);

	render(vertexArray, sphere);
}

void RendererGL::renderResolve(const DynamicOffsets currentDynamicOffsets)
{
	// No stencil test/write
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// No depth test/write
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(programResolve.getId());

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, dynamicBuffer,
			currentDynamicOffsets.sceneUBO,
			sizeof(SceneDynamicUBO));

	// Bind HDR MS texture
	glBindSampler(1, attachmentSampler);
	glBindTextureUnit(1, hdrTex);

	render(vertexArray, fullscreenTri);
}

void RendererGL::render(GLuint vertexArray, const Model m)
{
	glBindVertexArray(vertexArray);
	glDrawElementsBaseVertex(GL_TRIANGLES, m.count, GL_UNSIGNED_INT,
		(void*)(intptr_t)m.indexOffset, m.vertexOffset/sizeof(Vertex));
}