#include "dds_stream.hpp"

#include "gl_util.hpp"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <limits>

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

void DDSStreamer::init(bool asynchronous, int pageSize, int numPages, int maxSize)
{
	_asynchronous = asynchronous;
	_maxSize = (maxSize>0)?maxSize:numeric_limits<int>::max();

	_pageSize = pageSize;
	_numPages = numPages;
	int pboSize = pageSize*numPages;
	glCreateBuffers(1, &_pbo);
	GLbitfield storageFlags = GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT;
#ifdef USE_COHERENT_MAPPING
	storageFlags = storageFlags | GL_MAP_COHERENT_BIT;
#endif
	glNamedBufferStorage(_pbo, pboSize, nullptr, storageFlags);
	GLbitfield mapFlags = storageFlags;
#ifndef USE_COHERENT_MAPPING
	mapFlags = mapFlags | GL_MAP_FLUSH_EXPLICIT_BIT;
#endif
	_pboPtr = glMapNamedBufferRange(
		_pbo, 0, pboSize, mapFlags);

	_usedPages.resize(numPages, false);
	_pageFences.resize(numPages);

	// Don't need threading if synchronous
	if (!_asynchronous) return;

	_t = thread([this]{
		while (true)
		{
			LoadInfo info{};
			{
				unique_lock<mutex> lk(_mtx);
				_cond.wait(lk, [this]{ return _killThread || !_loadInfoQueue.empty();});
				if (_killThread) return;
				info = _loadInfoQueue[0];
				_loadInfoQueue.erase(_loadInfoQueue.begin());
			}

			// Use this to simulate slow load times (debug purposes)
			//this_thread::sleep_for(chrono::milliseconds(200));

			LoadData data = load(info);

			{
				lock_guard<mutex> lk(_dataMtx);
				_loadData.push_back(data);
			}
		}
	});
}

DDSStreamer::~DDSStreamer()
{
	if (_pbo)
	{
		glUnmapNamedBuffer(_pbo);
		glDeleteBuffers(1, &_pbo);
	}

	if (_t.joinable())
	{
		{
			lock_guard<mutex> lk(_mtx);
			_killThread = true;
		}
		_cond.notify_one();
		_t.join();
	}
}

struct TexInfo
{
	int size = 0;
	int levels = 0;
	string prefix = "";
	string separator = "";
	string suffix = "";
	bool rowColumnOrder = false;
};

TexInfo parseInfoFile(const string &filename, int maxSize)
{
	ifstream in(filename.c_str(), ios::in | ios::binary);
	if (!in) return {};

	string contents;
	in.seekg(0, ios::end);
	contents.resize(in.tellg());
	in.seekg(0, ios::beg);
	in.read(&contents[0], contents.size());

	try
	{
		shaun::parser p{};
		shaun::object obj = p.parse(contents.c_str());
		shaun::sweeper swp(&obj);

		TexInfo info{};
		info.size = swp("size").value<shaun::number>();
		info.levels = swp("levels").value<shaun::number>();
		info.prefix = (string)swp("prefix").value<shaun::string>();
		info.separator = (string)swp("separator").value<shaun::string>();
		info.suffix = (string)swp("suffix").value<shaun::string>();
		info.rowColumnOrder = swp("row_column_order").value<shaun::boolean>();

		int maxRows = maxSize/(info.size*2);
		int maxLevel = (int)floor(log2(maxRows))+1;
		info.levels = max(1,min(info.levels, maxLevel));
		return info;
	} 
	catch (shaun::parse_error &e)
	{
		cout << e << endl;
		return {};
	}
}

int DDSStreamer::getPageSpan(int size)
{
	return ((size-1)/_pageSize)+1;
}

