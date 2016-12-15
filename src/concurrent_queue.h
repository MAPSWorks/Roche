#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

template <class T>
class concurrent_queue
{
public:
	concurrent_queue() { }
	~concurrent_queue() { }
	
	bool empty()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return container_.empty();
	}
	
	size_t size()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return container_.size();
	}
	
	void push(const T& val)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		container_.push(val);
		pushed_.notify_one();
	}
	
	T wait_next()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		while (container_.empty())
			pushed_.wait(lock);
		
		T ret = container_.front();
		container_.pop();
		
		return ret;
	}

	bool try_next(T& ret)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		if (container_.empty())
			return false;

		ret = container_.front();
		container_.pop();
		return true;
	}
	
	void pop()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		container_.pop();
	}
	
	T& front()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return container_.front();
	}
private:
	std::condition_variable pushed_;
	std::queue<T> container_;
	std::mutex mtx_;
};