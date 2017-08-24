#pragma once

#include "gui.hpp"

#include "graphics_api.hpp"
#include "gl_util.hpp"
#include "fence.hpp"
#include "shader_pipeline.hpp"

#include <vector>

class GuiGL : public Gui
{
protected:
	void initGraphics(
		int atlasWidth, int atlasHeight, 
		const std::vector<uint8_t> &atlasData);
	void displayGraphics(const RenderInfo &info);

private:
	GLuint _atlas;
	GLuint _vao;
	Buffer _vertexBuffer;
	ShaderPipeline _pipeline;
	int _frameId = 0;
	int _bufferFrames = 3;
	std::vector<BufferRange> _ranges;
	std::vector<Fence> _fences;
};