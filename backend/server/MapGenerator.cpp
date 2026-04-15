#include "MapGenerator.h"
#include <cstdlib>

std::vector<std::vector<int>> generateMapGrid(int w, int h) {
    std::vector<std::vector<int>> grid(h, std::vector<int>(w));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            grid[y][x] = rand() % 3; // terrain types
        }
    }
    return grid;
}