#include "game.hpp"

#include <thread>
#include <chrono>

int main(int argc, char **argv)
{
	// Time
	const int FRAMERATE_LIMIT = 60;
	const long FRAMETIME = (long)(1000000000.0/(double)FRAMERATE_LIMIT);
	std::chrono::nanoseconds FRAMETIME_DUR(FRAMETIME);

	// Game init
	Game game;
	game.init();


	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
	std::chrono::duration<double, std::nano> elapsed;

	double dt = 0.0;

	while (game.isRunning())
	{
		start = std::chrono::high_resolution_clock::now();
		game.update(dt);
		end = std::chrono::high_resolution_clock::now();
		elapsed = end-start;
		if (elapsed < FRAMETIME_DUR)
		{
			elapsed = FRAMETIME_DUR - elapsed;
			std::this_thread::sleep_for(elapsed);
			dt = (double)FRAMETIME/1000000000.0;
		}
		else
		{
			dt = ((double)elapsed.count()/1000000000.0);
		}
	}
	return 0;
}
