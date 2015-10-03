#include "game.h"

#include <thread>
#include <chrono>

int main(int argc, char **argv)
{

  const int FRAMERATE_LIMIT = 60;
  std::chrono::nanoseconds FRAMETIME((long)(1000000000.0/(double)FRAMERATE_LIMIT));

  Game game;
  game.init();

  std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
  std::chrono::duration<double> elapsed;

  while (game.isRunning())
  {
    start = std::chrono::high_resolution_clock::now();
    game.update();
    game.render();
    end = std::chrono::high_resolution_clock::now();
    elapsed = end-start;
    elapsed = FRAMETIME - elapsed;
    std::this_thread::sleep_for(elapsed);
  }
  return 0;
}