DDSStreamer::Handle DDSStreamer::createTex(const string &filename)
{
	// Get info file
	TexInfo info = parseInfoFile(filename + "/info.sn", _maxSize);

	// Check if file exists or is valid
	if (info.levels == 0) return 0;

	const string tailFile = "/level0/" + info.prefix + "0" + info.separator + "0" + info.suffix;

	// Tail loader
	DDSLoader tailLoader = DDSLoader(filename+tailFile);

	// Storage params
	const int width = min(_maxSize,info.size<<(info.levels-1));
	const int height = width/2;
	const GLenum format = DDSFormatToGL(tailLoader.getFormat());
	const int mipNumber = mipmapCount(width);

	// Gen jobs
	vector<LoadInfo> jobs;

	// Gen texture & sampler
	const Handle h = genHandle();
	GLuint texId;
	glCreateTextures(GL_TEXTURE_2D, 1, &texId);
	glTextureStorage2D(texId, mipNumber, format, width, height);

	_texs.insert(make_pair(h, StreamTexture(texId)));

	// Unique for each tile
	int tileId = 0;

	// Tail mipmaps (level0)
	const int tailMipsFile = mipmapCount(info.size);
	const int tailMips = mipmapCount(min(_maxSize, info.size));
	const int skipMips = tailMipsFile-tailMips;
	for (int i=tailMips-1;i>=0;--i)
	{
		LoadInfo tailInfo{};
		tailInfo.handle = h;
		tailInfo.loader = tailLoader;
		tailInfo.fileLevel = i+skipMips;
		tailInfo.offsetX = 0;
		tailInfo.offsetY = 0;
		tailInfo.level = info.levels-1+i;
		tailInfo.imageSize = tailLoader.getImageSize(tailInfo.fileLevel);
		tailInfo.tileId = tileId;
		jobs.push_back(tailInfo);
		tileId += 1;
	}

	for (int i=1;i<info.levels;++i)
	{
		const string levelFolder = filename + "/level" + to_string(i) + "/";
		const int rows = 1<<(i-1);
		const int columns = 2*rows;
		const int level = info.levels-i-1;

		for (int x=0;x<columns;++x)
		{
			for (int y=0;y<rows;++y)
			{
				const string ddsFile = 
					info.prefix+
					to_string(info.rowColumnOrder?y:x)+
					info.separator+
					to_string(info.rowColumnOrder?x:y)+
					info.suffix;
				const string fullFilename = levelFolder+ddsFile;
				const DDSLoader loader(fullFilename);
				const int fileLevel = 0;
				const int imageSize = loader.getImageSize(fileLevel);

				LoadInfo loadInfo{};
				loadInfo.handle = h;
				loadInfo.loader = std::move(loader);
				loadInfo.fileLevel = fileLevel;
				loadInfo.offsetX = x*info.size;
				loadInfo.offsetY = y*info.size;
				loadInfo.level = level;
				loadInfo.imageSize = imageSize;
				loadInfo.tileId = tileId;
				jobs.push_back(loadInfo);
				tileId += 1;
			}
		}
	}

	_tileUpdated.insert(make_pair(h, vector<bool>(tileId, false)));

	if (_asynchronous)
	{
		_loadInfoWaiting.insert(_loadInfoWaiting.end(), jobs.begin(), jobs.end());
	}
	else
	{
		// Synchronous mode : load whole texture now
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbo);
		for (auto info : jobs)
		{
			while (info.pageOffset == -1)
			{
				auto fencesSignaled = areFencesSignaled();
				info.pageOffset = acquirePages(getPageSpan(info.imageSize), fencesSignaled);
			}
			LoadData d =  load(info);
			updateTile(d);
			releasePages(info.pageOffset, getPageSpan(info.imageSize));
		}
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		_texs[h].setComplete();
	}

	return h;
}



const StreamTexture &DDSStreamer::getTex(Handle handle)
{
	if (!handle) return _nullTex;
	auto it = _texs.find(handle);
	if (it == _texs.end()) return _nullTex;
	return it->second;
}

