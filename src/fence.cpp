#include "fence.hpp"

#include <stdexcept>

Fence::Fence(Fence&& fence) : sync{fence.sync}
{
	fence.sync = 0;
}

Fence &Fence::operator=(Fence && fence)
{
	if (sync) glDeleteSync(sync);
	sync = fence.sync;
	fence.sync = 0;
}

Fence::~Fence()
{
	if (sync) glDeleteSync(sync);
}

void Fence::wait()
{
	if (!sync) return;

	glWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
}

void Fence::waitClient()
{
	if (!sync) return;

	const GLenum ret = glClientWaitSync(sync, 0, 0);

	if (ret == GL_WAIT_FAILED)
	{
		throw std::runtime_error("Fence wait failed");
	}
}

void Fence::lock()
{
	if (sync) glDeleteSync(sync);
	sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}