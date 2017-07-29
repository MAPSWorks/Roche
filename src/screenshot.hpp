#pragma once

#include <string>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <utility>

/**
 * Asynchronously saves images to file system
 */
class Screenshot
{
public:
	/// Layout of the pixels in an image
	enum class Format
	{
		RGBA8, BGRA8
	};

	Screenshot();
	~Screenshot();
	/// Check whether an image is currently being saved
	bool isSaving();
	/** Asynchronously saves an image to file system
	 * @param filename file to save to
	 * @param width width of the image in pixels
	 * @param height height of the image in pixels
	 * @param format @see Format
	 * @param data pixel data of the image
	 */
	void save(
		const std::string &filename, 
		int width,
		int height,
		Format format,
		const std::vector<uint8_t> &data);

private:
	/// Image saving thread
	std::thread _t;
	/// Synchronizes _save and _killThread
	std::mutex _mtx;
	/// Waits on _save and _killThread
	std::condition_variable _cond;
	/// Signals the thread to save an image
	bool _save = false;
	/// Signals the thread to terminate itself
	bool _killThread = false;

	/// Where to save the image
	std::string _filename = "";
	/// Width of the image in pixels
	int _width = 0;
	/// Height of the image in pixels
	int _height = 0;
	/// Format of the image
	Format _format = Format::RGBA8;
	/// Pixel data of the image
	std::vector<uint8_t> _data;
};