#pragma once

#include <vector>
#include <map>
#include <memory>

#include "graphics_api.hpp"


/**
 * The half range [offset, offset+size) in bytes in a buffer
 */
class BufferRange
{
public:
	// Constructors
	BufferRange() = default;
	BufferRange(uint32_t offset, uint32_t size);
	// Getters
	uint32_t getOffset() const;
	uint32_t getSize() const;
private:
	uint32_t _offset = 0;
	uint32_t _size = 0;
};

class DrawCommand
{

public:
	DrawCommand() = default;
	DrawCommand(
		GLuint vao,
		GLenum mode,
		uint32_t count,
		GLenum type,
		uint32_t indexOffset, 
		uint32_t baseVertex);
	DrawCommand(GLuint vao, GLenum mode, GLenum type,
		size_t vertexSize, size_t indexSize,
		BufferRange vertices, BufferRange indices);
	void draw() const;
private:
	GLuint _vao;
	GLenum _mode;
	GLsizei _count;
	GLenum _type;
	GLvoid *_indices;
	GLint _baseVertex;
};

/**
 * Memory allocated from the GL, used to store ranges representing relevant
 * objects (vertex data, index data, uniform data, SSBO...)
 *
 * Using the buffer is done in three steps :
 * - Assigning ranges to get offsets in the buffer.
 * - Validating the ranges so writes and reads can take place. You can't assign
 * ranges after this step.
 * - Write to or read from the buffer.
 *
 * For GL operations you can request the buffer handle (getId()) and for dynamic
 * buffers, the pointer to mapped memory (getPtr()).
 */
class Buffer
{
public:
	enum class Usage
	{
		STATIC,DYNAMIC
	};

	enum class Access
	{
		NO_ACCESS, READ_ONLY, WRITE_ONLY, READ_WRITE
	};

	// Constructors
	/**
	 * Creates an empty buffer without OpenGL context attached.
	 */
	Buffer() = default;

	// Modifiers

	/**
	 * Creates a buffer
	 * (GL Context sensitve method!)
	 * @param dynamic set to true for a buffer where data is updated often,
	 * false for data updated rarely or never
	 * @param write application can write to the buffer
	 * @param read application can read from the buffer
	 */
	Buffer(Usage usage, Access access, uint32_t size=0);

	Buffer(const Buffer &) = delete;
	Buffer(Buffer &&);
	Buffer &operator=(const Buffer&) = delete;
	Buffer &operator=(Buffer &&);
	~Buffer();

	/**
	 * Reserves a range in the buffer
	 * @param size size in bytes of the range
	 * @param stride size in bytes between elements (or alignment requirements)
	 * @return offset and size of reserved range
	 */
	BufferRange assign(uint32_t size, uint32_t stride);
	/**
	 * Reserves a range of vertex data
	 * @param count number of vertices
	 * @param stride size in bytes of a vertex
	 * @return offset and size of reserved range
	 */
	BufferRange assignVertices(uint32_t count, uint32_t stride);
	/**
	 * Reserves a range of index data
	 * @param count number of indices
	 * @param stride size in bytes of an index
	 * @return offset and size of reserved range
	 */
	BufferRange assignIndices(uint32_t count, uint32_t stride);
	/**
	 * Reserves a range for a Uniform Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @return offset and size of reserved range
	 */
	BufferRange assignUBO(uint32_t size);
	/**
	 * Reserves a range for a Shader Storage Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @param data optional data
	 * @return offset and size of reserved range
	 */
	BufferRange assignSSBO(uint32_t size);

	/**
	 * Locks assigned ranges so writing and reading can take place
	 * (GL Context sensitve method!)
	 */
	void validate();
	/**
	 * Write data into a range
	 * (GL Context sensitve method!)
	 * @param range range to update
	 * @param data data to write to the buffer
	 */
	void write(BufferRange range, const void *data);
	/**
	 * Reads from the buffer
	 * (GL Context sensitve method!)
	 * @param range range to read from
	 * @param resulting data, allocating should be done by the user
	 */
	void read(BufferRange range, void *data);
	// Getters
	/// Returns the OpenGL buffer handle
	const GLuint &getId() const;
	/// Returns the mapped pointer (for off-thread copy)
	void* getPtr() const;
private:
	// Gets alignment requirements from the GL
	void getLimits();
	// Creates static storage for the buffer
	void storageStatic();
	// Creates dynamic storage for the buffer
	void storageDynamic();

	GLuint _id = 0; // OpenGL buffer name
	uint32_t _size = 0; // Buffer size in bytes
	bool _validated = false;
	uint32_t _lastOffset = 0; // Offset+size of last assigned element
	void *_mapPtr = nullptr; // Persistent map for dynamic buffers

	Usage _usage = Usage::STATIC;
	Access _access = Access::WRITE_ONLY;

	static uint32_t _alignUBO; // minimum alignment between UBOs
	static uint32_t _alignSSBO; // minimum alignment between SSBOs
	// Indicates if above alignments were requested from the GL
	static bool _limitsDefined; 
};