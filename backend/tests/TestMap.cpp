#include "../server/MapGenerator.h"
#include <cassert>

void testMap() {
    auto map = generateMapGrid(10, 10);

    assert(map.size() == 10);
    assert(map[0].size() == 10);
}