#include <gtest/gtest.h>
#include "output/image_writer.h"
#include <stb/stb_image.h>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

// Helper: create a small test image with known pixel values
static Image makeTestImage(int w, int h) {
    Image img(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float u = static_cast<float>(x) / (w - 1);
            float v = static_cast<float>(y) / (h - 1);
            img.pixels[y * w + x] = Color(u, v, 0.5f, 1.0f);
        }
    }
    return img;
}

class ImageWriterTest : public ::testing::Test {
protected:
    std::string tmpPath;

    void SetUp() override {
        tmpPath = fs::temp_directory_path() / "test_image_writer_output.png";
    }

    void TearDown() override {
        std::remove(tmpPath.c_str());
    }
};

TEST_F(ImageWriterTest, WritePNG_CreatesFile) {
    Image img = makeTestImage(4, 4);
    bool ok = ImageWriter::writePNG(img, tmpPath);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(fs::exists(tmpPath));
}

TEST_F(ImageWriterTest, WritePNG_ReadBackDimensions) {
    const int W = 8, H = 6;
    Image img = makeTestImage(W, H);
    ASSERT_TRUE(ImageWriter::writePNG(img, tmpPath));

    int w, h, channels;
    unsigned char* data = stbi_load(tmpPath.c_str(), &w, &h, &channels, 4);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(w, W);
    EXPECT_EQ(h, H);
    stbi_image_free(data);
}

TEST_F(ImageWriterTest, WritePNG_ReadBackPixelValues) {
    // Write a 2x2 image with known colors and verify round-trip
    Image img(2, 2);
    img.pixels[0] = Color(1.0f, 0.0f, 0.0f, 1.0f); // red
    img.pixels[1] = Color(0.0f, 1.0f, 0.0f, 1.0f); // green
    img.pixels[2] = Color(0.0f, 0.0f, 1.0f, 1.0f); // blue
    img.pixels[3] = Color(1.0f, 1.0f, 1.0f, 0.5f); // white semi-transparent

    ASSERT_TRUE(ImageWriter::writePNG(img, tmpPath));

    int w, h, channels;
    unsigned char* data = stbi_load(tmpPath.c_str(), &w, &h, &channels, 4);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(w, 2);
    ASSERT_EQ(h, 2);

    // Red pixel
    EXPECT_EQ(data[0], 255); EXPECT_EQ(data[1], 0); EXPECT_EQ(data[2], 0); EXPECT_EQ(data[3], 255);
    // Green pixel
    EXPECT_EQ(data[4], 0); EXPECT_EQ(data[5], 255); EXPECT_EQ(data[6], 0); EXPECT_EQ(data[7], 255);
    // Blue pixel
    EXPECT_EQ(data[8], 0); EXPECT_EQ(data[9], 0); EXPECT_EQ(data[10], 255); EXPECT_EQ(data[11], 255);
    // White semi-transparent: alpha = round(0.5 * 255 + 0.5) = 128
    EXPECT_EQ(data[12], 255); EXPECT_EQ(data[13], 255); EXPECT_EQ(data[14], 255); EXPECT_EQ(data[15], 128);

    stbi_image_free(data);
}

TEST_F(ImageWriterTest, WritePNG_InvalidPath) {
    Image img = makeTestImage(2, 2);
    // Writing to a non-existent deep directory should fail
    bool ok = ImageWriter::writePNG(img, "/nonexistent_dir_xyz/sub/output.png");
    EXPECT_FALSE(ok);
}

TEST_F(ImageWriterTest, WritePNG_EmptyPath) {
    Image img = makeTestImage(2, 2);
    bool ok = ImageWriter::writePNG(img, "");
    EXPECT_FALSE(ok);
}

TEST_F(ImageWriterTest, WritePNG_ZeroDimensions) {
    Image img;
    img.width = 0;
    img.height = 0;
    bool ok = ImageWriter::writePNG(img, tmpPath);
    EXPECT_FALSE(ok);
}

TEST_F(ImageWriterTest, WritePNG_ClampsOutOfRangeValues) {
    Image img(1, 1);
    img.pixels[0] = Color(2.0f, -0.5f, 1.5f, 1.0f);
    ASSERT_TRUE(ImageWriter::writePNG(img, tmpPath));

    int w, h, channels;
    unsigned char* data = stbi_load(tmpPath.c_str(), &w, &h, &channels, 4);
    ASSERT_NE(data, nullptr);
    // Values should be clamped: 2.0->255, -0.5->0, 1.5->255
    EXPECT_EQ(data[0], 255);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(data[2], 255);
    EXPECT_EQ(data[3], 255);
    stbi_image_free(data);
}
