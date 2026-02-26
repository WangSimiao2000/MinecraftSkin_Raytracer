#include <gtest/gtest.h>
#include "skin/skin_parser.h"
#include <cstdio>
#include <fstream>

// Helper: create a test skin image with identifiable pixel patterns.
// Each pixel at (x,y) gets a unique color based on coordinates.
static Image makeTestImage(int w, int h) {
    Image img(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            img.pixels[y * w + x] = Color(
                x / 63.0f,
                y / 63.0f,
                (x + y) / 126.0f,
                1.0f
            );
        }
    }
    return img;
}

// Helper: save an Image to a temp PNG and return the path
static std::string saveTempImage(const Image& img, const std::string& name) {
    std::string path = "/tmp/" + name + ".png";
    img.savePNG(path);
    return path;
}

// Helper: verify a TextureRegion matches the expected region from the source image
static void verifyRegion(const TextureRegion& region, const Image& img,
                         int ox, int oy, int w, int h) {
    ASSERT_EQ(region.width, w);
    ASSERT_EQ(region.height, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color& actual = region.pixels[y * w + x];
            const Color& expected = img.pixels[(oy + y) * img.width + (ox + x)];
            EXPECT_NEAR(actual.r, expected.r, 2.0f / 255.0f)
                << "Mismatch at region (" << x << "," << y << ") "
                << "source (" << ox + x << "," << oy + y << ")";
            EXPECT_NEAR(actual.g, expected.g, 2.0f / 255.0f);
            EXPECT_NEAR(actual.b, expected.b, 2.0f / 255.0f);
        }
    }
}

// ── Format Detection ────────────────────────────────────────────────────────

TEST(SkinParser, Parse64x64Format) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_skin_64x64");

    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value->format, SkinData::NEW_64x64);

    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x32Format) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_skin_64x32");

    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value->format, SkinData::OLD_64x32);

    std::remove(path.c_str());
}

// ── Invalid File Handling ───────────────────────────────────────────────────

TEST(SkinParser, RejectNonexistentFile) {
    auto result = SkinParser::parse("/tmp/nonexistent_skin_file_xyz.png");
    ASSERT_FALSE(result.isOk());
    EXPECT_TRUE(result.error.has_value());
}

TEST(SkinParser, RejectWrongDimensions) {
    Image img(32, 32);
    std::string path = saveTempImage(img, "test_skin_wrong_size");

    auto result = SkinParser::parse(path);
    ASSERT_FALSE(result.isOk());
    EXPECT_NE(result.error->find("32x32"), std::string::npos);

    std::remove(path.c_str());
}

TEST(SkinParser, RejectNonPngFile) {
    std::string path = "/tmp/test_not_a_png.png";
    std::ofstream f(path, std::ios::binary);
    f << "this is not a png file";
    f.close();

    auto result = SkinParser::parse(path);
    ASSERT_FALSE(result.isOk());

    std::remove(path.c_str());
}

// ── 64x64 Region Extraction ─────────────────────────────────────────────────

// Helper to verify a body part's 6 faces match the expected box layout
// Box layout at origin (ox, oy) with dimensions (w, h, d):
//   top:    (ox+d, oy, w, d)
//   bottom: (ox+d+w, oy, w, d)
//   left:   (ox, oy+d, d, h)
//   front:  (ox+d, oy+d, w, h)
//   right:  (ox+d+w, oy+d, d, h)
//   back:   (ox+2d+w, oy+d, w, h)
static void verifyBodyPart(const BodyPartTexture& part, const Image& img,
                           int ox, int oy, int w, int h, int d) {
    verifyRegion(part.top,    img, ox + d,         oy,     w, d);
    verifyRegion(part.bottom, img, ox + d + w,     oy,     w, d);
    verifyRegion(part.left,   img, ox,             oy + d, d, h);
    verifyRegion(part.front,  img, ox + d,         oy + d, w, h);
    verifyRegion(part.right,  img, ox + d + w,     oy + d, d, h);
    verifyRegion(part.back,   img, ox + 2 * d + w, oy + d, w, h);
}

