#pragma once

#include <map>
#include <vector>
#include <utility>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "ddsloader.hpp"

class DDSStreamer
{
public:
	typedef uint32_t Handle;

	class Data
	{
	public:
		Data() = default;
		Data(const DDSLoader &loader,
			std::vector<std::vector<uint8_t>> &&mips);
		int getMipmapCount() const;
		DDSLoader getLoader() const;
		std::vector<uint8_t> get(int i) const;
	private:
		DDSLoader _loader;
		std::vector<std::vector<uint8_t>> _mips;
	};

	DDSStreamer();
	~DDSStreamer();
	/**
	 * Submits a texture to load, returns a unique handle
	 */
	Handle submit(const DDSLoader &loader);
	/**
	 * Returns the loaded data and destroys the handle (if available) 
	 */
	std::pair<bool, Data> get(Handle handle);

private:
	Handle genHandle();
	void deleteHandle(Handle handle);

	std::mutex _lobbyMtx;
	std::condition_variable _cond;
	std::queue<Handle> _lobbyQueue;
	std::map<Handle, const DDSLoader> _lobby;
	std::mutex _readyMtx;
	std::map<Handle, Data> _ready;
	std::thread _t;
	bool _killThread = false;
};