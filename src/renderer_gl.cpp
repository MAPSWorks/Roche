#include "renderer_gl.hpp"
#include "ddsloader.hpp"

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
	float ambientColor;
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

struct FlareDynamicUBO
{
	glm::mat4 modelMat;
	glm::vec4 color;
	float brightness;
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

void generateFlareModel(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{
	const int detail = 32;
	vertices.resize((detail+1)*2);

	for (int i=0;i<=detail;++i)
	{
		const float f = i/(float)detail;
		const glm::vec2 pos = glm::vec2(cos(f*2*PI),sin(f*2*PI));
		vertices[i*2+0] = {glm::vec4(0,0,0,1), glm::vec4(f, 0, 0.5, 0.5)};
		vertices[i*2+1] = {glm::vec4(pos,0,1), glm::vec4(f, 1, pos*glm::vec2(0.5)+glm::vec2(0.5))};
	}

	indices.resize(detail*6);
	for (int i=0;i<detail;++i)
	{
		indices[i*6+0] = (i*2)+0;
		indices[i*6+1] = (i*2)+1;
		indices[i*6+2] = (i*2)+2;
		indices[i*6+3] = (i*2)+2;
		indices[i*6+4] = (i*2)+1;
		indices[i*6+5] = (i*2)+3;
	}
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

	modelsVertices.resize(3);
	modelsIndices.resize(3);

	const size_t FULLSCREEN_TRI_INDEX = 0;
	const size_t FLARE_MODEL_INDEX = 1;
	const size_t SPHERE_INDEX = 2;

	// Fullscreen tri
	generateFullscreenTri(
		modelsVertices[FULLSCREEN_TRI_INDEX],
		modelsIndices[FULLSCREEN_TRI_INDEX]);

	// Flare
	generateFlareModel(
		modelsVertices[FLARE_MODEL_INDEX],
		modelsIndices[FLARE_MODEL_INDEX]);

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
	flareModel = models[FLARE_MODEL_INDEX];
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
		// Planet UBOs
		dynamicOffsets[i].planetUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			currentOffset = align(currentOffset, uboMinAlign);
			dynamicOffsets[i].planetUBOs[j] = currentOffset;
			currentOffset += sizeof(PlanetDynamicUBO);
		}
		dynamicOffsets[i].flareUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			currentOffset = align(currentOffset, uboMinAlign);
			dynamicOffsets[i].flareUBOs[j] = currentOffset;
			currentOffset += sizeof(FlareDynamicUBO);
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
	createRendertargets();
	createTextures();

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
	objectLabel(GL_TEXTURE, depthStencilTex);
	objectLabel(GL_TEXTURE, hdrMSRendertarget);
	objectLabel(GL_FRAMEBUFFER, hdrFbo);
	objectLabel(GL_PROGRAM, programPlanet.getId());
	objectLabel(GL_PROGRAM, programSun.getId());
	objectLabel(GL_PROGRAM, programTonemap.getId());
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
				tl.width  = texWait.loader.getWidth (tl.mipmap);
				tl.height = texWait.loader.getHeight(tl.mipmap);
				tl.data = imageData;
				// Compute size of current mipmap + offset of next mipmap
				size_t size0;
				texWait.loader.getImageData(tl.mipmap, 1, &size0, nullptr);
				tl.imageSize = size0;
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
	
	// Default cloud tex
	const uint8_t cloudData[] = {255, 255, 255, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &cloudTexDefault);
	glTextureStorage2D(cloudTexDefault, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(cloudTexDefault, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cloudData);

	// Default night tex
	const uint8_t nightData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &nightTexDefault);
	glTextureStorage2D(nightTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(nightTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &nightData);

	createFlare();
}

void RendererGL::createFlare()
{
	const int flareSize = 512;
	const int mipmapCount = 1 + floor(log2(flareSize));
	{
		std::vector<uint16_t> pixelData;
		Renderer::generateFlareIntensityTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareIntensityTex);
		glTextureStorage1D(flareIntensityTex, mipmapCount, GL_R16F, flareSize);
		glTextureSubImage1D(flareIntensityTex, 0, 0, flareSize, GL_RED, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareIntensityTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareIntensityTex);
	}
	{
		std::vector<uint8_t> pixelData;
		Renderer::generateFlareLinesTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_2D, 1, &flareLinesTex);
		glTextureStorage2D(flareLinesTex, mipmapCount, GL_R8, flareSize, flareSize);
		glTextureSubImage2D(flareLinesTex, 0, 0, 0, flareSize, flareSize, GL_RED, GL_UNSIGNED_BYTE, pixelData.data());
		glGenerateTextureMipmap(flareLinesTex);
		glBindTextureUnit(0, flareLinesTex);
	}
	{
		std::vector<uint16_t> pixelData;
		Renderer::generateFlareHaloTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareHaloTex);
		glTextureStorage1D(flareHaloTex, mipmapCount, GL_RGBA16F, flareSize);
		glTextureSubImage1D(flareHaloTex, 0, 0, flareSize, GL_RGBA, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareHaloTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareHaloTex);
	}
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

