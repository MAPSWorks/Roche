#include "renderer_gl.hpp"
#include "ddsloader.hpp"

#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using namespace std;

void RendererGL::windowHints()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
}

struct Vertex
{
	vec4 position;
	vec4 uv;
	vec4 normal;
	vec4 tangent;
};

typedef uint32_t Index;

GLenum indexType()
{
	switch (sizeof(Index))
	{
		case 2: return GL_UNSIGNED_SHORT;
		case 1: return GL_UNSIGNED_BYTE;
		default: return GL_UNSIGNED_INT;
	}
}

void generateSphere(
	const int meridians, 
	const int rings, 
	const bool exterior, 
	vector<Vertex> &vertices, 
	vector<Index> &indices)
{
	// Vertices
	vertices.resize((meridians+1)*(rings+1));
	size_t offset = 0;
	for (int i=0;i<=rings;++i)
	{
		const float phi = glm::pi<float>()*((float)i/(float)rings-0.5);
		const float cp = cos(phi);
		const float sp = sin(phi);
		for (int j=0;j<=meridians;++j)
		{
			const float theta = 2*glm::pi<float>()*((float)j/(float)meridians);
			const float ct = cos(theta);
			const float st = sin(theta);
			vec3 pos = vec3(cp*ct,cp*st,sp);
			vec3 normal = normalize(pos);
			vec3 tangent = cross(normal, vec3(0,0,1));
			vertices[offset] = {
				vec4(pos,1), 
				vec4((float)j/(float)meridians, 1.f-(float)i/(float)rings,0.0,0.0),
				vec4(normal,0),
				vec4(tangent,0)
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
			const Index i1 = i+1;
			const Index j1 = j+1;
			vector<Index> ind = {
				(Index)(i *(rings+1)+j),
				(Index)(i1*(rings+1)+j),
				(Index)(i1*(rings+1)+j1),
				(Index)(i1*(rings+1)+j1),
				(Index)(i *(rings+1)+j1),
				(Index)(i *(rings+1)+j)
			};
			// Remove back face culling
			if (!exterior)
				swap(ind[1], ind[4]);

			// Copy to indices
			memcpy(&indices[offset*6], ind.data(), ind.size()*sizeof(Index));
			offset++;
		}
	}
}

void generateFullscreenTri(vector<Vertex> &vertices, vector<Index> &indices)
{
	vertices.resize(3);
	vertices[0].position = vec4(-2,-1,0,1);
	vertices[1].position = vec4( 2,-1,0,1);
	vertices[2].position = vec4( 0, 4,0,1);

	indices = {0,1,2};
}

void generateFlareModel(vector<Vertex> &vertices, vector<Index> &indices)
{
	const int detail = 32;
	vertices.resize((detail+1)*2);

	for (int i=0;i<=detail;++i)
	{
		const float f = i/(float)detail;
		const vec2 pos = vec2(cos(f*2*glm::pi<float>()),sin(f*2*glm::pi<float>()));
		vertices[i*2+0] = {vec4(0,0,0,1), vec4(f, 0, 0.5, 0.5)};
		vertices[i*2+1] = {vec4(pos,0,1), vec4(f, 1, pos*vec2(0.5)+vec2(0.5))};
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

void generateRingModel(
	const int meridians,
	const float near,
	const float far,
	vector<Vertex> &vertices, 
	vector<Index> &indices)
{
	vertices.resize((meridians+1)*2);
	indices.resize(meridians*6);

	{
		int offset = 0;
		for (int i=0;i<=meridians;++i)
		{
			float angle = (glm::pi<float>()*i)/(float)meridians;
			vec2 pos = vec2(cos(angle),sin(angle));
			vertices[offset+0] = {vec4(pos*near, 0.0,1.0), vec4(pos*1.f,0.0,0.0)};
			vertices[offset+1] = {vec4(pos*far , 0.0,1.0), vec4(pos*2.f,0.0,0.0)};
			offset += 2;
		}
	}

	{
		int offset = 0;
		Index vert = 0;
		for (int i=0;i<meridians;++i)
		{
			indices[offset+0] = vert+2;
			indices[offset+1] = vert+0;
			indices[offset+2] = vert+1;
			indices[offset+3] = vert+2;
			indices[offset+4] = vert+1;
			indices[offset+5] = vert+3;
			offset += 6;
			vert += 2; 
		}
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
	const vector<PlanetParameters> planetParams,
	const int msaa,
	const int windowWidth,
	const int windowHeight)
{
	this->planetParams = planetParams;
	this->planetCount = planetParams.size();
	this->msaaSamples = msaa;
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	this->bufferFrames = 3; // triple-buffering

	this->planetData.resize(planetCount);

	vertexBuffer = Buffer(false);
	indexBuffer = Buffer(false);

	createVertexArray();

	// Static vertex & index data
	vector<Vertex> vertices;
	vector<Index> indices;

	// Fullscreen tri
	generateFullscreenTri(vertices, indices);
	fullscreenTri = DrawCommand(vertexArray, GL_TRIANGLES, indexType(),
		sizeof(Vertex), sizeof(Index),
		vertexBuffer.assignVertices(vertices.size(), sizeof(Vertex), vertices.data()),
		indexBuffer.assignIndices(indices.size(), sizeof(Index), indices.data()));

	// Flare
	generateFlareModel(vertices, indices);
	flareModel = DrawCommand(vertexArray, GL_TRIANGLES, indexType(),
		sizeof(Vertex), sizeof(Index),
		vertexBuffer.assignVertices(vertices.size(), sizeof(Vertex), vertices.data()),
		indexBuffer.assignIndices(indices.size(), sizeof(Index), indices.data()));

	// Sphere
	const int planetMeridians = 128;
	const int planetRings = 128;
	generateSphere(planetMeridians, planetRings, false, 
		vertices, indices);
	sphere = DrawCommand(vertexArray, GL_TRIANGLES, indexType(),
		sizeof(Vertex), sizeof(Index),
		vertexBuffer.assignVertices(vertices.size(), sizeof(Vertex), vertices.data()),
		indexBuffer.assignIndices(indices.size(), sizeof(Index), indices.data()));

	// Load custom models
	for (uint32_t i=0;i<planetCount;++i)
	{
		auto &data = planetData[i];
		const PlanetParameters param = planetParams[i];
		if (param.assetPaths.modelFilename != "")
		{
			throw runtime_error("Custom model not supported");
		}
		else
		{
			data.planetModel = sphere;
		}
		// Rings
		if (param.ringParam.hasRings)
		{
			float near = param.ringParam.innerDistance;
			float far = param.ringParam.outerDistance;
			const int ringMeridians = 128;
			generateRingModel(ringMeridians, near, far, 
				vertices, indices);
			data.ringModel = DrawCommand(vertexArray, GL_TRIANGLES, indexType(),
				sizeof(Vertex), sizeof(Index),
				vertexBuffer.assignVertices(vertices.size(), sizeof(Vertex), vertices.data()),
				indexBuffer.assignIndices(indices.size(), sizeof(Index), indices.data()));
		}
	}

	vertexBuffer.write();
	indexBuffer.write();

	// Dynamic UBO buffer assigning
	uboBuffer = Buffer(true);
	dynamicData.resize(bufferFrames); // multiple buffering
	for (auto &data : dynamicData)
	{
		// Scene UBO
		data.sceneUBO = uboBuffer.assignUBO(sizeof(SceneDynamicUBO));
		// Planet UBOs
		data.planetUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			data.planetUBOs[j] = uboBuffer.assignUBO(sizeof(PlanetDynamicUBO));
		}
		// Flare UBOs
		data.flareUBOs.resize(planetCount);
		for (uint32_t j=0;j<planetCount;++j)
		{
			data.flareUBOs[j] = uboBuffer.assignUBO(sizeof(FlareDynamicUBO));
		}
	}

	uboBuffer.write();

	createShaders();
	createRendertargets();
	createTextures();

	initThread();
}

void RendererGL::createTextures()
{
	// Anisotropy
	float maxAnisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

	const float requestedAnisotropy = 16.f;
	textureAnisotropy = (requestedAnisotropy > maxAnisotropy)?maxAnisotropy:requestedAnisotropy;

	// Default diffuse tex
	const uint8_t diffuseData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &diffuseTexDefault);
	glTextureStorage2D(diffuseTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(diffuseTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, diffuseData);
	
	// Default cloud tex
	const uint8_t cloudData[] = {255, 255, 255, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &cloudTexDefault);
	glTextureStorage2D(cloudTexDefault, 1, GL_RGBA8, 1, 1);
	glTextureSubImage2D(cloudTexDefault, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cloudData);

	// Default night tex
	const uint8_t nightData[] = {0, 0, 0};
	glCreateTextures(GL_TEXTURE_2D, 1, &nightTexDefault);
	glTextureStorage2D(nightTexDefault, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(nightTexDefault, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, nightData);

	createFlare();

	// Samplers
	glCreateSamplers(1, &planetTexSampler);
	glSamplerParameterf(planetTexSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(planetTexSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glCreateSamplers(1, &atmoSampler);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glSamplerParameteri(atmoSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glCreateSamplers(1, &ringSampler);
	glSamplerParameteri(ringSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(ringSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(ringSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
}

int mipmapCount(int size)
{
	return 1 +std::floor(std::log2(size));
}

void RendererGL::createFlare()
{
	const int flareSize = 512;
	const int mips = mipmapCount(flareSize);
	{
		vector<uint16_t> pixelData;
		Renderer::generateFlareIntensityTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareIntensityTex);
		glTextureStorage1D(flareIntensityTex, mips, GL_R16F, flareSize);
		glTextureSubImage1D(flareIntensityTex, 0, 0, flareSize, GL_RED, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareIntensityTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareIntensityTex);
	}
	{
		vector<uint8_t> pixelData;
		Renderer::generateFlareLinesTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_2D, 1, &flareLinesTex);
		glTextureStorage2D(flareLinesTex, mips, GL_R8, flareSize, flareSize);
		glTextureSubImage2D(flareLinesTex, 0, 0, 0, flareSize, flareSize, GL_RED, GL_UNSIGNED_BYTE, pixelData.data());
		glGenerateTextureMipmap(flareLinesTex);
	}
	{
		vector<uint16_t> pixelData;
		Renderer::generateFlareHaloTex(flareSize, pixelData);
		glCreateTextures(GL_TEXTURE_1D, 1, &flareHaloTex);
		glTextureStorage1D(flareHaloTex, mips, GL_RGBA16F, flareSize);
		glTextureSubImage1D(flareHaloTex, 0, 0, flareSize, GL_RGBA, GL_HALF_FLOAT, pixelData.data());
		glTextureParameteri(flareHaloTex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glGenerateTextureMipmap(flareHaloTex);
	}
}

void RendererGL::createVertexArray()
{
	// Vertex Array Object creation
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &vertexArray);
	glVertexArrayVertexBuffer(vertexArray, VERTEX_BINDING, vertexBuffer.getId(), 0, sizeof(Vertex));
	glVertexArrayElementBuffer(vertexArray, indexBuffer.getId());

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

	// Highpass rendertargets
	glCreateTextures(GL_TEXTURE_2D, 5, highpassRendertargets);
	for (int i=0;i<5;++i)
	{
		glTextureStorage2D(highpassRendertargets[i], 
			1, GL_R11F_G11F_B10F, windowWidth>>i, windowHeight>>i);
	}

	// Bloom rendertargets
	glCreateTextures(GL_TEXTURE_2D, 4, bloomRendertargets);
	for (int i=0;i<4;++i)
	{
		glTextureStorage2D(bloomRendertargets[i], 
			1, GL_R11F_G11F_B10F, windowWidth>>(i+1), windowHeight>>(i+1));

	}

	// Sampler
	glCreateSamplers(1, &rendertargetSampler);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(rendertargetSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Framebuffers
	glCreateFramebuffers(1, &hdrFbo);
	glNamedFramebufferTexture(hdrFbo, GL_COLOR_ATTACHMENT0, hdrMSRendertarget, 0);
	glNamedFramebufferTexture(hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTex, 0);

	// Enable SRGB output
	glEnable(GL_FRAMEBUFFER_SRGB);
}

void RendererGL::createShaders()
{
	Shader planetVert(GL_VERTEX_SHADER, "shaders/planet.vert");
	Shader planetFrag(GL_FRAGMENT_SHADER, "shaders/planet.frag");
	Shader deferredVert(GL_VERTEX_SHADER, "shaders/deferred.vert");

	programSun.addShader(planetVert);
	programSun.addShader(planetFrag);
	programSun.setConstant("IS_STAR", "");
	programSun.compileAndLink();

	programPlanet.addShader(planetVert);
	programPlanet.addShader(planetFrag);
	programPlanet.compileAndLink();

	programPlanetAtmo.addShader(planetVert);
	programPlanetAtmo.addShader(planetFrag);
	programPlanetAtmo.setConstant("HAS_ATMO", "");
	programPlanetAtmo.compileAndLink();

	programAtmo.addShader(planetVert);
	programAtmo.addShader(GL_FRAGMENT_SHADER, "shaders/atmo.frag");
	programAtmo.setConstant("IS_ATMO", "");
	programAtmo.compileAndLink();

	programRingFar.addShader(planetVert);
	programRingFar.addShader(GL_FRAGMENT_SHADER, "shaders/ring.frag");
	programRingFar.setConstant("IS_FAR_RING", "");
	programRingFar.compileAndLink();

	programRingNear.addShader(planetVert);
	programRingNear.addShader(GL_FRAGMENT_SHADER, "shaders/ring.frag");
	programRingNear.setConstant("IS_NEAR_RING", "");
	programRingNear.compileAndLink();

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

	programFlare.addShader(GL_VERTEX_SHADER, "shaders/flare.vert");
	programFlare.addShader(GL_FRAGMENT_SHADER, "shaders/flare.frag");
	programFlare.compileAndLink();

	programTonemap.addShader(deferredVert);
	programTonemap.addShader(GL_FRAGMENT_SHADER, "shaders/tonemap.frag");
	programTonemap.compileAndLink();
}

void RendererGL::destroy()
{
	// Kill thread
	{
		lock_guard<mutex> lk(texWaitMutex);
		killThread = true;
	}
	texWaitCondition.notify_one();
}

/// Converts a RGBA color to a BC1, BC2 or BC3 block (roughly, without interpolation)
void RGBAToBC(const vec4 color, DDSLoader::Format format, vector<uint8_t> &block)
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

RendererGL::PlanetData::StreamTex RendererGL::loadDDSTexture(
	const string filename,
	int *lodMin,
	const vec4 defaultColor)
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
		// Default color at highest mipmap
		*lodMin = mipmapCount-1;
		vector<uint8_t> block;
		RGBAToBC(defaultColor, loader.getFormat(), block);
		glCompressedTextureSubImage2D(id, *lodMin, 0, 0, 1, 1, 
			DDSFormatToGL(loader.getFormat()), block.size(), block.data());

		// Create sampler
		GLuint sampler;
		glCreateSamplers(1, &sampler);
		glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureAnisotropy);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_LOD, *lodMin);

		// Create jobs
		const int groupLoadMipmap = 8; // Number of highest mipmaps to load together
		int jobCount = std::max(mipmapCount-groupLoadMipmap+1, 1);
		vector<TexWait> texWait(jobCount);
		for (int i=0;i<texWait.size();++i)
		{
			texWait[i] = {id, sampler, lodMin, jobCount-i-1, (i)?1:std::min(groupLoadMipmap,mipmapCount), loader};
		}

		// Push jobs to queue
		{
			lock_guard<mutex> lk(texWaitMutex);
			for (auto tw : texWait)
			{
				texWaitQueue.push(tw);
			}
		}
		// Wake up loading thread
		texWaitCondition.notify_one();
		return PlanetData::StreamTex(id, sampler, *lodMin, *lodMin);
	}
	return PlanetData::StreamTex(0,0,0,0);
}

void RendererGL::unloadDDSTexture(PlanetData::StreamTex tex)
{
	if (tex.id)
		glDeleteTextures(1, &tex.id);
	if (tex.sampler)
		glDeleteSamplers(1, &tex.sampler);
}

void RendererGL::render(
		const dvec3 viewPos, 
		const float fovy,
		const dvec3 viewCenter,
		const vec3 viewUp,
		const float exposure,
		const float ambientColor,
		const vector<PlanetState> planetStates)
{

	const float closePlanetMinSizePixels = 1;
	this->closePlanetMaxDistance = windowHeight/(closePlanetMinSizePixels*tan(fovy/2));
	this->farPlanetMinDistance = closePlanetMaxDistance*0.35;
	this->farPlanetOptimalDistance = closePlanetMaxDistance*2.0;
	this->texLoadDistance = closePlanetMaxDistance*1.4;
	this->texUnloadDistance = closePlanetMaxDistance*1.6;

	profiler.begin("Full frame");

	auto &currentData = dynamicData[frameId];

	// Projection and view matrices
	const mat4 projMat = perspective(fovy, windowWidth/(float)windowHeight, 1.f, (float)5e10);
	const mat4 viewMat = lookAt(vec3(0), (vec3)(viewCenter-viewPos), viewUp);

	// Planet classification
	vector<uint32_t> closePlanets;
	vector<uint32_t> farPlanets;
	vector<uint32_t> atmoPlanets;

	vector<uint32_t> texLoadPlanets;
	vector<uint32_t> texUnloadPlanets;

	for (uint32_t i=0;i<planetStates.size();++i)
	{
		auto &data = planetData[i];
		const float radius = planetParams[i].bodyParam.radius;
		const dvec3 pos = planetStates[i].position;
		const double dist = distance(viewPos, pos)/radius;
		if (dist < texLoadDistance && !data.texLoaded)
		{
			// Textures need to be loaded
			texLoadPlanets.push_back(i);
		}
		else if (dist > texUnloadDistance && data.texLoaded)
		{
			// Textures need to be unloaded
			texUnloadPlanets.push_back(i);
		}

		// Don't render planets behind the view
		if ((viewMat*vec4(pos - viewPos,1.0)).z < 0)
		{
			if (dist < closePlanetMaxDistance)
			{
				// Detailed model
				closePlanets.push_back(i);
				// Planet atmospheres
				if (planetParams[i].atmoParam.hasAtmosphere)
				{
					atmoPlanets.push_back(i);
				}
			}
			if (dist > farPlanetMinDistance || planetParams[i].bodyParam.isStar)
			{
				// Flares
				farPlanets.push_back(i);
			}
		}
	}

	// Manage stream textures
	loadTextures(texLoadPlanets);
	unloadTextures(texUnloadPlanets);
	uploadLoadedTextures();

	const float exp = pow(2, exposure);

	// Scene uniform update
	SceneDynamicUBO sceneUBO;
	sceneUBO.projMat = projMat;
	sceneUBO.viewMat = viewMat;
	sceneUBO.viewPos = vec4(0.0,0.0,0.0,1.0);
	sceneUBO.ambientColor = ambientColor;
	sceneUBO.exposure = exp;

	// Planet uniform update
	// Close planets (detailed model)
	vector<PlanetDynamicUBO> planetUBOs(planetCount);
	for (uint32_t i : closePlanets)
	{
		planetUBOs[i] = getPlanetUBO(viewPos, viewMat, 
			planetStates[i], planetParams[i], planetData[i]);
	}
	// Far away planets (flare)
	vector<FlareDynamicUBO> flareUBOs(planetCount);
	for (uint32_t i : farPlanets)
	{
		flareUBOs[i] = getFlareUBO(viewPos, projMat, viewMat, fovy, exp,
			planetStates[i], planetParams[i]);
	}

	// Dynamic data upload
	profiler.begin("Sync wait");
	currentData.fence.wait();
	profiler.end();

	uboBuffer.update(currentData.sceneUBO, &sceneUBO);
	for (uint32_t i=0;i<planetUBOs.size();++i)
	{
		uboBuffer.update(currentData.planetUBOs[i], &planetUBOs[i]);
	}
	for (uint32_t i=0;i<flareUBOs.size();++i)
	{
		uboBuffer.update(currentData.flareUBOs[i], &flareUBOs[i]);
	}

	uboBuffer.write();

	// Planet sorting from front to back
	sort(closePlanets.begin(), closePlanets.end(), [&](int i, int j)
	{
		const float distI = distance(planetStates[i].position, viewPos);
		const float distJ = distance(planetStates[j].position, viewPos);
		return distI < distJ;
	});

	// Atmosphere sorting from back to front
	sort(atmoPlanets.begin(), atmoPlanets.end(), [&](int i, int j)
	{
		const float distI = distance(planetStates[i].position, viewPos);
		const float distJ = distance(planetStates[j].position, viewPos);
		return distI > distJ;
	});

	// Backface culling
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	profiler.begin("Planets");
	renderHdr(closePlanets, currentData);
	profiler.end();
	profiler.begin("Atmospheres");
	renderAtmo(atmoPlanets, currentData);
	profiler.end();
	profiler.begin("Bloom");
	renderBloom();
	profiler.end();
	profiler.begin("Flares");
	renderFlares(farPlanets, currentData);
	profiler.end();
	profiler.begin("Tonemapping");
	renderTonemap(currentData);
	profiler.end();

	profiler.end();

	currentData.fence.lock();

	frameId = (frameId+1)%bufferFrames;
}

void RendererGL::renderHdr(
	const vector<uint32_t> closePlanets,
	const DynamicData ddata)
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
	vector<GLuint> diffuseTextures(planetCount);
	vector<GLuint> cloudTextures(planetCount);
	vector<GLuint> nightTextures(planetCount);

	// Planet rendering
	for (uint32_t i : closePlanets)
	{
		auto &data = planetData[i];
		const bool star = planetParams[i].bodyParam.isStar;
		const bool hasAtmo = planetParams[i].atmoParam.hasAtmosphere;
		glUseProgram(
			(star?
			programSun:
			(hasAtmo?programPlanetAtmo:programPlanet)).getId());

		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			ddata.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			ddata.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		// Bind textures
		bool hasDiffuse = data.diffuse.id;
		bool hasCloud = data.cloud.id;
		bool hasNight = data.night.id;
		GLuint samplers[] = {
			hasDiffuse?data.diffuse.sampler:planetTexSampler,
			hasCloud?data.cloud.sampler:planetTexSampler,
			hasNight?data.night.sampler:planetTexSampler};
		GLuint texs[] = {
			hasDiffuse?data.diffuse.id:diffuseTexDefault,
			hasCloud?data.cloud.id:cloudTexDefault,
			hasNight?data.night.id:nightTexDefault};
		glBindSamplers(2, 3, samplers);
		glBindTextures(2, 3, texs);

		// Min LOD smoothing
		for (auto tex : {&data.diffuse, &data.cloud, &data.night})
		{
			if (tex->smoothLodMin > tex->lodMin)
				tex->smoothLodMin = std::max((float)tex->lodMin, 
					tex->smoothLodMin - 0.1f*ceil(tex->smoothLodMin - tex->lodMin));
			glSamplerParameterf(tex->sampler, GL_TEXTURE_MIN_LOD, tex->smoothLodMin);
		}

		if (hasAtmo)
		{
			glBindSampler(5, atmoSampler);
			glBindTextureUnit(5, data.atmoLookupTable);
		}

		data.planetModel.draw();
	}
}

void RendererGL::renderAtmo(
	const vector<uint32_t> atmoPlanets,
	const DynamicData data)
{
	// Only depth test
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// Blending
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_SRC_ALPHA);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

	for (uint32_t i : atmoPlanets)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			data.planetUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		bool hasRings = planetParams[i].ringParam.hasRings;

		auto &data = planetData[i];
		DrawCommand ringModel = data.ringModel;

		// Far rings
		if (hasRings)
		{
			glUseProgram(programRingFar.getId());
			glBindSampler(2, ringSampler);
			glBindTextureUnit(2, data.ringTex1);
			glBindSampler(3, ringSampler);
			glBindTextureUnit(3, data.ringTex2);
			ringModel.draw();
		}

		// Atmosphere
		glUseProgram(programAtmo.getId());
		
		glBindSampler(2, atmoSampler);
		glBindTextureUnit(2, data.atmoLookupTable);
		data.planetModel.draw();

		// Near rings
		if (hasRings)
		{
			glUseProgram(programRingNear.getId());
			glBindSampler(2, ringSampler);
			glBindTextureUnit(2, data.ringTex1);
			glBindSampler(3, ringSampler);
			glBindTextureUnit(3, data.ringTex2);
			ringModel.draw();
		}
	}
}

void RendererGL::renderBloom()
{
	const int workgroupSize = 16;
	// Highpass
	glUseProgram(programHighpass.getId());
	glBindImageTexture(0, hdrMSRendertarget       , 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
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
			glBindSampler(0, rendertargetSampler);
			glBindTextureUnit(0, bloomImages[i*3+0]);
			glBindImageTexture(1, bloomImages[i*3+2], 0, GL_FALSE, 0, GL_READ_ONLY , GL_R11F_G11F_B10F);
			glBindImageTexture(2, bloomImages[i*3+3], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
			glDispatchCompute(
				(int)ceil(2*windowWidth /(float)dispatchSize),
				(int)ceil(2*windowHeight/(float)dispatchSize), 1);
		}
	}
}

void RendererGL::renderFlares(
	const vector<uint32_t> farPlanets, 
	const DynamicData data)
{
	// Only depth test
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);
	// No stencil writes
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Blending add
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

	glUseProgram(programFlare.getId());

	for (uint32_t i : farPlanets)
	{
		// Bind Scene UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

		// Bind planet UBO
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboBuffer.getId(),
			data.flareUBOs[i].getOffset(),
			sizeof(PlanetDynamicUBO));

		// Bind textures
		GLuint samplers[] = {0,0,0};
		glBindSamplers(2, 3, samplers);
		glBindTextureUnit(2, flareIntensityTex);
		glBindTextureUnit(3, flareLinesTex);
		glBindTextureUnit(4, flareHaloTex);

		flareModel.draw();
	}
}

