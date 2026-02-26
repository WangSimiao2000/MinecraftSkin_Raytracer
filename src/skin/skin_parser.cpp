#include "skin/skin_parser.h"
#include <algorithm>

// Minecraft skin box texture layout:
// Given a box with pixel dimensions (w, h, d) and texture origin (ox, oy):
//
//   Column: ox   ox+d   ox+d+w   ox+2d+w   ox+2d+2w
//   Row oy:       [top w×d]  [bottom w×d]
//   Row oy+d: [left d×h] [front w×h] [right d×h] [back w×h]
//
BodyPartTexture SkinParser::extractBodyPart(const Image& img, int ox, int oy, int w, int h, int d) {
    BodyPartTexture part;
    part.top    = img.extractRegion(ox + d,         oy,     w, d);
    part.bottom = img.extractRegion(ox + d + w,     oy,     w, d);
    part.left   = img.extractRegion(ox,             oy + d, d, h);
    part.front  = img.extractRegion(ox + d,         oy + d, w, h);
    part.right  = img.extractRegion(ox + d + w,     oy + d, d, h);
    part.back   = img.extractRegion(ox + 2 * d + w, oy + d, w, h);
    return part;
}

TextureRegion SkinParser::mirrorHorizontal(const TextureRegion& region) {
    TextureRegion mirrored(region.width, region.height);
    for (int y = 0; y < region.height; ++y) {
        for (int x = 0; x < region.width; ++x) {
            mirrored.pixels[y * region.width + x] =
                region.pixels[y * region.width + (region.width - 1 - x)];
        }
    }
    return mirrored;
}

BodyPartTexture SkinParser::mirrorBodyPart(const BodyPartTexture& part) {
    BodyPartTexture m;
    m.top    = mirrorHorizontal(part.top);
    m.bottom = mirrorHorizontal(part.bottom);
    m.front  = mirrorHorizontal(part.front);
    m.back   = mirrorHorizontal(part.back);
    // Swap left and right, then mirror each
    m.left   = mirrorHorizontal(part.right);
    m.right  = mirrorHorizontal(part.left);
    return m;
}

SkinData SkinParser::parseNew(const Image& img) {
    SkinData skin;
    skin.format = SkinData::NEW_64x64;

    // Head: 8×8×8 box
    // Inner at (0, 0), outer at (32, 0)
    skin.head      = extractBodyPart(img, 0,  0,  8, 8, 8);
    skin.headOuter = extractBodyPart(img, 32, 0,  8, 8, 8);

    // Body: 8×12×4 box (w=8, h=12, d=4)
    // Inner at (16, 16), outer at (16, 32)
    skin.body      = extractBodyPart(img, 16, 16, 8, 12, 4);
    skin.bodyOuter = extractBodyPart(img, 16, 32, 8, 12, 4);

    // Right arm: 4×12×4 box
    // Inner at (40, 16), outer at (40, 32)
    skin.rightArm      = extractBodyPart(img, 40, 16, 4, 12, 4);
    skin.rightArmOuter = extractBodyPart(img, 40, 32, 4, 12, 4);

    // Left arm: 4×12×4 box
    // Inner at (32, 48), outer at (48, 48)
    skin.leftArm      = extractBodyPart(img, 32, 48, 4, 12, 4);
    skin.leftArmOuter = extractBodyPart(img, 48, 48, 4, 12, 4);

    // Right leg: 4×12×4 box
    // Inner at (0, 16), outer at (0, 32)
    skin.rightLeg      = extractBodyPart(img, 0,  16, 4, 12, 4);
    skin.rightLegOuter = extractBodyPart(img, 0,  32, 4, 12, 4);

    // Left leg: 4×12×4 box
    // Inner at (16, 48), outer at (0, 48)
    skin.leftLeg      = extractBodyPart(img, 16, 48, 4, 12, 4);
    skin.leftLegOuter = extractBodyPart(img, 0,  48, 4, 12, 4);

    return skin;
}

SkinData SkinParser::parseOld(const Image& img) {
    SkinData skin;
    skin.format = SkinData::OLD_64x32;

    // Head inner at (0, 0), head outer at (32, 0) — old format has head outer too
    skin.head      = extractBodyPart(img, 0,  0,  8, 8, 8);
    skin.headOuter = extractBodyPart(img, 32, 0,  8, 8, 8);

    // Body inner at (16, 16) — no outer in old format
    skin.body = extractBodyPart(img, 16, 16, 8, 12, 4);

    // Right arm inner at (40, 16)
    skin.rightArm = extractBodyPart(img, 40, 16, 4, 12, 4);

    // Right leg inner at (0, 16)
    skin.rightLeg = extractBodyPart(img, 0, 16, 4, 12, 4);

    // Left arm = mirror of right arm
    skin.leftArm = mirrorBodyPart(skin.rightArm);

    // Left leg = mirror of right leg
    skin.leftLeg = mirrorBodyPart(skin.rightLeg);

    // Outer layers (except head) are empty default-constructed BodyPartTexture
    // bodyOuter, rightArmOuter, leftArmOuter, rightLegOuter, leftLegOuter
    // remain default (empty TextureRegions)

    return skin;
}

Result<SkinData, std::string> SkinParser::parse(const std::string& filePath) {
    // Load the image
    auto imgOpt = Image::load(filePath);
    if (!imgOpt.has_value()) {
        return Result<SkinData, std::string>::err(
            "Failed to load file: " + filePath + " (not a valid PNG or file not found)");
    }

    const Image& img = imgOpt.value();

    // Validate dimensions
    if (img.width == 64 && img.height == 64) {
        return Result<SkinData, std::string>::ok(parseNew(img));
    } else if (img.width == 64 && img.height == 32) {
        return Result<SkinData, std::string>::ok(parseOld(img));
    } else {
        return Result<SkinData, std::string>::err(
            "Invalid skin dimensions: " + std::to_string(img.width) + "x" +
            std::to_string(img.height) + " (expected 64x64 or 64x32)");
    }
}