void DDSStreamer::deleteTex(Handle handle)
{
	if (handle)
	{
		_texDeleted.push_back(handle);
		_tileUpdated.erase(handle);
		_texs.erase(handle);
	}
}

vector<bool> DDSStreamer::areFencesSignaled()
{
	vector<bool> fencesAvailable(_pageFences.size());
	transform(_pageFences.begin(), _pageFences.end(), 
		fencesAvailable.begin(), [](Fence &fence){return fence.waitClient(0);});
	return fencesAvailable;
}

void DDSStreamer::update()
{
	if (!_asynchronous) return;
	// Invalidate deleted textures from pre-queue
	auto isDeleted = [this](const LoadInfo &info)
	{
		for (Handle h : _texDeleted)
		{
			if (h == info.handle)
			{
				if (info.pageOffset != -1) 
					releasePages(info.pageOffset, getPageSpan(info.imageSize));
				return true;
			}
		}
		return false;
	};
	// Invalidate deleted textures from pre-queue
	_loadInfoWaiting.erase(
		remove_if(_loadInfoWaiting.begin(), _loadInfoWaiting.end(), isDeleted),
		_loadInfoWaiting.end());
	{
		// Invalidate deleted textures from currently processing queue
		lock_guard<mutex> lk(_mtx);
		_loadInfoQueue.erase(
			remove_if(_loadInfoQueue.begin(), _loadInfoQueue.end(), isDeleted),
			_loadInfoQueue.end());
	}

	// Get fence state
	auto fencesSignaled = areFencesSignaled();
	// Mark textures as complete if fences are signaled
	setTexturesAsComplete(fencesSignaled);

	// Assign offsets
	std::vector<LoadInfo> assigned;
	std::vector<LoadInfo> nonAssigned;

	for_each(_loadInfoWaiting.begin(), _loadInfoWaiting.end(), 
		[&assigned, &nonAssigned, &fencesSignaled, this](const LoadInfo &info) {
			const int pages = getPageSpan(info.imageSize);
			const int pageOffset = acquirePages(pages, fencesSignaled);
			if (pageOffset == -1)
			{
				nonAssigned.push_back(info);
			}
			else
			{
				LoadInfo s = info;
				s.pageOffset = pageOffset;
				assigned.push_back(s);
				_tileRanges[make_pair(info.handle, info.tileId)] = 
					make_pair(pageOffset, pages);
			}
		});

	{
		lock_guard<mutex> lk(_mtx);
		// Submit created textures
		_loadInfoQueue.insert(
			_loadInfoQueue.end(),
			assigned.begin(),
			assigned.end());
	}

	_cond.notify_one();
	_loadInfoWaiting = nonAssigned;
	_texDeleted.clear();

	// Get loaded tiles
	const int maxCost = 20000000;
	vector<LoadData> data;
	{
		lock_guard<mutex> lk(_dataMtx);
		if (!_loadData.empty())
		{
			// Always accept first element
			auto first = _loadData.begin();
			data.push_back(*first);
			int currentCost = getCost(*first);
			_loadData.erase(first);
			// Choose next elements with cost
			_loadData.erase(std::remove_if(_loadData.begin(), _loadData.end(), [&](LoadData &d){
				currentCost += getCost(d);
				if (currentCost < maxCost)
				{
					data.push_back(d);
					return true;
				}
				return false;
			}), _loadData.end());
		}
	}
	// Update
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbo);
	for (LoadData d : data)
	{
		updateTile(d);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void DDSStreamer::setTexturesAsComplete(const vector<bool> &fencesSignaled)
{
	for (auto p : _tileUpdated)
	{
		if (!_texs[p.first].isComplete())
		{
			// Get if all tiles have been set in the process of updating
			if (all_of(p.second.begin(), p.second.end(), [](bool b){return b;}))
			{
				// Get if all fences have been signaled
				bool signaled = true;
				for (int i=0;(i<(int)p.second.size()) && signaled;++i)
				{
					auto range = _tileRanges[make_pair(p.first, i)];
					for (int j=range.first; j<range.second && signaled; ++j)
					{
						if (!fencesSignaled[j]) signaled = false;
					}
				}
				if (signaled)
				{
					_texs[p.first].setComplete();
					for (int i=0;i<(int)p.second.size();++i)
					{
						_tileRanges.erase(make_pair(p.first, i));
					}
				}
			}
		}
	}
}

int DDSStreamer::getCost(const LoadData &data)
{
	const int overheadCost = 2000;
	return overheadCost + data.imageSize;
}

void DDSStreamer::updateTile(const LoadData &d)
{
#ifndef USE_COHERENT_MAPPING
	glFlushMappedNamedBufferRange(_pbo, d.pageOffset*_pageSize, d.imageSize);
#endif
	auto it = _texs.find(d.handle);
	if (it != _texs.end())
	{
		auto &tex = it->second;
		glCompressedTextureSubImage2D(tex.getTextureId(),
			d.level,
			d.offsetX,
			d.offsetY,
			d.width,
			d.height,
			d.format,
			d.imageSize,
			(void*)(intptr_t)(d.pageOffset*_pageSize));

		_tileUpdated[d.handle][d.tileId] = true;
	}
	releasePages(d.pageOffset, getPageSpan(d.imageSize));
}

int DDSStreamer::acquirePages(int pages, const vector<bool> &fencesAvailable)
{
	if (pages > (int)_numPages)
	{
		throw runtime_error("Not enough pages");
	}

	int start = 0;
	for (int i=0;i<(int)_usedPages.size();++i)
	{
		if (_usedPages[i] || !fencesAvailable[i])
		{
			start = i+1;
		}
		else
		{
			if (i-start+1 == pages)
			{
				for (int j=start;j<start+pages;++j)
				{
					_usedPages[j] = true;
				}
				return start;
			}
		}
	}
	return -1;
}
void DDSStreamer::releasePages(int pageStart, int pages)
{
	for (int i=pageStart;i<pageStart+pages;++i)
	{
		_pageFences[i].lock();
		_usedPages[i] = false;
	}
}

DDSStreamer::LoadData DDSStreamer::load(const LoadInfo &info)
{
	LoadData s{};
	int level = info.fileLevel;
	int pageOffset = info.pageOffset;
	s.handle = info.handle;
	s.level = info.level;
	s.offsetX = info.offsetX;
	s.offsetY = info.offsetY;
	s.width = info.loader.getWidth(level);
	s.height = info.loader.getHeight(level);
	s.format  = DDSFormatToGL(info.loader.getFormat());
	s.imageSize = info.imageSize;
	s.pageOffset = pageOffset;
	s.tileId = info.tileId;

	info.loader.writeImageData(level, (char*)_pboPtr+pageOffset*_pageSize);

	return s;
}

DDSStreamer::Handle DDSStreamer::genHandle()
{
	static Handle h=0;
	++h;
	if (h == 0) ++h;
	return h;
}

StreamTexture::StreamTexture(GLuint id) :
	_texId{id}
{

}

StreamTexture::StreamTexture(StreamTexture &&tex) : 
	_texId{tex._texId}
{
	tex._texId = 0;
}

StreamTexture &StreamTexture::operator=(StreamTexture &&tex)
{
	if (_texId && tex._texId != _texId) glDeleteTextures(1, &_texId);
	_texId = tex._texId;
	tex._texId = 0;
	return *this;
}

StreamTexture::~StreamTexture()
{
	if (_texId) glDeleteTextures(1, &_texId);
}

void StreamTexture::setComplete()
{
	_complete = true;
}

GLuint StreamTexture::getTextureId(GLuint def) const
{
	if (_texId) return _texId;
	return def;
}

bool StreamTexture::isComplete() const
{
	return _complete;
}

GLuint StreamTexture::getCompleteTextureId(GLuint def) const
{
	if (isComplete()) return getTextureId(def);
	return def;
}