#include <gtest/gtest.h>
#include "skin/image.h"
#include "skin/texture_region.h"
#include <cstdio>

// ── TextureRegion Tests ─────────────────────────────────────────────────────

TEST(TextureRegion, DefaultConstruct) {
    TextureRegion tr;
    EXPECT_EQ(tr.width, 0);
    EXPECT_EQ(tr.height, 0);
    EXPECT_TRUE(tr.pixels.empty());
}

TEST(TextureRegion, SizedConstruct) {
    TextureRegion tr(4, 3);
    EXPECT_EQ(tr.width, 4);
    EXPECT_EQ(tr.height, 3);
    EXPECT_EQ(static_cast<int>(tr.pixels.size()), 12);
}

TEST(TextureRegion, SampleCenter) {
    // 2x2 texture with known colors
    TextureRegion tr(2, 2, {
        Color(1, 0, 0, 1), Color(0, 1, 0, 1),  // row 0
        Color(0, 0, 1, 1), Color(1, 1, 0, 1),   // row 1
    });
    // u=0.25, v=0.25 → x=0, y=0 → red
    Color c = tr.sample(0.25f, 0.25f);
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);

    // u=0.75, v=0.25 → x=1, y=0 → green
    c = tr.sample(0.75f, 0.25f);
    EXPECT_FLOAT_EQ(c.r, 0.0f);
    EXPECT_FLOAT_EQ(c.g, 1.0f);

    // u=0.25, v=0.75 → x=0, y=1 → blue
    c = tr.sample(0.25f, 0.75f);
    EXPECT_FLOAT_EQ(c.b, 1.0f);

    // u=0.75, v=0.75 → x=1, y=1 → yellow
    c = tr.sample(0.75f, 0.75f);
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 1.0f);
    EXPECT_FLOAT_EQ(c.b, 0.0f);
}

TEST(TextureRegion, SampleClampsOutOfRange) {
    TextureRegion tr(2, 2, {
        Color(1, 0, 0, 1), Color(0, 1, 0, 1),
        Color(0, 0, 1, 1), Color(1, 1, 0, 1),
    });
    // Negative UV should clamp to 0,0
    Color c = tr.sample(-1.0f, -1.0f);
    EXPECT_FLOAT_EQ(c.r, 1.0f);  // top-left red

    // UV > 1 should clamp to max
    c = tr.sample(5.0f, 5.0f);
    EXPECT_FLOAT_EQ(c.r, 1.0f);  // bottom-right yellow
    EXPECT_FLOAT_EQ(c.g, 1.0f);
}

TEST(TextureRegion, SampleEmptyReturnsDefault) {
    TextureRegion tr;
    Color c = tr.sample(0.5f, 0.5f);
    EXPECT_FLOAT_EQ(c.r, 0.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);
    EXPECT_FLOAT_EQ(c.b, 0.0f);
}

TEST(TextureRegion, SampleSinglePixel) {
    TextureRegion tr(1, 1, {Color(0.5f, 0.6f, 0.7f, 0.8f)});
    // Any UV should return the single pixel
    Color c = tr.sample(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(c.r, 0.5f);
    c = tr.sample(0.99f, 0.99f);
    EXPECT_FLOAT_EQ(c.r, 0.5f);
}

// ── Image Tests ─────────────────────────────────────────────────────────────

TEST(Image, DefaultConstruct) {
    Image img;
    EXPECT_EQ(img.width, 0);
    EXPECT_EQ(img.height, 0);
    EXPECT_TRUE(img.pixels.empty());
}

TEST(Image, SizedConstruct) {
    Image img(64, 64);
    EXPECT_EQ(img.width, 64);
    EXPECT_EQ(img.height, 64);
    EXPECT_EQ(static_cast<int>(img.pixels.size()), 64 * 64);
}

TEST(Image, ExtractRegion) {
    Image img(4, 4);
    // Fill with a pattern: pixel value = (x*0.1, y*0.1, 0, 1)
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            img.pixels[y * 4 + x] = Color(x * 0.1f, y * 0.1f, 0.0f, 1.0f);
        }
    }

    // Extract 2x2 region starting at (1, 1)
    TextureRegion region = img.extractRegion(1, 1, 2, 2);
    EXPECT_EQ(region.width, 2);
    EXPECT_EQ(region.height, 2);
    EXPECT_FLOAT_EQ(region.pixels[0].r, 0.1f);  // x=1
    EXPECT_FLOAT_EQ(region.pixels[0].g, 0.1f);  // y=1
    EXPECT_FLOAT_EQ(region.pixels[3].r, 0.2f);  // x=2
    EXPECT_FLOAT_EQ(region.pixels[3].g, 0.2f);  // y=2
}

TEST(Image, LoadNonexistentReturnsNullopt) {
    auto result = Image::load("nonexistent_file_12345.png");
    EXPECT_FALSE(result.has_value());
}

TEST(Image, SaveAndReload) {
    // Create a small image, save it, reload it, verify pixels
    Image img(2, 2);
    img.pixels[0] = Color(1.0f, 0.0f, 0.0f, 1.0f);  // red
    img.pixels[1] = Color(0.0f, 1.0f, 0.0f, 1.0f);  // green
    img.pixels[2] = Color(0.0f, 0.0f, 1.0f, 1.0f);  // blue
    img.pixels[3] = Color(1.0f, 1.0f, 1.0f, 1.0f);  // white

    const std::string tmpPath = "/tmp/test_image_save_reload.png";
    img.savePNG(tmpPath);

    auto loaded = Image::load(tmpPath);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->width, 2);
    EXPECT_EQ(loaded->height, 2);

    // uint8 round-trip: 255/255 = 1.0, 0/255 = 0.0 — exact for these values
    EXPECT_NEAR(loaded->pixels[0].r, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[0].g, 0.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[1].g, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[2].b, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[3].r, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[3].g, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(loaded->pixels[3].b, 1.0f, 1.0f / 255.0f);

    std::remove(tmpPath.c_str());
}
