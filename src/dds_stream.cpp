#include "dds_stream.hpp"

#include "gl_util.hpp"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
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

	}
	return 0;
}

void DDSStreamer::init(int pageSize, int numPages, int maxSize)
{
	_maxSize = maxSize;
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

		if (maxSize)
		{
			int maxRows = maxSize/(info.size*2);
			int maxLevel = (int)floor(log2(maxRows))+1;
			info.levels = max(1,min(info.levels, maxLevel));
		}
		return info;
	} 
	catch (shaun::parse_error &e)
	{
		cout << e << endl;
		return {};
	}
}

DDSStreamer::Handle DDSStreamer::createTex(const string &filename)
{
	// Get info file
	TexInfo info = parseInfoFile(filename + "/info.sn", _maxSize);

	// Check if file exists or is valid
	if (info.levels == 0) return 0;

	const string tailFile = "/level0/0_0.DDS";

	// Gen texture
	const Handle h = genHandle();
	GLuint id;
	glCreateTextures(GL_TEXTURE_2D, 1, &id);
	_texs.insert(make_pair(h, StreamTexture(id)));

	// Tail loader
	DDSLoader tailLoader = DDSLoader(filename+tailFile);

	// Storage
	const int width = info.size<<(info.levels-1);
	const int height = width/2;
	const GLenum format = DDSFormatToGL(tailLoader.getFormat());
	glTextureStorage2D(id, mipmapCount(width), format, width, height);

	// Gen jobs
	vector<LoadInfo> jobs;

	// Tail mipmaps (level0)
	const int tailMips = mipmapCount(info.size);
	for (int i=tailMips-1;i>=0;--i)
	{
		LoadInfo tailInfo{};
		tailInfo.handle = h;
		tailInfo.loader = tailLoader;
		tailInfo.fileLevel = i;
		tailInfo.offsetX = 0;
		tailInfo.offsetY = 0;
		tailInfo.level = info.levels-1+i;
		tailInfo.imageSize = tailLoader.getImageSize(i);
		jobs.push_back(tailInfo);
	}

	for (int i=1;i<info.levels;++i)
	{
		const string levelFolder = filename + "/level" + to_string(i) + "/";
		const int rows = 1<<(i-1);
		const int columns = 2*rows;

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
				loadInfo.level = info.levels-i-1;
				loadInfo.imageSize = imageSize;
				jobs.push_back(loadInfo);
			}
		}
	}

	_loadInfoWaiting.insert(_loadInfoWaiting.end(), jobs.begin(), jobs.end());

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
	_texDeleted.push_back(handle);
	_texs.erase(handle);
}

void DDSStreamer::update()
{
	// Invalidate deleted textures from pre-queue
	auto isDeleted = [this](const LoadInfo &info)
	{
		for (Handle h : _texDeleted)
		{
			if (h == info.handle)
			{
				if (info.ptrOffset != -1) releasePages(info.ptrOffset, info.imageSize);
				return true;
			}
		}
		return false;
	};
	remove_if(_loadInfoWaiting.begin(), _loadInfoWaiting.end(), isDeleted);

	// Assign offsets
	std::vector<LoadInfo> assigned;
	std::vector<LoadInfo> nonAssigned;

	for_each(_loadInfoWaiting.begin(), _loadInfoWaiting.end(), 
		[&assigned, &nonAssigned, this](const LoadInfo &info) {
			int ptrOffset = acquirePages(info.imageSize);
			if (ptrOffset == -1)
			{
				nonAssigned.push_back(info);
			}
			else
			{
				LoadInfo s = info;
				s.ptrOffset = ptrOffset;
				assigned.push_back(s);
			}
		});

	{
		lock_guard<mutex> lk(_mtx);
		// Invalidate deleted textures from queue
		remove_if(_loadInfoQueue.begin(), _loadInfoQueue.end(), isDeleted);
		// Submit created textures
		_loadInfoQueue.insert(
			_loadInfoQueue.end(),
			assigned.begin(),
			assigned.end());
	}
	_cond.notify_one();
	_loadInfoWaiting = nonAssigned;
	_texDeleted.clear();

	// Get loaded slices
	vector<LoadData> data;
	{
		lock_guard<mutex> lk(_dataMtx);
		data = _loadData;
		_loadData.clear();
	}
	// Update
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbo);
	for (LoadData d : data)
	{
#ifndef USE_COHERENT_MAPPING
		glFlushMappedNamedBufferRange(_pbo, d.ptrOffset, d.imageSize);
#endif
		auto it = _texs.find(d.handle);
		if (it != _texs.end())
		{
			glCompressedTextureSubImage2D(it->second.getId(),
				d.level,
				d.offsetX,
				d.offsetY,
				d.width,
				d.height,
				d.format,
				d.imageSize,
				(void*)(intptr_t)d.ptrOffset);
		}
		releasePages(d.ptrOffset, d.imageSize);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

int DDSStreamer::acquirePages(int size)
{
	const int pages = ((size-1)/_pageSize)+1;
	if (pages > _numPages)
	{
		throw runtime_error("Not enough pages");
	}

	int start = 0;
	for (int i=0;i<_usedPages.size();++i)
	{
		if (_usedPages[i] || !_pageFences[i].waitClient(1000))
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
				return start*_pageSize;
			}
		}
	}
	return -1;
}
void DDSStreamer::releasePages(int offset, int size)
{
	const int pageStart = offset/_pageSize;
	const int pages = ((size-1)/_pageSize)+1;
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
	int ptrOffset = info.ptrOffset;
	s.handle = info.handle;
	s.level = info.level;
	s.offsetX = info.offsetX;
	s.offsetY = info.offsetY;
	s.width = info.loader.getWidth(level);
	s.height = info.loader.getHeight(level);
	s.format  = DDSFormatToGL(info.loader.getFormat());
	s.imageSize = info.imageSize;
	s.ptrOffset = ptrOffset;

	info.loader.writeImageData(level, (char*)_pboPtr+ptrOffset);

	return s;
}

DDSStreamer::Handle DDSStreamer::genHandle()
{
	static Handle h=0;
	++h;
	if (h == 0) ++h;
	return h;
}

StreamTexture::StreamTexture(GLuint id) :_id{id}
{

}

StreamTexture::StreamTexture(StreamTexture &&tex) : _id{tex._id}
{
	tex._id = 0;
}

StreamTexture &StreamTexture::operator=(StreamTexture &&tex)
{
	if (tex._id != _id) glDeleteTextures(1, &_id);
	_id = tex._id;
	tex._id = 0;
	return *this;
}

StreamTexture::~StreamTexture()
{
	if (_id) glDeleteTextures(1, &_id);
}

GLuint StreamTexture::getId(GLuint def) const
{
	if (_id) return _id;
	return def;
}