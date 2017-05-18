#pragma once

#include <vector>
#include <map>
#include <memory>

#include "graphics_api.hpp"


/**
 * The half range [offset, offset+size) in bytes in a buffer
 */
class BufferSection
{
public:
	// Constructors
	BufferSection();
	BufferSection(uint32_t offset, uint32_t size);
	// Getters
	uint32_t getOffset() const;
	uint32_t getSize() const;
private:
	uint32_t _offset, _size;
};

class DrawCommand
{

public:
	DrawCommand();
	DrawCommand(
		GLuint vao,
		GLenum mode,
		uint32_t count,
		GLenum type,
		uint32_t indexOffset, 
		uint32_t baseVertex);
	DrawCommand(GLuint vao, GLenum mode, GLenum type,
		size_t vertexSize, size_t indexSize,
		BufferSection vertices, BufferSection indices);
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
 * Memory allocated from the GL, used to store sections representing relevant
 * objects (vertex data, index data, uniform data, SSBO...)
 *
 * Objects are 'assigned', i.e. it gives back a valid section for the object 
 * when calling 'assign*()'. Updating the buffer is done by calling 'update*()'
 * with a section previously assigned. Then the data has to be transfered with 
 * either 'write()' or 'read()'.
 *
 * Please note multiple buffering has to be managed by the user, by assigning
 * multiple times the same object.
 */
class Buffer
{
public:
	// Constructors
	Buffer();

	// Modifiers

	/**
	 * Creates a buffer
	 * @param dynamic set to true for a buffer where data is updated often,
	 * false for data updated rarely or never
	 * @param write application can write to the buffer
	 * @param read application can read from the buffer
	 */
	Buffer(bool dynamic, bool write = true, bool read = false);

	/**
	 * Reserves a section of the buffer
	 * @param size size in bytes of the section
	 * @param stride size in bytes between elements (or alignment requirements)
	 * @param data optional data, is saved internally so you don't have to keep
	 * the pointer around
	 * @return offset and size of reserved section
	 */
	BufferSection assign(uint32_t size, uint32_t stride,
		const void *data=nullptr);
	/**
	 * Reserves a section of vertex data
	 * @param count number of vertices
	 * @param stride size in bytes of a vertex
	 * @param data optional data
	 * @return offset and size of reserved section
	 */
	BufferSection assignVertices(uint32_t count, uint32_t stride, 
		const void *data=nullptr);
	/**
	 * Reserves a section of index data
	 * @param count number of indices
	 * @param stride size in bytes of an index
	 * @param data optional data
	 * @return offset and size of reserved section
	 */
	BufferSection assignIndices(uint32_t count, uint32_t stride,
		const void *data=nullptr);

	/**
	 * Reserves a section for a Uniform Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @param data optional data
	 * @return offset and size of reserved section
	 */
	BufferSection assignUBO(uint32_t size , const void *data=nullptr);
	/**
	 * Reserves a section for a Shader Storage Buffer Object
	 * (GL Context sensitve method!)
	 * @param size size in bytes
	 * @param data optional data
	 * @return offset and size of reserved section
	 */
	BufferSection assignSSBO(uint32_t size, const void *data=nullptr);
	/**
	 * Updates a section with new data (for writing)
	 * @param section section to update
	 * @param data data to write to the buffer (is saved internally)
	 */
	void update(BufferSection section, const void *data);
	/**
	 * Writes all changes to the buffer
	 * (GL Context sensitve method!)
	 */
	void write();
	/**
	 * Reads from the buffer
	 * (GL Context sensitve method!)
	 * @param section section to read from
	 * @param resulting data, allocating should be done by the user
	 */
	void read(BufferSection section, void *data);
	// Getters
	GLuint getId() const;
private:
	// Gets alignment requirements from the GL
	void getLimits();
	// Aligns an offset
	uint32_t align(uint32_t offset, uint32_t align);
	// Creates static storage for the buffer
	void storageStatic();
	// Creates dynamic storage for the buffer
	void storageDynamic();
	// Locks the structure of the buffer (no assigning possible after that)
	void lockAssigning();

	GLuint _id; // OpenGL buffer name
	uint32_t _size; // Buffer size in bytes
	bool _built; // Indicates if the buffer structure has already been built
	uint32_t _lastOffset; // Offset+size of last assigned element
	void *_mapPtr; // Persistent map for dynamic buffers

	bool _dynamic; // 'dynamic' (updates every frame) or 'static' (rare updates)
	bool _write, _read; // write and read operations possible or not

	/* Data to upload at the next upload() call
	 * Key : offset in buffer, value : size and pointer to data */
	std::map<uint32_t, std::pair<
	uint32_t, std::shared_ptr<uint8_t>>> _dataToUpload;

	static uint32_t _alignUBO; // minimum alignment between UBOs
	static uint32_t _alignSSBO; // minimum alignment between SSBOs
	// Indicates if above alignments were requested from the GL
	static bool _limitsDefined; 
};