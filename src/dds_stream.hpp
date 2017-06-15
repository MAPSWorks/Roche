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

GLenum DDSFormatToGL(DDSLoader::Format format);

class StreamTexture
{
public:
	StreamTexture() = default;
	explicit StreamTexture(GLuint id);
	StreamTexture(const StreamTexture &) = delete;
	StreamTexture &operator=(const StreamTexture &) = delete;
	StreamTexture(StreamTexture &&tex);
	StreamTexture &operator=(StreamTexture &&tex);
	~StreamTexture();

	GLuint getId(GLuint def=0) const;

private:
	GLuint _id = 0;
};

class DDSStreamer
{
public:
	typedef uint32_t Handle;

	DDSStreamer() = default;
	void init(int sliceSize, int numSlices);
	~DDSStreamer();

	Handle createTex(const std::string &filename);
	const StreamTexture &getTex(Handle handle);
	void deleteTex(Handle handle);

	void update();

private:
	struct SliceInfo
	{
		Handle handle;
		DDSLoader loader;
		int fileLevel;
		int offsetX;
		int offsetY;
		int level;
		int slice = -1;
	};

	struct SliceData
	{
		Handle handle;
		int level;
		int offsetX;
		int offsetY;
		int width;
		int height;
		GLenum format;
		int imageSize;
		int ptrOffset;
		int slice;
	};

	int acquireSlice();
	void releaseSlice(int slice);
	SliceData load(const SliceInfo &info);

	int getSlicePtrOffset(int slice);

	Handle genHandle();

	int _sliceSize = 0;
	int _numSlices = 0;
	GLuint _pbo = 0;
	void *_pboPtr = nullptr;
	std::vector<bool> _usedSlices;
	std::vector<Fence> _sliceFences;

	std::vector<SliceInfo> _sliceInfoWaiting; // To be added to queue
	std::vector<SliceInfo> _sliceInfoQueue; // Currently loading
	std::vector<SliceData> _sliceData; // Finished loading

	std::map<Handle, StreamTexture> _texs;
	std::vector<Handle> _texDeleted;
	StreamTexture _nullTex{};

	std::mutex _mtx;
	std::mutex _dataMtx;
	std::thread _t;
	bool _killThread = false;
	std::condition_variable _cond;
	
};