TEST(SkinParser, Parse64x64HeadInner) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_64x64_head");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Head inner: origin (0,0), box 8x8x8
    verifyBodyPart(result.value->head, img, 0, 0, 8, 8, 8);
    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x64HeadOuter) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_64x64_head_outer");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Head outer: origin (32,0), box 8x8x8
    verifyBodyPart(result.value->headOuter, img, 32, 0, 8, 8, 8);
    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x64Body) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_64x64_body");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Body inner: origin (16,16), box 8x12x4
    verifyBodyPart(result.value->body, img, 16, 16, 8, 12, 4);
    // Body outer: origin (16,32), box 8x12x4
    verifyBodyPart(result.value->bodyOuter, img, 16, 32, 8, 12, 4);
    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x64Arms) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_64x64_arms");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Right arm inner: origin (40,16), box 4x12x4
    verifyBodyPart(result.value->rightArm, img, 40, 16, 4, 12, 4);
    // Right arm outer: origin (40,32), box 4x12x4
    verifyBodyPart(result.value->rightArmOuter, img, 40, 32, 4, 12, 4);
    // Left arm inner: origin (32,48), box 4x12x4
    verifyBodyPart(result.value->leftArm, img, 32, 48, 4, 12, 4);
    // Left arm outer: origin (48,48), box 4x12x4
    verifyBodyPart(result.value->leftArmOuter, img, 48, 48, 4, 12, 4);
    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x64Legs) {
    Image img = makeTestImage(64, 64);
    std::string path = saveTempImage(img, "test_64x64_legs");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Right leg inner: origin (0,16), box 4x12x4
    verifyBodyPart(result.value->rightLeg, img, 0, 16, 4, 12, 4);
    // Right leg outer: origin (0,32), box 4x12x4
    verifyBodyPart(result.value->rightLegOuter, img, 0, 32, 4, 12, 4);
    // Left leg inner: origin (16,48), box 4x12x4
    verifyBodyPart(result.value->leftLeg, img, 16, 48, 4, 12, 4);
    // Left leg outer: origin (0,48), box 4x12x4
    verifyBodyPart(result.value->leftLegOuter, img, 0, 48, 4, 12, 4);
    std::remove(path.c_str());
}

// ── 64x32 Old Format ────────────────────────────────────────────────────────

TEST(SkinParser, Parse64x32HeadAndBody) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_64x32_head_body");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Head inner: origin (0,0), box 8x8x8
    verifyBodyPart(result.value->head, img, 0, 0, 8, 8, 8);
    // Head outer: origin (32,0), box 8x8x8
    verifyBodyPart(result.value->headOuter, img, 32, 0, 8, 8, 8);
    // Body inner: origin (16,16), box 8x12x4
    verifyBodyPart(result.value->body, img, 16, 16, 8, 12, 4);
    std::remove(path.c_str());
}

TEST(SkinParser, Parse64x32RightArmDirect) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_64x32_rarm");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Right arm inner: origin (40,16), box 4x12x4
    verifyBodyPart(result.value->rightArm, img, 40, 16, 4, 12, 4);
    std::remove(path.c_str());
}

// ── Mirror Horizontal ───────────────────────────────────────────────────────

TEST(SkinParser, MirrorHorizontal) {
    TextureRegion region(3, 2, {
        Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1),
        Color(1, 1, 0, 1), Color(0, 1, 1, 1), Color(1, 0, 1, 1),
    });

    TextureRegion mirrored = SkinParser::mirrorHorizontal(region);
    ASSERT_EQ(mirrored.width, 3);
    ASSERT_EQ(mirrored.height, 2);

    // Row 0: blue, green, red
    EXPECT_FLOAT_EQ(mirrored.pixels[0].b, 1.0f);  // was at [2]
    EXPECT_FLOAT_EQ(mirrored.pixels[1].g, 1.0f);  // was at [1]
    EXPECT_FLOAT_EQ(mirrored.pixels[2].r, 1.0f);  // was at [0]

    // Row 1: magenta, cyan, yellow
    EXPECT_FLOAT_EQ(mirrored.pixels[3].r, 1.0f);
    EXPECT_FLOAT_EQ(mirrored.pixels[3].b, 1.0f);  // magenta
    EXPECT_FLOAT_EQ(mirrored.pixels[4].g, 1.0f);
    EXPECT_FLOAT_EQ(mirrored.pixels[4].b, 1.0f);  // cyan
    EXPECT_FLOAT_EQ(mirrored.pixels[5].r, 1.0f);
    EXPECT_FLOAT_EQ(mirrored.pixels[5].g, 1.0f);  // yellow
}

