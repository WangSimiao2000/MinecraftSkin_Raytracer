#include "output/image_writer.h"
#include <stb/stb_image_write.h>
#include <cstdint>
#include <vector>

bool ImageWriter::writePNG(const Image& image, const std::string& path) {
    if (image.width <= 0 || image.height <= 0 || path.empty()) {
        return false;
    }

    const int numPixels = image.width * image.height;
    if (static_cast<int>(image.pixels.size()) < numPixels) {
        return false;
    }

    std::vector<uint8_t> data(numPixels * 4);
    for (int i = 0; i < numPixels; ++i) {
        Color c = image.pixels[i].clamp();
        data[i * 4 + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
        data[i * 4 + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
        data[i * 4 + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
        data[i * 4 + 3] = static_cast<uint8_t>(c.a * 255.0f + 0.5f);
    }

    int stride = image.width * 4;
    int result = stbi_write_png(path.c_str(), image.width, image.height, 4, data.data(), stride);
    return result != 0;
}