void RendererGL::renderTonemap(const DynamicData data)
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
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuffer.getId(),
			data.sceneUBO.getOffset(),
			sizeof(SceneDynamicUBO));

	// Bind image after bloom is done
	glBindSampler(1, rendertargetSampler);
	glBindTextureUnit(1, hdrMSRendertarget);
	glBindSampler(2, rendertargetSampler);
	glBindTextureUnit(2, bloomRendertargets[0]);

	fullscreenTri.draw();
}

void RendererGL::loadTextures(const vector<uint32_t> texLoadPlanets)
{
	// Texture loading
	for (uint32_t i : texLoadPlanets)
	{
		const PlanetParameters param = planetParams[i];
		auto &data = planetData[i];
		// Textures & samplers
		data.diffuse = loadDDSTexture(param.assetPaths.diffuseFilename, &data.diffuse.lodMin, vec4(1,1,1,1));
		data.cloud = loadDDSTexture(param.assetPaths.cloudFilename, &data.cloud.lodMin  , vec4(0,0,0,0));
		data.night = loadDDSTexture(param.assetPaths.nightFilename, &data.night.lodMin  , vec4(0,0,0,0));

		// Generate atmospheric scattering lookup texture
		if (param.atmoParam.hasAtmosphere)
		{
			const int size = 128;
			vector<float> table;
			planetParams[i].atmoParam.generateLookupTable(table, size, param.bodyParam.radius);

			GLuint &tex = data.atmoLookupTable;

			glCreateTextures(GL_TEXTURE_2D, 1, &tex);
			glTextureStorage2D(tex, mipmapCount(size), GL_RG32F, size, size);
			glTextureSubImage2D(tex, 0, 0, 0, size, size, GL_RG, GL_FLOAT, table.data());
			glGenerateTextureMipmap(tex);
		}

		// Load ring textures
		if (param.ringParam.hasRings)
		{
			// Load files
			vector<float> backscat, forwardscat, unlit, transparency, color;
			const RingParameters ringParam = param.ringParam;
			const AssetPaths assetPaths = param.assetPaths;
			ringParam.loadFile(assetPaths.backscatFilename, backscat);
			ringParam.loadFile(assetPaths.forwardscatFilename, forwardscat);
			ringParam.loadFile(assetPaths.unlitFilename, unlit);
			ringParam.loadFile(assetPaths.transparencyFilename, transparency);
			ringParam.loadFile(assetPaths.colorFilename, color);

			size_t size = backscat.size();

			// Check sizes
			if (size != forwardscat.size() &&
				size != unlit.size() &&
				size != transparency.size() &&
				size*3 != color.size())
			{
				throw runtime_error("Ring texture sizes don't match");
			}

			// Assemble values into two textures (t1 for back, forward and unlit, t2 for color+transparency)
			vector<float> t1(size*3);
			vector<float> t2(size*4);
			for (int i=0;i<size;++i)
			{
				t1[i*3+0] = backscat[i];
				t1[i*3+1] = forwardscat[i];
				t1[i*3+2] = unlit[i];
				t2[i*4+0] = color[i*3+0];
				t2[i*4+1] = color[i*3+1];
				t2[i*4+2] = color[i*3+2];
				t2[i*4+3] = transparency[i];
			}

			GLuint &tex1 = data.ringTex1;
			GLuint &tex2 = data.ringTex2;

			glCreateTextures(GL_TEXTURE_1D, 1, &tex1);
			glTextureStorage1D(tex1, mipmapCount(size), GL_RGB32F, size);
			glTextureSubImage1D(tex1, 0, 0, size, GL_RGB, GL_FLOAT, t1.data());
			glGenerateTextureMipmap(tex1);

			glCreateTextures(GL_TEXTURE_1D, 1, &tex2);
			glTextureStorage1D(tex2, mipmapCount(size), GL_RGBA32F, size);
			glTextureSubImage1D(tex2, 0, 0, size, GL_RGBA, GL_FLOAT, t2.data());
			glGenerateTextureMipmap(tex2);
		}

		data.texLoaded = true;
	}
}

