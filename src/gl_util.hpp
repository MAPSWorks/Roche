#pragma once

#include <vector>
#include <map>
#include <memory>

#include "graphics_api.hpp"
#include "ddsloader.hpp"

/**
 * Transforms a DDSLoader::Format to the corresponding GL format
 * @param format DDSLoader format
 * @return GL format
 */
GLenum DDSFormatToGL(DDSLoader::Format format);

/** Computes the number of mips necessary to have a complete chain
 * @param size largest dimension of texture (max(width, height, depth))
 * @returns number of mips
 */
int mipmapCount(int size);
/** Computes the size of dimension of texture at given level
 * @param size size of dimension at level 0
 * @param level mipmap level
 * @returns size of dimension at given level
 */
int mipmapSize(int size, int level);

/**
 * The half range [offset, offset+size) in bytes in a buffer
 */
class BufferRange
{
public:
	// Constructors
	BufferRange() = default;
	/// Construcs the range with an offset in bytes and a size in bytes
	BufferRange(uint32_t offset, uint32_t size);
	// Getters
	/// Returns the offset in bytes
	uint32_t getOffset() const;
	/// Returns the size in bytes
	uint32_t getSize() const;
private:
	/// Offset in bytes
	uint32_t _offset = 0;
	/// Size in bytes
	uint32_t _size = 0;
};

/**
 * Information necessary to draw geometry
 */
class DrawCommand
{

public:
	DrawCommand() = default;
	/** Complete constructor
	 * @param vao GL VAO
	 * @param mode GL rendering mode (GL_POINTS, GL_LINES, GL_TRIANGLES, GL_PATCHES)
	 * @param count number of indices
	 * @param type type of indices (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT)
	 * @param indexOffset offset in bytes of first index in index buffer bound to vao
	 * @param baseVertex offset in vertices of first vertex in vertex buffer bound to vao
	 */

	struct VertexInfo
	{
		GLuint binding;
		GLuint buffer;
		BufferRange range;
		int stride;
	};

	struct IndexInfo
	{
		GLuint buffer;
		BufferRange range;
		size_t count;
	};
	DrawCommand(GLuint vao, GLenum mode, GLenum type,
		const std::vector<VertexInfo> &vertexInfo, const IndexInfo &indexInfo);
	/** Draw model */
	void draw(bool tessellated = false) const;
private:
	GLenum _vao;
	GLenum _mode;
	GLsizei _count;
	GLenum _type;
	GLvoid *_indices;
	GLuint _elementBuffer;
	std::vector<VertexInfo> _vertexInfo;
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
	/// Indicates whether data is updated once (static) or several times (dynamic)
	enum class Usage
	{
		STATIC, DYNAMIC
	};
	/// Access flags for data inside the buffer
	enum class Access
	{
		NO_ACCESS, READ_ONLY, WRITE_ONLY, READ_WRITE
	};

	// Constructors
	/// Empty constructor
	Buffer() = default;

	// Modifiers

	/**
	 * Creates a buffer
	 * (GL Context sensitive method!)
	 * @param size size in bytes of the buffer if known
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
	 * @return reserved range
	 */
	BufferRange assign(uint32_t size, uint32_t stride);
	/**
	 * Reserves a range of vertex data
	 * @param count number of vertices
	 * @param stride size in bytes of a vertex
	 * @return reserved range
	 */
	BufferRange assignVertices(uint32_t count, uint32_t stride);
	/**
	 * Reserves a range of index data
	 * @param count number of indices
	 * @param stride size in bytes of an index
	 * @return reserved range
	 */
	BufferRange assignIndices(uint32_t count, uint32_t stride);
	/**
	 * Reserves a range for a Uniform Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @return reserved range
	 */
	BufferRange assignUBO(uint32_t size);
	/**
	 * Reserves a range for a Shader Storage Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @param data optional data
	 * @return reserved range
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
	/// Returns the OpenGL buffer name
	const GLuint &getId() const;
	/// Returns the mapped pointer (for manual writes/reads)
	void* getPtr() const;
private:
	/// Gets alignment requirements from the GL
	void getLimits();
	/// Creates static storage for the buffer
	void storageStatic();
	/// Creates dynamic storage for the buffer
	void storageDynamic();

	/// GL buffer ID
	GLuint _id = 0; 
	/// Buffer size in bytes
	uint32_t _size = 0;
	/// Whether the buffer is ready for writing/reading
	bool _validated = false;
	/// Offset+size of last assigned element
	uint32_t _lastOffset = 0;
	/// Persistent map for dynamic buffers
	void *_mapPtr = nullptr; 

	/// Buffer usage
	Usage _usage = Usage::STATIC;
	/// Buffer access flags
	Access _access = Access::WRITE_ONLY;

	/// Minimum alignment between UBOs
	static uint32_t _alignUBO;
	/// minimum alignment between SSBOs
	static uint32_t _alignSSBO; 
	/// Indicates if above alignments were already requested from the GL
	static bool _limitsDefined; 
};