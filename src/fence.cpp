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
  return *this;
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

bool Fence::waitClient(uint64_t timeout)
{
	if (!sync) return true;

	const GLenum ret = glClientWaitSync(sync, 0, timeout);

	if (ret == GL_CONDITION_SATISFIED ||
		ret == GL_ALREADY_SIGNALED) return true;

	if (ret == GL_TIMEOUT_EXPIRED) return false;

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