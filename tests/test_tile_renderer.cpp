#include <gtest/gtest.h>
#include "raytracer/tile_renderer.h"
#include <thread>
#include <atomic>
#include <set>

// ── Tile generation tests ──────────────────────────────────────────────────

TEST(TileRenderer, GenerateTilesExactDivision) {
    // 64x64 image with tile size 32 → 4 tiles (2x2)
    auto tiles = TileRenderer::generateTiles(64, 64, 32);
    ASSERT_EQ(tiles.size(), 4u);

    EXPECT_EQ(tiles[0].x, 0);  EXPECT_EQ(tiles[0].y, 0);
    EXPECT_EQ(tiles[0].width, 32); EXPECT_EQ(tiles[0].height, 32);

    EXPECT_EQ(tiles[1].x, 32); EXPECT_EQ(tiles[1].y, 0);
    EXPECT_EQ(tiles[1].width, 32); EXPECT_EQ(tiles[1].height, 32);

    EXPECT_EQ(tiles[2].x, 0);  EXPECT_EQ(tiles[2].y, 32);
    EXPECT_EQ(tiles[3].x, 32); EXPECT_EQ(tiles[3].y, 32);
}

TEST(TileRenderer, GenerateTilesWithRemainder) {
    // 50x30 image with tile size 32 → 2 cols x 1 row = 2 tiles
    auto tiles = TileRenderer::generateTiles(50, 30, 32);
    ASSERT_EQ(tiles.size(), 2u);

    // First tile: full 32x30
    EXPECT_EQ(tiles[0].x, 0);
    EXPECT_EQ(tiles[0].y, 0);
    EXPECT_EQ(tiles[0].width, 32);
    EXPECT_EQ(tiles[0].height, 30);

    // Second tile: clipped to 18x30
    EXPECT_EQ(tiles[1].x, 32);
    EXPECT_EQ(tiles[1].y, 0);
    EXPECT_EQ(tiles[1].width, 18);
    EXPECT_EQ(tiles[1].height, 30);
}

TEST(TileRenderer, GenerateTilesSmallImage) {
    // Image smaller than tile size → single tile
    auto tiles = TileRenderer::generateTiles(10, 10, 32);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles[0].x, 0);
    EXPECT_EQ(tiles[0].y, 0);
    EXPECT_EQ(tiles[0].width, 10);
    EXPECT_EQ(tiles[0].height, 10);
}

TEST(TileRenderer, GenerateTilesInvalidInput) {
    EXPECT_TRUE(TileRenderer::generateTiles(0, 64, 32).empty());
    EXPECT_TRUE(TileRenderer::generateTiles(64, 0, 32).empty());
    EXPECT_TRUE(TileRenderer::generateTiles(64, 64, 0).empty());
    EXPECT_TRUE(TileRenderer::generateTiles(-1, 64, 32).empty());
}

// ── Parallel render tests ──────────────────────────────────────────────────

// Helper: build a minimal scene with one opaque box
static Scene makeSimpleScene() {
    Scene scene;
    scene.backgroundColor = Color(0.1f, 0.1f, 0.1f);
    scene.light = Light{Vec3(0, 50, 50), Color(1, 1, 1), 1.0f};
    scene.camera = Camera{Vec3(0, 18, 40), Vec3(0, 18, 0), Vec3(0, 1, 0), 60.0f};
    return scene;
}

TEST(TileRenderer, RenderProducesCorrectSize) {
    Scene scene = makeSimpleScene();
    RayTracer::Config config;
    config.width = 32;
    config.height = 32;
    config.maxBounces = 0;
    config.tileSize = 16;
    config.threadCount = 2;

    Image img = TileRenderer::render(scene, config);
    EXPECT_EQ(img.width, 32);
    EXPECT_EQ(img.height, 32);
    EXPECT_EQ(static_cast<int>(img.pixels.size()), 32 * 32);
}

TEST(TileRenderer, ProgressCallbackInvoked) {
    Scene scene = makeSimpleScene();
    RayTracer::Config config;
    config.width = 32;
    config.height = 32;
    config.maxBounces = 0;
    config.tileSize = 16;
    config.threadCount = 1;

    // 32/16 = 2 cols, 2 rows → 4 tiles
    std::atomic<int> callCount{0};
    int lastTotal = 0;
    TileRenderer::render(scene, config, [&](int done, int total) {
        callCount.fetch_add(1);
        lastTotal = total;
    });

    EXPECT_EQ(callCount.load(), 4);
    EXPECT_EQ(lastTotal, 4);
}

TEST(TileRenderer, DefaultThreadCountUsesHardwareConcurrency) {
    // Config with threadCount = 0 should use hardware_concurrency
    // We just verify render completes without error
    Scene scene = makeSimpleScene();
    RayTracer::Config config;
    config.width = 16;
    config.height = 16;
    config.maxBounces = 0;
    config.tileSize = 8;
    config.threadCount = 0; // auto

    Image img = TileRenderer::render(scene, config);
    EXPECT_EQ(img.width, 16);
    EXPECT_EQ(img.height, 16);
}

TEST(TileRenderer, SingleThreadMatchesMultiThread) {
    Scene scene = makeSimpleScene();

    RayTracer::Config config1;
    config1.width = 16;
    config1.height = 16;
    config1.maxBounces = 1;
    config1.tileSize = 8;
    config1.threadCount = 1;

    RayTracer::Config configN = config1;
    configN.threadCount = 4;

    Image img1 = TileRenderer::render(scene, config1);
    Image imgN = TileRenderer::render(scene, configN);

    ASSERT_EQ(img1.pixels.size(), imgN.pixels.size());
    for (size_t i = 0; i < img1.pixels.size(); ++i) {
        EXPECT_FLOAT_EQ(img1.pixels[i].r, imgN.pixels[i].r) << "pixel " << i;
        EXPECT_FLOAT_EQ(img1.pixels[i].g, imgN.pixels[i].g) << "pixel " << i;
        EXPECT_FLOAT_EQ(img1.pixels[i].b, imgN.pixels[i].b) << "pixel " << i;
        EXPECT_FLOAT_EQ(img1.pixels[i].a, imgN.pixels[i].a) << "pixel " << i;
    }
}

TEST(TileRenderer, NullProgressCallbackIsOk) {
    Scene scene = makeSimpleScene();
    RayTracer::Config config;
    config.width = 8;
    config.height = 8;
    config.maxBounces = 0;
    config.tileSize = 8;
    config.threadCount = 1;

    // Should not crash with nullptr callback
    Image img = TileRenderer::render(scene, config, nullptr);
    EXPECT_EQ(img.width, 8);
}