void RendererGL::unloadTextures(vector<uint32_t> texUnloadPlanets)
{
	// Texture unloading
	for (uint32_t i : texUnloadPlanets)
	{
		auto &data = planetData[i];
		const auto param = planetParams[i];
		unloadDDSTexture(data.diffuse);
		unloadDDSTexture(data.cloud);
		unloadDDSTexture(data.night);

		data.diffuse = PlanetData::StreamTex();
		data.cloud = PlanetData::StreamTex();
		data.night = PlanetData::StreamTex();

		if (param.atmoParam.hasAtmosphere)
		{
			glDeleteTextures(1, &data.atmoLookupTable);
		}

		if (param.ringParam.hasRings)
		{
			glDeleteTextures(1, &data.ringTex1);
			glDeleteTextures(2, &data.ringTex2);
		}

		data.texLoaded = false;
	}
}

void RendererGL::uploadLoadedTextures()
{
	// Texture uploading
	lock_guard<mutex> lk(texLoadedMutex);
	while (!texLoadedQueue.empty())
	{
		TexLoaded texLoaded = texLoadedQueue.front();
		texLoadedQueue.pop();
		glCompressedTextureSubImage2D(
			texLoaded.id,
			texLoaded.mipmap,
			0, 0, 
			texLoaded.width, texLoaded.height, 
			texLoaded.format,
			texLoaded.imageSize, texLoaded.data->data()+texLoaded.mipmapOffset);
		*texLoaded.lodMin = std::min(*texLoaded.lodMin, texLoaded.mipmap);
	}
}

