#pragma once

#include <string>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <utility>

class Screenshot
{
public:
	enum class Format
	{
		RGBA8, BGRA8
	};

	Screenshot();
	~Screenshot();
	bool isSaving();
	void save(
		const std::string &filename, 
		int width,
		int height,
		Format format,
		const std::vector<uint8_t> &data);

private:
	std::thread _t;
	std::mutex _mtx;
	std::condition_variable _cond;
	bool _save = false;
	bool _killThread = false;

	std::string _filename = "";
	int _width = 0;
	int _height = 0;
	Format _format = Format::RGBA8;
	std::vector<uint8_t> _data;
};