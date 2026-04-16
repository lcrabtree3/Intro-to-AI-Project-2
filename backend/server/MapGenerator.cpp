#include "MapGenerator.h"
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace {
    constexpr float BASE_NOISE_SCALE = 0.05f;
    constexpr int OCTAVES = 4;
    constexpr float PERSISTENCE = 0.5f;
    constexpr float LACUNARITY = 2.0f;

    float fade(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float grad(int hash, float x, float y) {
        switch (hash & 7) {
        case 0: return x + y;
        case 1: return -x + y;
        case 2: return x - y;
        case 3: return -x - y;
        case 4: return x;
        case 5: return -x;
        case 6: return y;
        default: return -y;
        }
    }

    std::array<int, 512> buildPermutation() {
        std::array<int, 512> p{};
        std::array<int, 256> base{};
        std::iota(base.begin(), base.end(), 0);

        for (int i = 255; i > 0; --i) {
            int j = std::rand() % (i + 1);
            std::swap(base[i], base[j]);
        }

        for (int i = 0; i < 256; ++i) {
            p[i] = base[i];
            p[i + 256] = base[i];
        }
        return p;
    }

    float perlin2D(float x, float y, const std::array<int, 512>& p) {
        int xi = static_cast<int>(std::floor(x)) & 255;
        int yi = static_cast<int>(std::floor(y)) & 255;

        float xf = x - std::floor(x);
        float yf = y - std::floor(y);

        float u = fade(xf);
        float v = fade(yf);

        int aa = p[p[xi] + yi];
        int ab = p[p[xi] + yi + 1];
        int ba = p[p[xi + 1] + yi];
        int bb = p[p[xi + 1] + yi + 1];

        float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
        float x2 = lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);

        return lerp(x1, x2, v);
    }

    float octavePerlin2D(float x, float y, const std::array<int, 512>& p) {
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float noiseSum = 0.0f;
        float amplitudeSum = 0.0f;

        for (int i = 0; i < OCTAVES; ++i) {
            noiseSum += perlin2D(x * frequency, y * frequency, p) * amplitude;
            amplitudeSum += amplitude;
            amplitude *= PERSISTENCE;
            frequency *= LACUNARITY;
        }

        if (amplitudeSum <= 0.0f) return 0.0f;
        return noiseSum / amplitudeSum; // [-1, 1]
    }
}

std::vector<std::vector<int>> generateMapGrid(int w, int h) {
    std::vector<std::vector<int>> grid(h, std::vector<int>(w));
    const auto permutation = buildPermutation();
    const float offsetX = static_cast<float>(std::rand() % 10000);
    const float offsetY = static_cast<float>(std::rand() % 10000);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const float nx = (static_cast<float>(x) + offsetX) * BASE_NOISE_SCALE;
            const float ny = (static_cast<float>(y) + offsetY) * BASE_NOISE_SCALE;
            const float n = octavePerlin2D(nx, ny, permutation); // [-1, 1]

            if (n < -0.15f) {
                grid[y][x] = 2; // rocky/highlands
            } else if (n < 0.2f) {
                grid[y][x] = 1; // dirt/plains
            } else {
                grid[y][x] = 0; // forest
            }
        }
    }
    return grid;
}
