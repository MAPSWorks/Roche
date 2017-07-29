#pragma once

#include "graphics_api.hpp"

/**
 * Waits on GPU commands to finish
 */
class Fence
{
public:
	Fence() = default;
	Fence(const Fence &) = delete;
	Fence(Fence &&);
	Fence& operator=(const Fence &) = delete;
	Fence& operator=(Fence &&);
	~Fence();
	/**
	 * Make the GL wait for GL commands submitted before the last call to lock()
	 * to finish. If lock() wasn't called, there is no wait.
	 */
	void wait();
	/**
	 * Make the application wait for GL commands submitted before the last call
	 * to lock() to finish. If lock() wasn't called, there is no wait.
	 * @param timeout time in ns before returning from the function 
	 * (-1 means wait indefinitely)
	 * @return whether the commands were finished before timeout occured
	 */
	bool waitClient(int64_t timeout=-1);
	/**
	 * Sets a fence after the submitted GL commands so calls to wait() and
	 * waitClient() have to wait on those commands to finish
	 */
	void lock();

private:
	/// GL sync object
	GLsync sync = 0;
};