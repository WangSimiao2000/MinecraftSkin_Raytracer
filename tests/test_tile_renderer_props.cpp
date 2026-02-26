/**
 * Property-based tests for TileRenderer tile generation.
 *
 * **Validates: Requirements 4.1**
 *
 * Property 11: Tile Complete Coverage (图块完整覆盖)
 * For any output image dimensions (width W, height H) and tile size T,
 * the generated tile set should completely cover the entire image area
 * with no overlap and no gaps (the union of all tile pixel coordinates
 * equals exactly [0, W) × [0, H)).
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <vector>
#include <set>

#include "raytracer/tile_renderer.h"

// ── Property 11: Tile Complete Coverage ─────────────────────────────────────
//
// **Validates: Requirements 4.1**
//
// For any image width W in [1, 2048], height H in [1, 2048], and tile size T
// in [1, 256], generateTiles produces a set of tiles whose pixel coordinates
// union is exactly [0, W) × [0, H) with no overlap.

RC_GTEST_PROP(TileRendererProps, TileCompleteCoverage, ()) {
    int W = *rc::gen::inRange(1, 2049);
    int H = *rc::gen::inRange(1, 2049);
    int T = *rc::gen::inRange(1, 257);

    auto tiles = TileRenderer::generateTiles(W, H, T);

    // Total pixel count from all tiles must equal W * H (no overlap, no gap).
    long long totalPixels = 0;
    for (const auto& tile : tiles) {
        // Each tile must be within image bounds
        RC_ASSERT(tile.x >= 0);
        RC_ASSERT(tile.y >= 0);
        RC_ASSERT(tile.x + tile.width <= W);
        RC_ASSERT(tile.y + tile.height <= H);
        RC_ASSERT(tile.width > 0);
        RC_ASSERT(tile.height > 0);

        totalPixels += static_cast<long long>(tile.width) * tile.height;
    }

    RC_ASSERT(totalPixels == static_cast<long long>(W) * H);

    // Verify no overlap: tiles should form a non-overlapping grid.
    // Check that tiles partition the x-axis and y-axis correctly.
    // Collect unique tile origins and verify grid structure.
    std::set<std::pair<int, int>> origins;
    for (const auto& tile : tiles) {
        auto result = origins.insert({tile.x, tile.y});
        // Each origin must be unique (no duplicate tiles)
        RC_ASSERT(result.second);
    }

    // Verify tiles are aligned to a grid: each tile.x should be a multiple
    // of T (except possibly the last column), and similarly for tile.y.
    for (const auto& tile : tiles) {
        RC_ASSERT(tile.x % T == 0);
        RC_ASSERT(tile.y % T == 0);

        // Interior tiles have full tile size; edge tiles are clipped
        int expectedW = std::min(T, W - tile.x);
        int expectedH = std::min(T, H - tile.y);
        RC_ASSERT(tile.width == expectedW);
        RC_ASSERT(tile.height == expectedH);
    }

    // Verify expected tile count
    int expectedCols = (W + T - 1) / T;
    int expectedRows = (H + T - 1) / T;
    RC_ASSERT(static_cast<int>(tiles.size()) == expectedCols * expectedRows);
}

// ── Property 12: Multi-threaded Rendering Determinism ───────────────────────
//
// **Validates: Requirements 4.2**
//
// For any scene and render configuration, rendering with 1 thread should
// produce pixel-identical results compared to rendering with N threads.

RC_GTEST_PROP(TileRendererProps, MultithreadRenderDeterminism, ()) {
    // Generate random but small image dimensions (8-64)
    int W = *rc::gen::inRange(8, 65);
    int H = *rc::gen::inRange(8, 65);
    // Tile size (4-32)
    int T = *rc::gen::inRange(4, 33);
    // Thread count for multi-threaded run (2-8)
    int N = *rc::gen::inRange(2, 9);
    // Max bounces (0-2, keep small for speed)
    int bounces = *rc::gen::inRange(0, 3);

    // Build a simple scene (empty, just camera + light + background)
    Scene scene;
    scene.backgroundColor = Color(0.2f, 0.3f, 0.5f);
    scene.light = Light{Vec3(0, 50, 50), Color(1, 1, 1), 1.0f};
    scene.camera = Camera{Vec3(0, 18, 40), Vec3(0, 18, 0), Vec3(0, 1, 0), 60.0f};

    // Render with 1 thread
    RayTracer::Config config1;
    config1.width = W;
    config1.height = H;
    config1.maxBounces = bounces;
    config1.tileSize = T;
    config1.threadCount = 1;

    Image img1 = TileRenderer::render(scene, config1);

    // Render with N threads
    RayTracer::Config configN = config1;
    configN.threadCount = N;

    Image imgN = TileRenderer::render(scene, configN);

    // Both images must have the same dimensions
    RC_ASSERT(img1.width == imgN.width);
    RC_ASSERT(img1.height == imgN.height);
    RC_ASSERT(img1.pixels.size() == imgN.pixels.size());

    // Every pixel must be exactly equal
    for (size_t i = 0; i < img1.pixels.size(); ++i) {
        RC_ASSERT(img1.pixels[i].r == imgN.pixels[i].r);
        RC_ASSERT(img1.pixels[i].g == imgN.pixels[i].g);
        RC_ASSERT(img1.pixels[i].b == imgN.pixels[i].b);
        RC_ASSERT(img1.pixels[i].a == imgN.pixels[i].a);
    }
}

