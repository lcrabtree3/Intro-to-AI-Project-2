#include "FogOfWar.h"

void reveal(std::vector<std::vector<bool>>& fog, int x, int y, int r) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int nx = x + dx;
            int ny = y + dy;

            if (ny >= 0 && ny < fog.size() && nx >= 0 && nx < fog[0].size()) {
                fog[ny][nx] = true;
            }
        }
    }
}