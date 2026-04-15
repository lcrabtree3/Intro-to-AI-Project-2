#include "../server/FogOfWar.h"
#include <vector>
#include <cassert>

void testFog() {
    std::vector<std::vector<bool>> fog(5, std::vector<bool>(5, false));

    reveal(fog, 2, 2, 1);

    assert(fog[2][2] == true);
    assert(fog[1][2] == true);
    assert(fog[2][1] == true);
    assert(fog[3][2] == true);
    assert(fog[2][3] == true);

    assert(fog[0][0] == false);
    assert(fog[4][4] == false);
}