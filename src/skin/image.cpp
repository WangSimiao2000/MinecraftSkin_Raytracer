#include "skin/image.h"
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <cstdint>

std::optional<Image> Image::load(const std::string& path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // force RGBA
    if (!data) {
        return std::nullopt;
    }

    Image img(w, h);
    for (int i = 0; i < w * h; ++i) {
        img.pixels[i] = Color(
            data[i * 4 + 0] / 255.0f,
            data[i * 4 + 1] / 255.0f,
            data[i * 4 + 2] / 255.0f,
            data[i * 4 + 3] / 255.0f
        );
    }

    stbi_image_free(data);
    return img;
}

void Image::savePNG(const std::string& path) const {
    std::vector<uint8_t> data(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        Color c = pixels[i].clamp();
        data[i * 4 + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
        data[i * 4 + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
        data[i * 4 + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
        data[i * 4 + 3] = static_cast<uint8_t>(c.a * 255.0f + 0.5f);
    }
    stbi_write_png(path.c_str(), width, height, 4, data.data(), width * 4);
}
