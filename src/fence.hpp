#pragma once

#include "graphics_api.hpp"

class Fence
{
public:
	Fence() = default;
	Fence(const Fence &) = delete;
	Fence(Fence &&);
	Fence& operator=(const Fence &) = delete;
	Fence& operator=(Fence &&);
	~Fence();
	void wait();
	void lock();

private:
	GLsync sync = 0;
};