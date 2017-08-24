#include "gui_gl.hpp"

using namespace std;

const size_t maxVertices = 10000;

void GuiGL::initGraphics(
	int atlasWidth, int atlasHeight, 
	const vector<uint8_t> &atlasData)
{
	// Atlas
	glCreateTextures(GL_TEXTURE_2D, 1, &_atlas);
	glTextureStorage2D(_atlas, 1, GL_RGBA8, atlasWidth, atlasHeight);
	glTextureSubImage2D(_atlas, 0, 0, 0, atlasWidth, atlasHeight, 
		GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());

	// VAO
	const int VERTEX_BINDING = 0;
	glCreateVertexArrays(1, &_vao);

	const int VERTEX_ATTRIB_POS   = 0;
	const int VERTEX_ATTRIB_UV    = 1;
	const int VERTEX_ATTRIB_COLOR = 2;

	// Position
	glEnableVertexArrayAttrib(_vao, VERTEX_ATTRIB_POS);
	glVertexArrayAttribBinding(_vao, VERTEX_ATTRIB_POS, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vao, VERTEX_ATTRIB_POS, 2, 
		GL_FLOAT, false, offsetof(Vertex, x));

	// UVs
	glEnableVertexArrayAttrib(_vao, VERTEX_ATTRIB_UV);
	glVertexArrayAttribBinding(_vao, VERTEX_ATTRIB_UV, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vao, VERTEX_ATTRIB_UV, 2, 
		GL_FLOAT, false, offsetof(Vertex, u));

	// Normals
	glEnableVertexArrayAttrib(_vao, VERTEX_ATTRIB_COLOR);
	glVertexArrayAttribBinding(_vao, VERTEX_ATTRIB_COLOR, VERTEX_BINDING);
	glVertexArrayAttribFormat(_vao, VERTEX_ATTRIB_COLOR, 4, 
		GL_UNSIGNED_BYTE, true, offsetof(Vertex, r));

	// Vertex Buffer
	_vertexBuffer = Buffer(
		Buffer::Usage::DYNAMIC,
		Buffer::Access::WRITE_ONLY);

	_ranges.resize(_bufferFrames);
	for (auto &range : _ranges)
		range = _vertexBuffer.assignVertices(maxVertices, sizeof(Vertex));
	_vertexBuffer.validate();

	// Fences
	_fences.resize(_bufferFrames);

	// Shader pipeline
	ShaderFactory factory;
	factory.setVersion(450);
	factory.setFolder("shaders/");

	_pipeline = factory.createPipeline({
		{GL_VERTEX_SHADER, "gui.vert"},
		{GL_FRAGMENT_SHADER, "gui.frag"}});
}

void GuiGL::displayGraphics(const RenderInfo &info)
{
	// Blending add
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	_fences[_frameId].waitClient();

	const size_t count = min(info.vertices.size(), maxVertices);
	const BufferRange frameRange = _ranges[_frameId];
	const BufferRange drawRange = {
		frameRange.getOffset(), 
		(uint32_t)(count*sizeof(Vertex))};
	_vertexBuffer.write(drawRange, info.vertices.data());

	_pipeline.bind();
	glBindTextureUnit(0, _atlas);
	DrawCommand(_vao, GL_TRIANGLES, count, 
		{{0, _vertexBuffer.getId(), drawRange, sizeof(Vertex)}}).draw();
	_fences[_frameId].lock();

	_frameId = (_frameId+1)%_bufferFrames;
}