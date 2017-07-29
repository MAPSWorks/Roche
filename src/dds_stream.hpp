#pragma once

#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>

#include "ddsloader.hpp"
#include "graphics_api.hpp"
#include "fence.hpp"

/**
 * Transforms a DDSLoader::Format to the corresponding GL format
 * @param format DDSLoader format
 * @return GL format
 */
GLenum DDSFormatToGL(DDSLoader::Format format);

/**
 * Texture streamed from the DDSStreamer class
 */
class StreamTexture
{
public:
	StreamTexture() = default;
	/** 
	 * @param id GL texture id
	 * @param samplerId GL sampler id
	 * @param minLod minimum LOD for sampler
	 */
	StreamTexture(GLuint id, GLuint samplerId, int minLod);
	StreamTexture(const StreamTexture &) = delete;
	StreamTexture &operator=(const StreamTexture &) = delete;
	StreamTexture(StreamTexture &&tex);
	StreamTexture &operator=(StreamTexture &&tex);
	~StreamTexture();

	/**
	 * Set to be usable in rendering
	 */
	void setComplete();
	/**
	 * Sets the minimum LOD value
	 * @param minLod minimum LOD value
	 */
	void setMinLod(int minLod);
	/**
	 * Returns the GL texture id\n
	 * WARNING: returns the GL texture id even if the texture isn't complete
	 * @param def default value to return if the texture does not exist
	 * @return GL texture id
	 */
	GLuint getTextureId(GLuint def=0) const;
	/**
	 * Indicates whether the texture is usable for rendering
	 */
	bool isComplete() const;
	/**
	 * Returns the minimum LOD value
	 */
	int getMinLod() const;
	/**
	 * Returns the texture GL id if it is usable for rendering
	 * @param def default value to return if the texture does not exist or
	 * it is incomplete
	 * @return GL texture id
	 */
	GLuint getCompleteTextureId(GLuint def=0) const;
	/**
	 * Returns the sampler id
	 * @param def default value to return if the texture does not exist or it is
	 * incomplete
	 */
	GLuint getSamplerId(GLuint def=0) const;

private:
	/// GL texture id
	GLuint _texId = 0;
	/// GL texture sampler
	GLuint _samplerId = 0;
	/// Minimum LOD
	int _minLod = 1000;
	/// Usable texture
	bool _complete = false;
};

/**
 * Asynchronously streams textures from file system to GL
 *
 * Keeps a large GL buffer of several pages of a given size which are assigned
 * to streaming texture data and then freed after the corresponding texture is
 * updated.
 */
class DDSStreamer
{
public:
	/// Handle for keeping track of stream textures
	typedef uint32_t Handle;

	DDSStreamer() = default;
	/**
	 * @param anisotropy Available anisotropy
	 * @param pageSize Size of a page in bytes
	 * @param numPages Number of pages in the buffer
	 * @param maxSize maximum texture width/height to load
	 */
	void init(int anisotropy, int pageSize, int numPages, int maxSize=0);
	~DDSStreamer();

	/**
	 * Creates a stream texture, setups streaming of its data and returns its 
	 * matching handle
	 * @param filename filename to load the texture from
	 * @return handle of newly created texture
	 */
	Handle createTex(const std::string &filename);
	/**
	 * Returns a stream texture from its matching handle created with createTex()
	 * @param handle handle of texture to return
	 * @param stream texture
	 */
	const StreamTexture &getTex(Handle handle);
	/**
	 * Deletes a stream texture and invalidates all transfer work on it
	 * @param handle handle of texture to delete
	 */
	void deleteTex(Handle handle);

	/**
	 * Updates GL textures with streamed data
	 */
	void update();

private:
	struct LoadInfo
	{
		/// Texture handle
		Handle handle;
		/// DDS Loader on file to load
		DDSLoader loader;
		/// Mip level in file to load
		int fileLevel;
		/// Offset in image to update
		int offsetX;
		/// Offset in image to update
		int offsetY;
		/// Mip Level of image to update
		int level;
		/// Size in bytes of data to update
		int imageSize;
		/// Unique id for this tile of this level
		int completenessId;
		/// Pointer of assigned buffer range (-1 when not yet assigned)
		int ptrOffset = -1;
	};

	struct LoadData
	{
		/// Texture handle
		Handle handle;
		/// Mip level of image to update
		int level;
		/// Offset in image to update
		int offsetX;
		/// Offset in image to update
		int offsetY;
		/// Width of tile
		int width;
		/// Height of tile
		int height;
		/// Format of incoming pixel data
		GLenum format;
		/// Size in bytes of incoming pixel data
		int imageSize;
		/// Point of assigned buffer range
		int ptrOffset;
		/// Unique id for this tile of this level
		int completenessId;
	};

	/**
	 * Acquires one or several free pages
	 * @param size in bytes to acquire
	 * @return index of first page acquired
	 */
	int acquirePages(int size);
	/**
	 * Releases one or several pages corresponding to buffer range
	 * @param offset offset in bytes of buffer range
	 * @param size size in bytes of buffer range
	 */
	void releasePages(int offset, int size);
	/**
	 * Loads an image from disk
	 * @param info input loading information
	 * @return output loading data
	 */
	LoadData load(const LoadInfo &info);

	/**
	 * Generates an unique handle
	 * @return unique handle
	 */
	Handle genHandle();

	/// Max anisotropy allowed
	int _anisotropy = 1;

	/// Maximum width/height of textures
	int _maxSize = 0;
	/// Size of pages in bytes
	int _pageSize = 0;
	/// Number of pages
	size_t _numPages = 0;

	/// GL id of Pixel Buffer
	GLuint _pbo = 0;
	/// Persistent map of pixel buffer
	void *_pboPtr = nullptr;
	/// Indicates whether a page is in use
	std::vector<bool> _usedPages;
	/// Fences to synchronize each page
	std::vector<Fence> _pageFences;

	/// Tile info waiting to be put in the streaming queue
	std::vector<LoadInfo> _loadInfoWaiting;
	/// Tile info queue in use by the loading thread
	std::vector<LoadInfo> _loadInfoQueue;
	/// Tile data that finished loading
	std::vector<LoadData> _loadData;

	/// Map of Handle->Stream Texture
	std::map<Handle, StreamTexture> _texs;
	/** Completeness of each mip level of each texture (when a mip is complete, 
	 * the sampler's min LOD is lowered) **/
	std::map<Handle, std::vector<std::vector<bool>>> _completeness;
	/// Textures to be deleted in next update() call
	std::vector<Handle> _texDeleted;
	/// Dummy texture to indicate inexistent texture
	StreamTexture _nullTex{};

	/// Synchronizes input
	std::mutex _mtx;
	/// Synchronizes output
	std::mutex _dataMtx;
	/// Streaming thread
	std::thread _t;
	/// Signals thread for it to terminate itself
	bool _killThread = false;
	/// Waits on tiles to load or thread to kill
	std::condition_variable _cond;
	
};