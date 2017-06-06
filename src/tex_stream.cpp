#include "tex_stream.hpp"

#include <iostream>
#include <cmath>

using namespace std;

int mipmapCount(int size)
{
	return 1 +std::floor(std::log2(size));
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