#pragma once

#include <string>
#include "skin/image.h"

class ImageWriter {
public:
    // Write an Image to a PNG file at the given path.
    // Converts float RGBA [0,1] to uint8 RGBA [0,255].
    // Returns true on success, false on failure (e.g. invalid path).
    static bool writePNG(const Image& image, const std::string& path);
};
