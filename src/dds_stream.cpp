#include "dds_stream.hpp"

#include "gl_util.hpp"

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>

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

void DDSStreamer::init(int sliceSize, int numSlices)
{
	_sliceSize = sliceSize;
	_numSlices = numSlices;
	int pboSize = getSlicePtrOffset(numSlices);
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

	_usedSlices.resize(numSlices, false);
	_sliceFences.resize(numSlices);

	_t = thread([this]{
		while (true)
		{
			SliceInfo info{};
			{
				unique_lock<mutex> lk(_mtx);
				_cond.wait(lk, [this]{ return _killThread || !_sliceInfoQueue.empty();});
				if (_killThread) return;
				info = _sliceInfoQueue[0];
				_sliceInfoQueue.erase(_sliceInfoQueue.begin());
			}

			SliceData data = load(info);

			{
				lock_guard<mutex> lk(_dataMtx);
				_sliceData.push_back(data);
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
};

TexInfo parseInfoFile(const string &filename)
{
	ifstream in(filename.c_str(), ios::in | ios::binary);
	if (!in) return {};

	string contents;
	in.seekg(0, ios::end);
	contents.resize(in.tellg());
	in.seekg(0, ios::beg);
	in.read(&contents[0], contents.size());

	using namespace shaun;
	try
	{
		parser p{};
		object obj = p.parse(contents.c_str());
		sweeper swp(&obj);

		TexInfo info{};
		info.size = swp("size").value<number>();
		info.levels = swp("levels").value<number>();

		return info;
	} 
	catch (parse_error &e)
	{
		cout << e << endl;
		return {};
	}
}

DDSStreamer::Handle DDSStreamer::createTex(const string &filename)
{
	// Get info file
	TexInfo info = parseInfoFile(filename + "/info.sn");

	// Check if file exists or is valid
	if (info.levels == 0) return 0;

	// Check slice size is smaller or equal
	if (info.size > _sliceSize)
	{
		cout << "Warning: " << filename << " has slices of size " << info.size
			<< " (max " << _sliceSize << ")" << endl;
		return 0;
	}

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
	vector<SliceInfo> jobs;

	// Tail mipmaps (level0)
	int tailMips = mipmapCount(info.size);
	for (int i=tailMips-1;i>=0;--i)
	{
		SliceInfo tailInfo{};
		tailInfo.handle = h;
		tailInfo.loader = tailLoader;
		tailInfo.fileLevel = i;
		tailInfo.offsetX = 0;
		tailInfo.offsetY = 0;
		tailInfo.level = info.levels-1+i;
		jobs.push_back(tailInfo);
	}

	for (int i=1;i<info.levels;++i)
	{
		const string levelFolder = filename + "/level" + to_string(i) + "/";
		const int columns = 2*i;
		const int rows = 1*i;

		for (int x=0;x<columns;++x)
		{
			for (int y=0;y<rows;++y)
			{
				const string ddsFile = to_string(x)+"_"+to_string(y)+".DDS";
				const string fullFilename = levelFolder+ddsFile;
				SliceInfo sliceInfo{};
				sliceInfo.handle = h;
				sliceInfo.loader = DDSLoader(fullFilename);
				sliceInfo.fileLevel = 0;
				sliceInfo.offsetX = x*info.size;
				sliceInfo.offsetY = y*info.size;
				sliceInfo.level = info.levels-i-1;
				jobs.push_back(sliceInfo);
			}
		}
	}

	_sliceInfoWaiting.insert(_sliceInfoWaiting.end(), jobs.begin(), jobs.end());

	return h;
}

const StreamTexture &DDSStreamer::getTex(Handle handle)
{
	if (!handle) return _nullTex;
	auto it = _texs.find(handle);
	if (it == _texs.end()) return _nullTex;
	return _texs[handle];
}

void DDSStreamer::deleteTex(Handle handle)
{
	_texDeleted.push_back(handle);
	_texs.erase(handle);
}

void DDSStreamer::update()
{
	// Invalidate deleted textures from pre-queue
	auto isDeleted = [this](const SliceInfo &info)
	{
		for (Handle h : _texDeleted)
		{
			if (h == info.handle)
			{
				if (info.slice != -1) releaseSlice(info.slice);
				return true;
			}
		}
		return false;
	};
	remove_if(_sliceInfoWaiting.begin(), _sliceInfoWaiting.end(), isDeleted);

	// Assign slices to new jobs
	transform(_sliceInfoWaiting.begin(), _sliceInfoWaiting.end(),
		_sliceInfoWaiting.begin(),
		[this](const SliceInfo &info)
		{
			SliceInfo s = info;
			s.slice = acquireSlice();
			return s;
		});

	{
		lock_guard<mutex> lk(_mtx);
		// Invalidate deleted textures from queue
		remove_if(_sliceInfoQueue.begin(), _sliceInfoQueue.end(), isDeleted);
		// Submit created textures
		_sliceInfoQueue.insert(
			_sliceInfoQueue.end(),
			_sliceInfoWaiting.begin(),
			_sliceInfoWaiting.end());
	}
	_cond.notify_one();
	_sliceInfoWaiting.clear();
	_texDeleted.clear();

	// Get loaded slices
	vector<SliceData> data;
	{
		lock_guard<mutex> lk(_dataMtx);
		data = _sliceData;
		_sliceData.clear();
	}
	// Update
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbo);
	for (SliceData d : data)
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
		releaseSlice(d.slice);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

int DDSStreamer::acquireSlice()
{
	while (true)
	{
		for (int i=0;i<_usedSlices.size();++i)
		{
			if (!_usedSlices[i])
			{
				if (_sliceFences[i].waitClient(1000))
				{
					_usedSlices[i] = true;
					return i;
				}
			}
		}
	}
}
void DDSStreamer::releaseSlice(int slice)
{
	_sliceFences[slice].lock();
	_usedSlices[slice] = false;
}

DDSStreamer::SliceData DDSStreamer::load(const SliceInfo &info)
{
	SliceData s{};
	int level = info.fileLevel;
	int ptrOffset = getSlicePtrOffset(info.slice);
	s.handle = info.handle;
	s.level = info.level;
	s.offsetX = info.offsetX;
	s.offsetY = info.offsetY;
	s.width = info.loader.getWidth(level);
	s.height = info.loader.getHeight(level);
	s.format  = DDSFormatToGL(info.loader.getFormat());
	s.imageSize = info.loader.getImageSize(level);
	s.ptrOffset = ptrOffset;

	info.loader.writeImageData(level, (char*)_pboPtr+ptrOffset);

	return s;
}

int DDSStreamer::getSlicePtrOffset(int slice)
{
	return slice*_sliceSize*_sliceSize;
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