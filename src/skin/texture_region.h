#pragma once

#include <vector>
#include <algorithm>
#include "math/color.h"

struct TextureRegion {
    int width = 0;
    int height = 0;
    std::vector<Color> pixels;  // row-major

    TextureRegion() = default;
    TextureRegion(int w, int h) : width(w), height(h), pixels(w * h) {}
    TextureRegion(int w, int h, std::vector<Color> px)
        : width(w), height(h), pixels(std::move(px)) {}

    // UV sampling with nearest-neighbor interpolation
    // u in [0,1] maps to width, v in [0,1] maps to height
    Color sample(float u, float v) const {
        if (width <= 0 || height <= 0 || pixels.empty()) {
            return Color();
        }
        int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
        int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
        return pixels[y * width + x];
    }
};
