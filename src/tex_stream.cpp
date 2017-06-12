#include "tex_stream.hpp"

#include "gl_util.hpp"

#include <iostream>

using namespace std;

GLenum DDSFormatToGL(DDSLoader::Format format)
{
	switch (format)
	{
		case DDSLoader::Format::BC1:       return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		case DDSLoader::Format::BC1_SRGB:  return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
		case DDSLoader::Format::BC2:       return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		case DDSLoader::Format::BC2_SRGB:  return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
		case DDSLoader::Format::BC3:       return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		case DDSLoader::Format::BC3_SRGB:  return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
		case DDSLoader::Format::BC4:       return GL_COMPRESSED_RED_RGTC1;
		case DDSLoader::Format::BC4_SIGNED:return GL_COMPRESSED_SIGNED_RED_RGTC1;
		case DDSLoader::Format::BC5:       return GL_COMPRESSED_RG_RGTC2;
		case DDSLoader::Format::BC5_SIGNED:return GL_COMPRESSED_SIGNED_RG_RGTC2;
		case DDSLoader::Format::BC6:       return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		case DDSLoader::Format::BC6_SIGNED:return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
		case DDSLoader::Format::BC7:       return GL_COMPRESSED_RGBA_BPTC_UNORM;
		case DDSLoader::Format::BC7_SRGB:  return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

	}
	return 0;
}

StreamTex::StreamTex(
	const string &filename,
	DDSStreamer &streamer,
	int maxTexSize)
{
	_loaded = false;
	_id = 0;
	
	DDSLoader loader(maxTexSize);
	if (!loader.open(filename)) return;

	if (loader.getMipmapCount() != 
		mipmapCount(std::max(loader.getWidth(0), loader.getHeight(0))))
	{
		cout << "Warning: Can't load stream texture " << filename
		<< ": not enough mipmaps" << endl;
		return;
	}

	const int mipmapCount = loader.getMipmapCount();
	// Create texture
	glCreateTextures(GL_TEXTURE_2D, 1, &_id);
	glTextureStorage2D(_id, 
		mipmapCount, DDSFormatToGL(loader.getFormat()), 
		loader.getWidth(0), loader.getHeight(0));

	_handle = streamer.submit(loader);
}

void StreamTex::destroy()
{
	if (_id) glDeleteTextures(1, &_id);
	_id = 0;
	_loaded = false;
}

void StreamTex::update(DDSStreamer &streamer)
{
	if (_id && !_loaded)
	{
		const auto &res = streamer.get(_handle);
		if (res.first)
		{
			const auto &data = res.second;
			for (int i=0;i<data.getMipmapCount();++i)
			{
				const auto imageData = std::move(data.get(i));
				glCompressedTextureSubImage2D(
				_id,
				i,
				0, 0, 
				data.getLoader().getWidth(i),
				data.getLoader().getHeight(i),
				DDSFormatToGL(data.getLoader().getFormat()),
				imageData.size(), imageData.data());
			}
			_loaded = true;
		}
	}
}

GLuint StreamTex::getId(GLuint defaultId) const
{
	if (_loaded) return _id;
	return defaultId;
}