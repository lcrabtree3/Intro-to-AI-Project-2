#include "../server/Inventory.h"
#include <cassert>

void testInventory() {
    assert(itemToString(KEY) == "KEY");
    assert(itemToString(POTION) == "POTION");
    assert(itemToString(WEAPON) == "WEAPON");
}