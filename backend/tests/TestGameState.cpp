#include "../server/GameState.h"
#include <cassert>

void testGameState() {
    GameState game;

    auto initial = game.toJSON();
    assert(initial["players"].size() == 2);
    assert(initial["currentTurn"] == 0);
    assert(initial["inCombat"] == false);

    game.movePlayer(0, 1, 0);
    auto afterMove = game.toJSON();
    assert(afterMove["currentTurn"] == 1);

    game.processAction(1, "inspect the nearby ruins");
    auto afterAction = game.toJSON();
    assert(afterAction["currentTurn"] == 0);
    assert(afterAction["actionHistory"].size() == 1);

    game.forceCombatForTesting(0);
    auto combatStart = game.toJSON();
    assert(combatStart["inCombat"] == true);
    assert(combatStart["currentTurn"] == 0);
    assert(combatStart["combatSubmitted"][0] == false);
    assert(combatStart["combatSubmitted"][1] == false);

    game.combatTurn(0, "rock");
    auto afterFirstCombatAction = game.toJSON();
    assert(afterFirstCombatAction["inCombat"] == true);
    assert(afterFirstCombatAction["currentTurn"] == 1);
    assert(afterFirstCombatAction["combatSubmitted"][0] == true);
    assert(afterFirstCombatAction["combatSubmitted"][1] == false);

    game.combatTurn(1, "paper");
    auto afterSecondCombatAction = game.toJSON();
    assert(afterSecondCombatAction["currentTurn"] == 0 || afterSecondCombatAction["inCombat"] == false);
}