void RendererGL::createRendertargets()
{
	// Depth stencil texture
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthStencilTex);
	glTextureStorage2DMultisample(
		depthStencilTex, msaaSamples, GL_DEPTH24_STENCIL8, windowWidth, windowHeight, GL_FALSE);

	// HDR MSAA Rendertarget
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &hdrMSRendertarget);
	glTextureStorage2DMultisample(hdrMSRendertarget, msaaSamples, GL_R11F_G11F_B10F,
		windowWidth, windowHeight, GL_FALSE);

	// HDR Rendertarget
	glCreateTextures(GL_TEXTURE_2D, 1, &hdrRendertarget);
	glTextureStorage2D(hdrRendertarget, 1, GL_R11F_G11F_B10F, windowWidth, windowHeight);
	glTextureParameteri(hdrRendertarget, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTextureParameteri(hdrRendertarget, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Highpass rendertargets
	glCreateTextures(GL_TEXTURE_2D, 5, highpassRendertargets);
	for (int i=0;i<5;++i)
	{
		const GLuint tex = highpassRendertargets[i];
		glTextureStorage2D(tex, 1, GL_R11F_G11F_B10F, windowWidth>>i, windowHeight>>i);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	// Bloom rendertargets
	glCreateTextures(GL_TEXTURE_2D, 4, bloomRendertargets);
	for (int i=0;i<4;++i)
	{
		const GLuint tex = bloomRendertargets[i];
		glTextureStorage2D(tex, 1, GL_R11F_G11F_B10F, windowWidth>>(i+1), windowHeight>>(i+1));
		glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	// Bloom applied rendertarget
	glCreateTextures(GL_TEXTURE_2D, 1, &appliedBloomRendertarget);
	glTextureStorage2D(appliedBloomRendertarget, 1, GL_R11F_G11F_B10F, windowWidth, windowHeight);

	// Framebuffers
	glCreateFramebuffers(1, &hdrFbo);
	glNamedFramebufferTexture(hdrFbo, GL_COLOR_ATTACHMENT0, hdrMSRendertarget, 0);
	glNamedFramebufferTexture(hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);

	glCreateFramebuffers(1, &appliedBloomFbo);
	glNamedFramebufferTexture(appliedBloomFbo, GL_COLOR_ATTACHMENT0, appliedBloomRendertarget, 0);
}

void RendererGL::createShaders()
{
	Shader planetVert(GL_VERTEX_SHADER, "shaders/planet.vert");
	Shader deferredVert(GL_VERTEX_SHADER, "shaders/deferred.vert");

	programSun.addShader(planetVert);
	programSun.addShader(GL_FRAGMENT_SHADER, "shaders/sun.frag");
	programSun.compileAndLink();

	programPlanet.addShader(planetVert);
	programPlanet.addShader(GL_FRAGMENT_SHADER, "shaders/planet.frag");
	programPlanet.compileAndLink();

	programResolve.addShader(GL_COMPUTE_SHADER, "shaders/resolve.comp");
	programResolve.compileAndLink();

	programHighpass.addShader(GL_COMPUTE_SHADER, "shaders/highpass.comp");
	programHighpass.compileAndLink();

	programDownsample.addShader(GL_COMPUTE_SHADER, "shaders/downsample.comp");
	programDownsample.compileAndLink();

	programBlurW.addShader(GL_COMPUTE_SHADER, "shaders/blur_w.comp");
	programBlurW.compileAndLink();

	programBlurH.addShader(GL_COMPUTE_SHADER, "shaders/blur_h.comp");
	programBlurH.compileAndLink();

	programBloomAdd.addShader(GL_COMPUTE_SHADER, "shaders/bloom_add.comp");
	programBloomAdd.compileAndLink();

	programBloomApply.addShader(GL_COMPUTE_SHADER, "shaders/bloom_apply.comp");
	programBloomApply.compileAndLink();

	programFlare.addShader(GL_VERTEX_SHADER, "shaders/flare.vert");
	programFlare.addShader(GL_FRAGMENT_SHADER, "shaders/flare.frag");
	programFlare.compileAndLink();

	programTonemap.addShader(deferredVert);
	programTonemap.addShader(GL_FRAGMENT_SHADER, "shaders/tonemap.frag");
	programTonemap.compileAndLink();
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
		glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
		const float ambientColor,
		const std::vector<PlanetState> planetStates)
{
	const float closePlanetMinSizePixels = 1;
	const float closePlanetMaxDistance = windowHeight/(closePlanetMinSizePixels*tan(fovy/2));
	const float farPlanetMinDistance = closePlanetMaxDistance*0.35;
	const float farPlanetOptimalDistance = closePlanetMaxDistance*2.0;
	const float texLoadDistance = closePlanetMaxDistance*1.4;
	const float texUnloadDistance = closePlanetMaxDistance*1.6;

	// Triple buffer of dynamic UBO
	const uint32_t nextFrameId = (frameId+1)%3;
	const auto &currentDynamicOffsets = dynamicOffsets[frameId];
	const auto &nextDynamicOffsets = dynamicOffsets[nextFrameId]; 

	// Projection and view matrices
	const glm::mat4 projMat = glm::perspective(fovy, windowWidth/(float)windowHeight, 1.f, (float)5e10);
	const glm::mat4 viewMat = glm::lookAt(glm::vec3(0), (glm::vec3)(viewCenter-viewPos), viewUp);

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

		// Don't render planets behind the view
		if ((viewMat*glm::vec4(planetStates[i].position - viewPos,1.0)).z < 0)
		{
			if (dist < closePlanetMaxDistance)
			{
				// Detailed model
				closePlanets.push_back(i);
			}
			if (dist > farPlanetMinDistance || planetParams[i].bodyParam.isStar)
			{
				// Flares
				farPlanets.push_back(i);
			}
		}
	}

	const float exp = glm::pow(2, exposure);

	// Scene uniform update
	SceneDynamicUBO sceneUBO;
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.viewPos = glm::vec4(0.0,0.0,0.0,1.0);
	sceneUBO.ambientColor = ambientColor;
	sceneUBO.invGamma = 1.f/gamma;
	sceneUBO.exposure = exp;

	// Planet uniform update
	// Close planets (detailed model)
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
	// Far away planets (flare)
	std::vector<FlareDynamicUBO> flareUBOs(planetCount);
	for (uint32_t i : farPlanets)
	{
		const glm::vec3 planetPos = glm::vec3(planetStates[i].position - viewPos);
		const glm::vec4 clip = projMat*viewMat*glm::vec4(planetPos,1.0);
		const glm::vec2 screen = glm::vec2(clip)/clip.w;
		const float FLARE_SIZE_DEGREES = 20.0;
		const glm::mat4 modelMat = 
			glm::translate(glm::mat4(), glm::vec3(screen, 0.0))*
			glm::scale(glm::mat4(), 
				glm::vec3(windowHeight/(float)windowWidth,1.0,0.0)*
				FLARE_SIZE_DEGREES*(float)PI/(fovy*180.0f));

		const float phaseAngle = glm::acos(glm::dot(
			(glm::vec3)glm::normalize(planetStates[i].position), 
			glm::normalize(planetPos)));
		const float phase = (1-phaseAngle/PI)*cos(phaseAngle)+(1/PI)*sin(phaseAngle);
		const bool isStar = planetParams[i].bodyParam.isStar;
		const float radius = planetParams[i].bodyParam.radius;
		const double dist = glm::distance(viewPos, planetStates[i].position)/radius;
		const float fade = glm::clamp(isStar?
			(float)   dist/10:
			(float) ((dist-farPlanetMinDistance)/
							(farPlanetOptimalDistance-farPlanetMinDistance)),0.f,1.f);
		const glm::vec3 cutDist = planetPos*0.005f;
		
		const float brightness = std::min(4.0f,
			exp*
			radius*radius*
			(isStar?100000.f:
			planetParams[i].bodyParam.albedo*0.2f*phase
			/glm::dot(cutDist,cutDist)))
			*fade;

		flareUBOs[i].modelMat = modelMat;
		flareUBOs[i].color = glm::vec4(planetParams[i].bodyParam.meanColor,1.0);
		flareUBOs[i].brightness = brightness;
	}

	// Dynamic data upload
	fences[nextFrameId].wait();

	memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.sceneUBO, 
		&sceneUBO, sizeof(SceneDynamicUBO));
	for (uint32_t i=0;i<planetUBOs.size();++i)
	{
		memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.planetUBOs[i], 
			&planetUBOs[i], sizeof(PlanetDynamicUBO));
	}
	for (uint32_t i=0;i<planetUBOs.size();++i)
	{
		memcpy((char*)dynamicBufferPtr+nextDynamicOffsets.flareUBOs[i],
			&flareUBOs[i], sizeof(FlareDynamicUBO));
	}

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
					texLoaded.imageSize, texLoaded.data->data()+texLoaded.mipmapOffset);
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

	renderHdr(previousFrameClosePlanets, currentDynamicOffsets);
	renderResolve();
	renderBloom();
	renderFlares(previousFrameFarPlanets, currentDynamicOffsets);
	renderTonemap(currentDynamicOffsets);

	frameId = (frameId+1)%3;
	previousFrameClosePlanets = closePlanets;
	previousFrameFarPlanets = farPlanets;
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
	// No blending
	glDisable(GL_BLEND);

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
			programSun:
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
}

