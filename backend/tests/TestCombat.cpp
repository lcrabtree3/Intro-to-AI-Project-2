#include "../server/Combat.h"
#include <cassert>

void testCombat() {
    std::string result = resolveCombat("rock");

    assert(result == "Win" || result == "Lose" || result == "Tie");
}