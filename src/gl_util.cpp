#include "gl_util.hpp"

#include <utility>
#include <stdexcept>
#include <cstring>

using namespace std;

uint32_t Buffer::_alignUBO = 256;
uint32_t Buffer::_alignSSBO = 32;
bool Buffer::_limitsDefined = false;

Buffer::Buffer()
{
	_id = 0;
	_size = 0;
	_built = false;
	_lastOffset = 0;
}

Buffer::Buffer(bool dynamic, bool write, bool read) : Buffer()
{
	_dynamic = dynamic;
	_write = write;
	_read = read;

	glCreateBuffers(1, &_id);
}

void Buffer::getLimits()
{
	if (!_limitsDefined)
	{
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (int*)&_alignUBO);
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, (int*)&_alignSSBO);
		_limitsDefined = true;
	}
}

uint32_t Buffer::align(const uint32_t offset, const uint32_t align)
{
	const uint32_t remainder = offset%align;

	if (remainder) return offset + align - remainder;
	else return offset;
}

void Buffer::update(const BufferSection section, const void *data)
{
	if (data)
	{
		size_t size = section.getSize();
		uint8_t *copy = (uint8_t*)malloc(size);
		memcpy(copy, data, size);
		_dataToUpload.insert(make_pair(section.getOffset(), 
			make_pair(section.getSize(), shared_ptr<uint8_t>(copy))));
	}
}

BufferSection Buffer::assign(
	const uint32_t size, 
	const uint32_t stride,
	const void *data)
{
	if (_built) 
		throw runtime_error("Can't assign memory after structure is set");

	_lastOffset = align(_lastOffset, stride);
	const BufferSection section = {_lastOffset, size};
	_lastOffset += size;

	update(section, data);
	return section;
}

BufferSection Buffer::assignVertices(uint32_t count, uint32_t stride, 
	const void *data)
{
	return assign(count*stride, stride, data);
}

BufferSection Buffer::assignIndices(uint32_t count, uint32_t stride, 
	const void *data)
{
	return assign(count*stride, stride, data);
}

BufferSection Buffer::assignUBO(const uint32_t size, const void *data)
{
	getLimits();
	return assign(size, _alignUBO, data);
}

BufferSection Buffer::assignSSBO(const uint32_t size, const void *data)
{
	getLimits();
	return assign(size, _alignSSBO, data);
}

void Buffer::storageStatic()
{
	glNamedBufferStorage(_id, _size, nullptr, GL_DYNAMIC_STORAGE_BIT);
}

void Buffer::storageDynamic()
{
	const GLbitfield storageFlags = 
	GL_MAP_PERSISTENT_BIT |
#ifdef USE_COHERENT_MAPPING
	GL_MAP_COHERENT_BIT |
#endif
	(_write?GL_MAP_WRITE_BIT:0) | 
	(_read ?GL_MAP_READ_BIT :0) ;

	const GLbitfield mapFlags = storageFlags
#ifndef USE_COHERENT_MAPPING
	| GL_MAP_FLUSH_EXPLICIT_BIT
#endif
	;

	glNamedBufferStorage(_id, _size, nullptr, storageFlags);
	_mapPtr = glMapNamedBufferRange(_id, 0, _size, mapFlags);

	if (!_mapPtr) throw runtime_error("Can't map dynamic buffer");
}

void Buffer::lockAssigning()
{
	if (!_built)
	{
		_size = _lastOffset;
		// Create storage
		if (_dynamic) storageDynamic();
		else storageStatic();
	}
	_built = true;
}

void Buffer::write()
{
	if (!_write) throw runtime_error("Can't write to a non write buffer");

	lockAssigning();

	for (auto m : _dataToUpload)
	{
		// Dynamic data : map memcpy
		if (_dynamic)
		{
			memcpy((uint8_t*)_mapPtr+m.first, m.second.second.get(), m.second.first);
#ifndef USE_COHERENT_MAPPING
			glFlushMappedNamedBufferRange(_id, m.first, m.second.first);
#endif
		}
		// Static data : BufferSubData
		else
			glNamedBufferSubData(_id, 
				m.first, m.second.first, m.second.second.get());
	}

	_dataToUpload.clear();
}

void Buffer::read(const BufferSection section, void *data)
{
	if (!_read) throw runtime_error("Can't read from a non read buffer");

	if (_dynamic)
	{
		memcpy(data, (uint8_t*)_mapPtr+section.getOffset(), section.getSize());
	}
	else
	{
		glGetNamedBufferSubData(_id, section.getOffset(), section.getSize(), data);
	}
}

GLuint Buffer::getId() const
{
	return _id;
}

DrawCommand::DrawCommand()
{

}

DrawCommand::DrawCommand(
	GLuint vao,
	GLenum mode,
	uint32_t count,
	GLenum type,
	uint32_t indexOffset, 
	uint32_t baseVertex)
{
	_vao = vao;
	_mode = mode;
	_count = count;
	_type = type;
	_indices = (void*)(intptr_t)indexOffset;
	_baseVertex = baseVertex;
}

DrawCommand::DrawCommand(GLuint vao, GLenum mode, GLenum type,
	size_t vertexSize, size_t indexSize,
	BufferSection vertices, BufferSection indices)
{
	_vao = vao;
	_mode = mode;
	_type = type;

	_count = indices.getSize()/indexSize;
	_indices = (void*)(intptr_t)indices.getOffset();
	_baseVertex = vertices.getOffset()/vertexSize;

}

void DrawCommand::draw() const
{
	glBindVertexArray(_vao);
	glDrawElementsBaseVertex(_mode, _count, _type, _indices, _baseVertex);
}

BufferSection::BufferSection()
{
	_offset = 0;
	_size = 0;
}

BufferSection::BufferSection(uint32_t offset, uint32_t size)
{
	_offset = offset;
	_size = size;
}

uint32_t BufferSection::getOffset() const
{
	return _offset;
}

uint32_t BufferSection::getSize() const
{
	return _size;
}