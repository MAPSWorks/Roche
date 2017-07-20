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
	explicit StreamTexture(GLuint id, GLuint samplerId, int minLod);
	StreamTexture(const StreamTexture &) = delete;
	StreamTexture &operator=(const StreamTexture &) = delete;
	StreamTexture(StreamTexture &&tex);
	StreamTexture &operator=(StreamTexture &&tex);
	~StreamTexture();

	void setComplete();
	void setMinLod(int minLod);

	GLuint getTextureId(GLuint def=0) const;
	bool isComplete() const;
	int getMinLod() const;
	GLuint getCompleteTextureId(GLuint def=0) const;
	GLuint getSamplerId(GLuint def=0) const;

private:
	GLuint _texId = 0;
	GLuint _samplerId = 0;
	int _minLod = 1000;
	bool _complete = false;
};

class DDSStreamer
{
public:
	typedef uint32_t Handle;

	DDSStreamer() = default;
	void init(int anisotropy, int pageSize, int numPages, int maxSize=0);
	~DDSStreamer();

	Handle createTex(const std::string &filename);
	const StreamTexture &getTex(Handle handle);
	void deleteTex(Handle handle);

	void update();

private:
	struct LoadInfo
	{
		Handle handle;
		DDSLoader loader;
		int fileLevel;
		int offsetX;
		int offsetY;
		int level;
		int imageSize;
		int completenessId;
		int ptrOffset = -1;
	};

	struct LoadData
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
		int completenessId;
	};

	int acquirePages(int size);
	void releasePages(int offset, int size);
	LoadData load(const LoadInfo &info);

	Handle genHandle();

	int _anisotropy = 1;

	int _maxSize = 0;
	int _pageSize = 0;
	int _numPages = 0;
	GLuint _pbo = 0;
	void *_pboPtr = nullptr;
	std::vector<bool> _usedPages;
	std::vector<Fence> _pageFences;

	std::vector<LoadInfo> _loadInfoWaiting; // To be added to queue
	std::vector<LoadInfo> _loadInfoQueue; // Currently loading
	std::vector<LoadData> _loadData; // Finished loading

	std::map<Handle, StreamTexture> _texs;
	std::map<Handle, std::vector<std::vector<bool>>> _completeness;
	std::vector<Handle> _texDeleted;
	StreamTexture _nullTex{};

	std::mutex _mtx;
	std::mutex _dataMtx;
	std::thread _t;
	bool _killThread = false;
	std::condition_variable _cond;
	
};