RendererGL::PlanetDynamicUBO RendererGL::getPlanetUBO(
	const dvec3 viewPos, const mat4 viewMat,
	const PlanetState state, const PlanetParameters params,
	const PlanetData data)
{
	const vec3 planetPos = state.position - viewPos;

	// Planet rotation
	const vec3 north = vec3(0,0,1);
	const vec3 rotAxis = params.bodyParam.rotationAxis;
	const quat q = rotate(quat(), 
		(float)acos(dot(north, rotAxis)), 
		cross(north, rotAxis))*
		rotate(quat(), state.rotationAngle, north);

	// Model matrix
	const mat4 modelMat = 
		translate(mat4(), planetPos)*
		mat4_cast(q)*
		scale(mat4(), vec3(params.bodyParam.radius));

	// Atmosphere matrix
	const mat4 atmoMat = 
		translate(mat4(), planetPos)*
		mat4_cast(q)*
		scale(mat4(), -vec3(params.bodyParam.radius+params.atmoParam.maxHeight));

	// Ring matrices
	const vec3 towards = normalize(planetPos);
	const vec3 up = params.ringParam.normal;
	const float sideflip = (dot(towards, up)<0)?1.f:-1.f;
	const vec3 right = normalize(cross(towards, up));
	const vec3 newTowards = cross(right, up);

	const mat4 lookAtFar = mat4(mat3(sideflip*right, -newTowards, up));
	const mat4 lookAtNear = mat4(mat3(-sideflip*right, newTowards, up));

	const mat4 ringFarMat = 
		translate(mat4(), planetPos)*
		lookAtFar;

	const mat4 ringNearMat =
		translate(mat4(), planetPos)*
		lookAtNear;

	// Light direction
	const vec3 lightDir = vec3(normalize(-state.position));

	PlanetDynamicUBO ubo;
	ubo.modelMat = modelMat;
	ubo.atmoMat = atmoMat;
	ubo.ringFarMat = ringFarMat;
	ubo.ringNearMat = ringNearMat;
	ubo.planetPos = viewMat*vec4(planetPos, 1.0);
	ubo.lightDir = viewMat*vec4(lightDir,0.0);
	ubo.K = params.atmoParam.K;
	ubo.cloudDisp = state.cloudDisp;
	ubo.nightTexIntensity = params.bodyParam.nightTexIntensity;
	ubo.albedo = params.bodyParam.albedo;
	ubo.radius = params.bodyParam.radius;
	ubo.atmoHeight = params.atmoParam.maxHeight;

	return ubo;
}

