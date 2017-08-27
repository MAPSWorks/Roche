#include "gl_util.hpp"

#include <utility>
#include <stdexcept>
#include <cstring>
#include <cmath>

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
		default: return 0;
	}
}

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
	const uint32_t stride,
	const void* data)
{
	if (_validated) 
		throw runtime_error("Can't assign memory after structure is set");

	_lastOffset = align(_lastOffset, stride);
	const BufferRange range = {_lastOffset, size};
	_lastOffset += size;

	if (data)
	{
		uint8_t *dataCpy = new uint8_t[range.getSize()];
		memcpy(dataCpy, data, range.getSize());
		_toWrite.push_back(make_pair(range, unique_ptr<uint8_t>(dataCpy))); 
	}

	return range;
}

BufferRange Buffer::assignVertices(uint32_t count, uint32_t stride,
	const void* data)
{
	return assign(count*stride, stride, data);
}

BufferRange Buffer::assignIndices(uint32_t count, uint32_t stride,
	const void* data)
{
	return assign(count*stride, stride, data);
}

BufferRange Buffer::assignUBO(const uint32_t size,
	const void* data)
{
	getLimits();
	return assign(size, _alignUBO, data);
}

BufferRange Buffer::assignSSBO(const uint32_t size,
	const void* data)
{
	getLimits();
	return assign(size, _alignSSBO, data);
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
	// Writes
	for (const auto &p : _toWrite)
	{
		write(p.first, p.second.get());
	}
	_toWrite.clear();
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
	const vector<VertexInfo> &vertexInfo, const IndexInfo &indexInfo)
{
	_indexed = true;
	_vao = vao;
	_mode = mode;
	_count = indexInfo.count;
	_type = indexInfo.type;
	_indices = (void*)(intptr_t)indexInfo.range.getOffset();
	_elementBuffer = indexInfo.buffer;
	_vertexInfo = vertexInfo;
}

DrawCommand::DrawCommand(
	GLuint vao,
	GLenum mode,
	size_t count, 
	const vector<VertexInfo> &vertexInfo)
{
	_indexed = false;
	_vao = vao;
	_mode = mode;
	_count = count;
	_vertexInfo = vertexInfo;
}

void DrawCommand::draw(bool tessellated) const
{
	glBindVertexArray(_vao);
	for (const auto &info : _vertexInfo)
		glBindVertexBuffer(info.binding, info.buffer, info.range.getOffset(), info.stride);
	const GLenum mode = tessellated?GL_PATCHES:_mode;
	if (_indexed)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _elementBuffer);
		glDrawElements(mode, _count, _type, _indices);
	}
	else
	{
		glDrawArrays(mode, 0, _count);
	}
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