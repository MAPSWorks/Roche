#pragma once

#include "graphics_api.hpp"
#include "dds_stream.hpp"

GLenum DDSFormatToGL(DDSLoader::Format format);

class StreamTex
{
public:
	StreamTex() = default;
	StreamTex(
		const std::string &filename, 
		DDSStreamer &streamer,
		int maxTexSize);
	void destroy();
	void update(DDSStreamer &streamer);
	GLuint getId(GLuint defaultId) const;
private:
	bool _loaded = false;
	GLuint _id = 0;
	DDSStreamer::Handle _handle;
};