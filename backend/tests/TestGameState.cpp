#include "../server/GameState.h"
#include <cassert>

void testGameState() {
    GameState game;

    game.movePlayer(0, 1, 0);

    auto json = game.toJSON();

    assert(json["players"].size() == 2);
}