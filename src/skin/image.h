#pragma once

#include <string>
#include <vector>
#include <optional>
#include "math/color.h"
#include "skin/texture_region.h"

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> pixels;  // row-major RGBA, float 0-1

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(w * h) {}

    // Load a PNG file. Returns std::nullopt on failure.
    static std::optional<Image> load(const std::string& path);

    // Extract a rectangular sub-region as a TextureRegion
    TextureRegion extractRegion(int x, int y, int w, int h) const {
        TextureRegion region(w, h);
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                int srcX = x + col;
                int srcY = y + row;
                if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                    region.pixels[row * w + col] = pixels[srcY * width + srcX];
                }
            }
        }
        return region;
    }

    // Save as PNG file
    void savePNG(const std::string& path) const;
};
