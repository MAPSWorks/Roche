#include "game.hpp"

#include <thread>
#include <chrono>

int main(int argc, char **argv)
{
	// Time
	const int maxFramerate{60};
	const long minFrameTime{(long)(1e9/(double)maxFramerate)};
	std::chrono::nanoseconds minFrameTimeNano{minFrameTime};

	// Game init
	Game game;
	game.init();

	double dt{0.0};

	while (game.isRunning())
	{
		const auto start{std::chrono::high_resolution_clock::now()};
		game.update(dt);
		const auto end{std::chrono::high_resolution_clock::now()};
		const std::chrono::duration<double, std::nano> elapsed{end-start};
		if (elapsed < minFrameTimeNano)
		{
			const auto sleepTime{minFrameTimeNano - elapsed};
			std::this_thread::sleep_for(sleepTime);
			dt = (double)minFrameTime/1e9;
		}
		else
		{
			dt = ((double)elapsed.count()/1e9);
		}
	}
	return 0;
}