RendererGL::FlareDynamicUBO RendererGL::getFlareUBO(
	const dvec3 viewPos, const mat4 projMat,
	const mat4 viewMat, const float fovy, const float exp, 
	const PlanetState state, const PlanetParameters params)
{
	const vec3 planetPos = vec3(state.position - viewPos);
	const vec4 clip = projMat*viewMat*vec4(planetPos,1.0);
	const vec2 screen = vec2(clip)/clip.w;
	const float FLARE_SIZE_DEGREES = 20.0;
	const mat4 modelMat = 
		translate(mat4(), vec3(screen, 0.999))*
		scale(mat4(), 
			vec3(windowHeight/(float)windowWidth,1.0,0.0)*
			FLARE_SIZE_DEGREES*(float)glm::pi<float>()/(fovy*180.0f));

	const float phaseAngle = acos(dot(
		(vec3)normalize(state.position), 
		normalize(planetPos)));
	const float phase = 
		(1-phaseAngle/glm::pi<float>())*cos(phaseAngle)+
		(1/glm::pi<float>())*sin(phaseAngle);
	const bool isStar = params.bodyParam.isStar;
	const float radius = params.bodyParam.radius;
	const double dist = distance(viewPos, state.position)/radius;
	const float fade = clamp(isStar?
		(float)   dist/10:
		(float) ((dist-farPlanetMinDistance)/
						(farPlanetOptimalDistance-farPlanetMinDistance)),0.f,1.f);
	const vec3 cutDist = planetPos*0.005f;
	
	const float brightness = std::min(4.0f,
		exp*
		radius*radius*
		(isStar?100.f:
		params.bodyParam.albedo*0.2f*phase)
		/dot(cutDist,cutDist))
		*fade;

	FlareDynamicUBO ubo;
	ubo.modelMat = modelMat;
	ubo.color = vec4(params.bodyParam.meanColor,1.0);
	ubo.brightness = brightness;

	return ubo;
}