void RendererGL::renderResolve()
{
	const int workgroupSize = 16;
	glUseProgram(programResolve.getId());
	glBindImageTexture(0, hdrMSRendertarget, 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
	glBindImageTexture(1, hdrRendertarget  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
	glDispatchCompute(
		(int)ceil(windowWidth/(float)workgroupSize), 
		(int)ceil(windowHeight/(float)workgroupSize), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void RendererGL::renderBloom()
{
	const int workgroupSize = 16;
	// Highpass
	glUseProgram(programHighpass.getId());
	glBindImageTexture(0, hdrRendertarget         , 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
	glBindImageTexture(1, highpassRendertargets[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
	glDispatchCompute(
		(int)ceil(windowWidth/(float)workgroupSize), 
		(int)ceil(windowHeight/(float)workgroupSize), 1);

	glUseProgram(programDownsample.getId());
	// Downsample to 16x
	for (int i=0;i<4;++i)
	{
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glBindImageTexture(0, highpassRendertargets[i]  , 0, GL_FALSE, 0, GL_READ_ONLY, GL_R11F_G11F_B10F);
		glBindImageTexture(1, highpassRendertargets[i+1], 0, GL_FALSE, 0, GL_WRITE_ONLY,GL_R11F_G11F_B10F);
		glDispatchCompute(
			(int)ceil(windowWidth /(float)(workgroupSize<<(i+1))), 
			(int)ceil(windowHeight/(float)(workgroupSize<<(i+1))), 1);
	}

	const GLuint bloomImages[] = {
		highpassRendertargets[4], // Blur x input  - 16x
		bloomRendertargets[3],    // Blur y input
		highpassRendertargets[3], // Add image input
		bloomRendertargets[2],    // ...           - 8x
		highpassRendertargets[3],
		highpassRendertargets[2],
		bloomRendertargets[1],    // ...           - 4x
		highpassRendertargets[2],
		highpassRendertargets[1],
		bloomRendertargets[0],   // Only blur      - 2x
		highpassRendertargets[1]
	};

	for (int i=0;i<4;++i)
	{
		const int dispatchSize = workgroupSize<<(4-i);

		// Blur horizontally & vertically
		const GLuint blurImages[] = {bloomImages[i*3+0], bloomImages[i*3+1]};
		const int blurPasses = 2;
		for (int j=0;j<2;++j)
		{
			glUseProgram((j?programBlurW:programBlurH).getId());
			for (int k=0;k<blurPasses;++k)
			{
				const int ping = (j*blurPasses+k)%2;
				const int pong = (ping+1)%2;
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				glBindImageTexture(0, blurImages[ping], 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
				glBindImageTexture(1, blurImages[pong], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
				glDispatchCompute(
					(int)ceil(windowWidth /(float)dispatchSize),
					(int)ceil(windowHeight/(float)dispatchSize), 1);
			}
		}

		// Add blur with higher res
		if (i!=3)
		{
			glUseProgram(programBloomAdd.getId());
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			glBindTextureUnit(0, bloomImages[i*3+0]);
			glBindImageTexture(1, bloomImages[i*3+2], 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
			glBindImageTexture(2, bloomImages[i*3+3], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
			glDispatchCompute(
				(int)ceil(2*windowWidth /(float)dispatchSize),
				(int)ceil(2*windowHeight/(float)dispatchSize), 1);
		}
	}
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glUseProgram(programBloomApply.getId());
	glBindImageTexture(0, hdrRendertarget, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R11F_G11F_B10F);
	glBindTextureUnit(1, bloomRendertargets[0]);
	glBindImageTexture(2, appliedBloomRendertarget, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
	glDispatchCompute(
		(int)ceil(windowWidth /(float)workgroupSize),
		(int)ceil(windowHeight/(float)workgroupSize), 1);

	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void RendererGL::renderFlares(
	const std::vector<uint32_t> farPlanets, 
	const DynamicOffsets currentDynamicOffsets)
{
	// No depth test/writes
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Blending add
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, appliedBloomFbo);

	glUseProgram(programFlare.getId());

	for (uint32_t i : farPlanets)
	{
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, dynamicBuffer,
			currentDynamicOffsets.sceneUBO,
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, dynamicBuffer,
			currentDynamicOffsets.flareUBOs[i],
			sizeof(PlanetDynamicUBO));

		// Bind textures
		glBindTextureUnit(2, flareIntensityTex);
		glBindTextureUnit(3, flareLinesTex);
		glBindTextureUnit(4, flareHaloTex);

		render(vertexArray, flareModel);
	}
}

void RendererGL::renderTonemap(const DynamicOffsets currentDynamicOffsets)
{
	// No stencil test/write
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// No depth test/write
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(programTonemap.getId());

	// Bind Scene UBO
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, dynamicBuffer,
			currentDynamicOffsets.sceneUBO,
			sizeof(SceneDynamicUBO));

	// Bind image after bloom is done
	glBindTextureUnit(1, appliedBloomRendertarget);

	render(vertexArray, fullscreenTri);
}

void RendererGL::render(GLuint vertexArray, const Model m)
{
	glBindVertexArray(vertexArray);
	glDrawElementsBaseVertex(GL_TRIANGLES, m.count, GL_UNSIGNED_INT,
		(void*)(intptr_t)m.indexOffset, m.vertexOffset/sizeof(Vertex));
}