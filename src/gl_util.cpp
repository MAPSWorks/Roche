#include "gl_util.hpp"

#include <utility>
#include <stdexcept>
#include <cstring>
#include <cmath>

using namespace std;

int mipmapCount(int size)
{
	return 1 +floor(log2(size));
}

int mipmapSize(int size, int level)
{
	return size>>level;
}

uint32_t Buffer::_alignUBO = 256;
uint32_t Buffer::_alignSSBO = 32;
bool Buffer::_limitsDefined = false;

Buffer::Buffer(const Usage usage, const Access access, uint32_t size) : 
	Buffer()
{
	_usage = usage;
	_access = access;

	glCreateBuffers(1, &_id);

	if (size != 0)
	{
		_lastOffset = size;
		validate();
	}
}

Buffer::Buffer(Buffer &&b) :
	_id{b._id},
	_size{b._size},
	_validated{b._validated},
	_lastOffset{b._lastOffset},
	_mapPtr{b._mapPtr},
	_usage{b._usage},
	_access{b._access}
{
	b._id = 0;
}

Buffer &Buffer::operator=(Buffer &&b)
{
	if (_id) glDeleteBuffers(1, &_id);
	_id = b._id;
	_size = b._size;
	_validated = b._validated;
	_lastOffset = b._lastOffset;
	_mapPtr = b._mapPtr;
	_usage = b._usage;
	_access = b._access;
	b._id = 0;
	return *this;
}

Buffer::~Buffer()
{
	if (_id) glDeleteBuffers(1, &_id);
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

uint32_t align(const uint32_t offset, const uint32_t align)
{
	const uint32_t remainder = offset%align;

	if (remainder) return offset + align - remainder;
	else return offset;
}

BufferRange Buffer::assign(
	const uint32_t size, 
	const uint32_t stride)
{
	if (_validated) 
		throw runtime_error("Can't assign memory after structure is set");

	_lastOffset = align(_lastOffset, stride);
	const BufferRange range = {_lastOffset, size};
	_lastOffset += size;

	return range;
}

BufferRange Buffer::assignVertices(uint32_t count, uint32_t stride)
{
	return assign(count*stride, stride);
}

BufferRange Buffer::assignIndices(uint32_t count, uint32_t stride)
{
	return assign(count*stride, stride);
}

BufferRange Buffer::assignUBO(const uint32_t size)
{
	getLimits();
	return assign(size, _alignUBO);
}

BufferRange Buffer::assignSSBO(const uint32_t size)
{
	getLimits();
	return assign(size, _alignSSBO);
}

void Buffer::storageStatic()
{
	glNamedBufferStorage(_id, _size, nullptr, GL_DYNAMIC_STORAGE_BIT);
}

GLbitfield getAccessBits(const Buffer::Access access)
{
	switch (access)
	{
		case Buffer::Access::NO_ACCESS: 
			return 0;
		case Buffer::Access::WRITE_ONLY: 
			return GL_MAP_WRITE_BIT;
		case Buffer::Access::READ_ONLY: 
			return GL_MAP_READ_BIT;
		case Buffer::Access::READ_WRITE: 
			return GL_MAP_WRITE_BIT|GL_MAP_READ_BIT;
		default : return 0;
	}
}

void Buffer::storageDynamic()
{
	const GLbitfield storageFlags = 
	GL_MAP_PERSISTENT_BIT |
#ifdef USE_COHERENT_MAPPING
	GL_MAP_COHERENT_BIT |
#endif
	getAccessBits(_access);

	glNamedBufferStorage(_id, _size, nullptr, storageFlags);
	if (_access != Access::NO_ACCESS)
	{
		const GLbitfield mapFlags = storageFlags
#ifndef USE_COHERENT_MAPPING
		| ((_access==Access::WRITE_ONLY
		|| _access==Access::READ_WRITE)?GL_MAP_FLUSH_EXPLICIT_BIT:0)
#endif
		;
		_mapPtr = glMapNamedBufferRange(_id, 0, _size, mapFlags);

		if (!_mapPtr) throw runtime_error("Can't map dynamic buffer");
	}
}

void Buffer::validate()
{
	if (!_validated)
	{
		_size = _lastOffset;
		// Create storage
		if (_usage == Usage::DYNAMIC) storageDynamic();
		else storageStatic();
	}
	_validated = true;
}

void Buffer::write(const BufferRange range, const void *data)
{
	if (_access == Access::NO_ACCESS || 
		_access == Access::READ_ONLY)
		throw runtime_error("Can't write to a buffer that does not support writes");

	if (!_validated)
		throw runtime_error("Can't write to a non-validated buffer");

	// Dynamic data : memcpy to persistent mapped buffer
	if (_usage == Usage::DYNAMIC)
	{
		memcpy((uint8_t*)_mapPtr+range.getOffset(), data, range.getSize());
#ifndef USE_COHERENT_MAPPING
		glFlushMappedNamedBufferRange(_id, range.getOffset(), range.getSize());
#endif
	}
	// Static data : BufferSubData
	else
	{
		glNamedBufferSubData(_id, range.getOffset(), range.getSize(), data);
	}
}

void Buffer::read(const BufferRange range, void *data)
{
	if (_access == Access::NO_ACCESS ||
		_access == Access::WRITE_ONLY)
		throw runtime_error("Can't read from a buffer that does not support reads");

	if (!_validated) 
		throw runtime_error("Can't read from a non-validated buffer");

	if (_usage == Usage::DYNAMIC)
	{
		memcpy(data, (uint8_t*)_mapPtr+range.getOffset(), range.getSize());
	}
	else
	{
		glGetNamedBufferSubData(_id, range.getOffset(), range.getSize(), data);
	}
}

const GLuint &Buffer::getId() const
{
	return _id;
}

void *Buffer::getPtr() const
{
	if (_usage == Usage::STATIC) 
		throw runtime_error("Can't get pointer on static buffer");
	if (!_validated) 
		throw runtime_error("Can't get pointer before validation");
	if (_access == Access::NO_ACCESS) 
		throw runtime_error("Can't get pointer of buffer that does not support writes or reads");
	if (!_mapPtr)
		throw runtime_error("Can't get pointer : pointer is null");
	return _mapPtr;
}

DrawCommand::DrawCommand(
	GLuint vao,
	GLenum mode,
	uint32_t count,
	GLenum type,
	uint32_t indexOffset, 
	uint32_t baseVertex) :
	
	_vao(vao),
	_mode(mode),
	_count(count),
	_type(type),
	_indices((void*)(intptr_t)indexOffset),
	_baseVertex(baseVertex)
{

}

DrawCommand::DrawCommand(GLuint vao, GLenum mode, GLenum type,
	size_t vertexSize, size_t indexSize,
	BufferRange vertices, BufferRange indices) :
	
	_vao(vao),
	_mode(mode),
	_type(type),
	_count(indices.getSize()/indexSize),
	_indices((void*)(intptr_t)indices.getOffset()),
	_baseVertex(vertices.getOffset()/vertexSize)
{

}

void DrawCommand::draw() const
{
	glBindVertexArray(_vao);
	glDrawElementsBaseVertex(_mode, _count, _type, _indices, _baseVertex);
}

BufferRange::BufferRange(uint32_t offset, uint32_t size) :
	_offset(offset),
	_size(size)
{
}

uint32_t BufferRange::getOffset() const
{
	return _offset;
}

uint32_t BufferRange::getSize() const
{
	return _size;
}