void RendererGL::initThread() {
	texLoadThread = thread([&,this]()
	{
		while (true)
		{
			// Wait for kill or queue not empty
			{
				unique_lock<mutex> lk(texWaitMutex);
				texWaitCondition.wait(lk, [&]{ return killThread || !texWaitQueue.empty();});
			}

			if (killThread) return;

			// Get info about texture were are going to load
			TexWait texWait;
			{
				lock_guard<mutex> lk(texWaitMutex);
				texWait = texWaitQueue.front();
				texWaitQueue.pop();
			}

			shared_ptr<vector<uint8_t>> imageData = make_shared<vector<uint8_t>>();

			// Load image data
			size_t imageSize;
			texWait.loader.getImageData(texWait.mipmap, texWait.mipmapCount, &imageSize, nullptr);
			imageData->resize(imageSize);
			texWait.loader.getImageData(texWait.mipmap, texWait.mipmapCount, &imageSize, imageData->data());

			vector<TexLoaded> texLoaded(texWait.mipmapCount);
			size_t offset = 0;

			for (unsigned int i=0;i<texLoaded.size();++i)
			{
				auto &tl = texLoaded[i];
				tl.id = texWait.id;
				tl.sampler = texWait.sampler;
				tl.lodMin = texWait.lodMin;
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

			// Emulate slow loading times with this
			//this_thread::sleep_for(chrono::milliseconds(1000));
			
			// Push loaded texture into queue
			{
				lock_guard<mutex> lk(texLoadedMutex);
				for (auto tl : texLoaded)
					texLoadedQueue.push(tl);
			}
		}
	});
}

void GPUProfilerGL::begin(const string name)
{
	int id = (bufferId+1)%2;
	auto &val = queries[id][name].first;
	if (val == 0)
	{
		glGenQueries(1, &val);
	}
	glQueryCounter(val, GL_TIMESTAMP);
	names.push(name);
	orderedNames[id].push_back(name);
}

void GPUProfilerGL::end()
{
	string name = names.top();
	names.pop();
	int id = (bufferId+1)%2;
	auto &val = queries[id][name].second;
	if (val == 0)
	{
		glCreateQueries(GL_TIMESTAMP, 1, &val);
	}
	glQueryCounter(val, GL_TIMESTAMP);
	lastQuery = val;
}

vector<pair<string,uint64_t>> GPUProfilerGL::get()
{
	vector<pair<string,uint64_t>> result;
	auto &m = queries[bufferId];
	for (const string name : orderedNames[bufferId])
	{
		auto val = m.find(name);
		GLuint q1 = val->second.first;
		GLuint q2 = val->second.second;
		if (q1 && q2)
		{
			uint64_t start, end;
			glGetQueryObjectui64v(q1, GL_QUERY_RESULT, &start);
			glGetQueryObjectui64v(q2, GL_QUERY_RESULT, &end);
			result.push_back(make_pair(name, end-start));
		}
		if (q1) glDeleteQueries(1, &q1);
		if (q2) glDeleteQueries(1, &q2);
	}

	m.clear();
	orderedNames[bufferId].clear();
	bufferId = (bufferId+1)%2;
	return result;
}

vector<pair<string,uint64_t>> RendererGL::getProfilerTimes()
{
	return profiler.get();
}