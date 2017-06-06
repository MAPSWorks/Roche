#include "screenshot.hpp"

#include <iostream>
#include <memory>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb_image_write.h"

using namespace std;

Screenshot::Screenshot()
{
	_t = thread([this]{
		while (true)
		{
			{
				unique_lock<mutex> lk(_mtx);
				_cond.wait(lk, [&]{return _killThread || _save;});
				if (_killThread) return;
			}

			// Create temp buffer for operations
			vector<uint8_t> buffer(4*_width*_height);

			// Flip upside down
			for (int i=0;i<_height;++i)
			{
				memcpy(buffer.data()+i*_width*4, 
					_data.data()+(_height-i-1)*_width*4, 
					_width*4);
			}

			// Flip GL_BGRA to GL_RGBA
			if (_format == Format::BGRA8)
			{
				for (int i=0;i<_width*_height*4;i+=4)
				{
					swap(buffer[i+0], buffer[i+2]);
				}
			}

			// Save screenshot
			if (!stbi_write_png(_filename.c_str(), 
				_width, _height, 4,
				buffer.data(), _width*4))
			{
				cout << "WARNING : Can't save screenshot " << 
					_filename << endl;
			}

			{
				lock_guard<mutex> lk(_mtx);
				_save = false;
			}
		}
	});
}

Screenshot::~Screenshot()
{
	{
		lock_guard<mutex> lk(_mtx);
		_killThread = true;
	}
	_cond.notify_one();

	_t.join();
}

bool Screenshot::isSaving()
{
	{
		unique_lock<mutex> lk(_mtx, defer_lock);
		if (lk.try_lock())
		{
			return _save;
		}
	}
	return true;
}

void Screenshot::save(
	const string &filename,
	const int width,
	const int height,
	const Format format,
	const vector<uint8_t> &data)
{
	if (isSaving()) return;

	{
		lock_guard<mutex> lk(_mtx);
		_save = true;
		_filename = filename;
		_width = width;
		_height = height;
		_format = format;
		_data = data;
	}
	_cond.notify_one();
}