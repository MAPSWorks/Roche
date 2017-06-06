#include "dds_stream.hpp"

using namespace std;

DDSStreamer::Data::Data(
	const DDSLoader &loader,
	vector<vector<uint8_t>> &&mips) :
	_loader{loader},
	_mips{mips}
{

}

int DDSStreamer::Data::getMipmapCount() const
{
	return _mips.size();
}

DDSLoader DDSStreamer::Data::getLoader() const
{
	return _loader;
}

vector<uint8_t> DDSStreamer::Data::get(const int i) const
{
	return _mips.at(i);
}

DDSStreamer::DDSStreamer()
{
	_t = thread([this]{
		while (true)
		{
			// Wait for kill or queue not empty
			{
				unique_lock<mutex> lk(_lobbyMtx);
				_cond.wait(lk, [&]{ return _killThread || !_lobbyQueue.empty();});
				if (_killThread) return;
			}

			// Get info about texture were are going to load
			Handle handle;
			DDSLoader loader;
			{
				lock_guard<mutex> lk(_lobbyMtx);
				handle = _lobbyQueue.front();
				_lobbyQueue.pop();
				auto it = _lobby.find(handle);
				loader = std::move(it->second);
				_lobby.erase(it);
			}

			vector<vector<uint8_t>>mips(loader.getMipmapCount());
			for (int i=0;i<mips.size();++i)
			{
				mips[i] = loader.getImageData(i);
			}

			// Emulate slow loading times with this
			//this_thread::sleep_for(chrono::milliseconds(1000));
			
			// Push loaded texture into queue
			{
				lock_guard<mutex> lk(_readyMtx);
				_ready.insert(make_pair(handle, Data(loader, std::move(mips))));
			}
		}
	});
}

DDSStreamer::~DDSStreamer()
{
	{
		lock_guard<mutex> lk(_lobbyMtx);
		_killThread = true;
	}
	_cond.notify_one();
	_t.join();
}

DDSStreamer::Handle DDSStreamer::submit(const DDSLoader &loader)
{
	Handle handle = genHandle();
	{
		lock_guard<mutex> lk(_lobbyMtx);
		_lobbyQueue.push(handle);
		_lobby.insert(make_pair(handle, loader));
	}
	_cond.notify_one();
	return handle;
}

pair<bool, DDSStreamer::Data> DDSStreamer::get(const Handle handle)
{
	{
		unique_lock<mutex> lk(_readyMtx);
		auto it = _ready.find(handle);
		if (it != _ready.end())
		{
			deleteHandle(handle);
			auto res = make_pair(true, Data(std::move(it->second)));
			_ready.erase(it);
			return res;
		}
	}
	return make_pair(false, Data());
}

DDSStreamer::Handle DDSStreamer::genHandle()
{
	static Handle i=0;
	++i;
	return i;
}

void DDSStreamer::deleteHandle(Handle handle)
{

}