TEST(SkinParser, Parse64x32LeftArmMirrorsRightArm) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_64x32_mirror_arm");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    const auto& rArm = result.value->rightArm;
    const auto& lArm = result.value->leftArm;

    // Left arm front should be horizontally mirrored right arm front
    TextureRegion expectedFront = SkinParser::mirrorHorizontal(rArm.front);
    ASSERT_EQ(lArm.front.width, expectedFront.width);
    ASSERT_EQ(lArm.front.height, expectedFront.height);
    for (int i = 0; i < (int)expectedFront.pixels.size(); ++i) {
        EXPECT_NEAR(lArm.front.pixels[i].r, expectedFront.pixels[i].r, 2.0f / 255.0f);
        EXPECT_NEAR(lArm.front.pixels[i].g, expectedFront.pixels[i].g, 2.0f / 255.0f);
        EXPECT_NEAR(lArm.front.pixels[i].b, expectedFront.pixels[i].b, 2.0f / 255.0f);
    }

    // Left arm left should be mirrored right arm right (swap + mirror)
    TextureRegion expectedLeft = SkinParser::mirrorHorizontal(rArm.right);
    ASSERT_EQ(lArm.left.width, expectedLeft.width);
    for (int i = 0; i < (int)expectedLeft.pixels.size(); ++i) {
        EXPECT_NEAR(lArm.left.pixels[i].r, expectedLeft.pixels[i].r, 2.0f / 255.0f);
        EXPECT_NEAR(lArm.left.pixels[i].g, expectedLeft.pixels[i].g, 2.0f / 255.0f);
    }
}

TEST(SkinParser, Parse64x32LeftLegMirrorsRightLeg) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_64x32_mirror_leg");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    const auto& rLeg = result.value->rightLeg;
    const auto& lLeg = result.value->leftLeg;

    // Left leg front should be horizontally mirrored right leg front
    TextureRegion expectedFront = SkinParser::mirrorHorizontal(rLeg.front);
    ASSERT_EQ(lLeg.front.width, expectedFront.width);
    ASSERT_EQ(lLeg.front.height, expectedFront.height);
    for (int i = 0; i < (int)expectedFront.pixels.size(); ++i) {
        EXPECT_NEAR(lLeg.front.pixels[i].r, expectedFront.pixels[i].r, 2.0f / 255.0f);
        EXPECT_NEAR(lLeg.front.pixels[i].g, expectedFront.pixels[i].g, 2.0f / 255.0f);
        EXPECT_NEAR(lLeg.front.pixels[i].b, expectedFront.pixels[i].b, 2.0f / 255.0f);
    }
}

TEST(SkinParser, Parse64x32OuterLayersEmpty) {
    Image img = makeTestImage(64, 32);
    std::string path = saveTempImage(img, "test_64x32_outer_empty");
    auto result = SkinParser::parse(path);
    ASSERT_TRUE(result.isOk());

    // Body outer should be empty (default constructed)
    EXPECT_EQ(result.value->bodyOuter.front.width, 0);
    EXPECT_EQ(result.value->rightArmOuter.front.width, 0);
    EXPECT_EQ(result.value->leftArmOuter.front.width, 0);
    EXPECT_EQ(result.value->rightLegOuter.front.width, 0);
    EXPECT_EQ(result.value->leftLegOuter.front.width, 0);

    // But head outer should exist
    EXPECT_GT(result.value->headOuter.front.width, 0);

    std::remove(path.c_str());
}
