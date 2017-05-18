#pragma once

#include "graphics_api.hpp"

class Fence
{
public:
	void wait();
	void lock();

private:
	GLsync sync;
};