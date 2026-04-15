#include <iostream>

void testMap();
void testCombat();
void testInventory();
void testFog();
void testGameState();

int main() {
    std::cout << "Running tests...\n";

    testMap();
    testCombat();
    testInventory();
    testFog();
    testGameState();

    std::cout << "All tests passed.\n";
}