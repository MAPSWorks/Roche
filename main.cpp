#include <iostream>
#include <cmath>
#include <cstring>

#include "game.h"

int main(int argc, char **argv)
{
  Game game;
  game.init();

  while (game.isRunning())
  {
    game.update();
    game.render();
  }
  return 0;
}
