#pragma once

#include <string>
#include <optional>
#include "skin/image.h"
#include "skin/texture_region.h"

// Simple Result type for returning success or error
template<typename T, typename E>
struct Result {
    std::optional<T> value;
    std::optional<E> error;

    bool isOk() const { return value.has_value(); }

    static Result ok(T val) {
        Result r;
        r.value = std::move(val);
        return r;
    }

    static Result err(E e) {
        Result r;
        r.error = std::move(e);
        return r;
    }
};

// Texture set for one body part (6 faces of a box)
struct BodyPartTexture {
    TextureRegion top, bottom, front, back, left, right;
};

// Complete parsed skin data
struct SkinData {
    enum Format { NEW_64x64, OLD_64x32 };
    Format format;

    // Inner layer textures
    BodyPartTexture head, body, rightArm, leftArm, rightLeg, leftLeg;
    // Outer layer textures
    BodyPartTexture headOuter, bodyOuter, rightArmOuter, leftArmOuter, rightLegOuter, leftLegOuter;
};

class SkinParser {
public:
    // Parse a skin file, auto-detecting 64x64 or 64x32 format
    static Result<SkinData, std::string> parse(const std::string& filePath);

    // Utility: mirror a TextureRegion horizontally
    static TextureRegion mirrorHorizontal(const TextureRegion& region);

private:
    // Extract body part textures from a box-layout region in the image.
    // (ox, oy) is the top-left origin of the body part's texture block.
    // w, h, d are the box dimensions (width, height, depth) in pixels.
    static BodyPartTexture extractBodyPart(const Image& img, int ox, int oy, int w, int h, int d);

    // Parse 64x64 new format
    static SkinData parseNew(const Image& img);

    // Parse 64x32 old format
    static SkinData parseOld(const Image& img);

    // Mirror an entire BodyPartTexture horizontally (and swap left/right faces)
    static BodyPartTexture mirrorBodyPart(const BodyPartTexture& part);
};
