#pragma once

#include <GL/glew.h>

class Fence
{
public:
	void wait();
	void lock();

private:
	GLsync sync;
};