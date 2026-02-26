/**
 * Property-based tests for image output resolution consistency.
 *
 * **Validates: Requirements 8.5**
 *
 * Property 13: Output Resolution Consistency (输出分辨率一致性)
 * For any user-configured output resolution (width W, height H),
 * the rendered output image dimensions should be exactly W × H pixels.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <stb/stb_image.h>

#include <filesystem>
#include <cstdio>

#include "raytracer/tile_renderer.h"
#include "output/image_writer.h"
#include "scene/scene.h"
#include "math/vec3.h"
#include "math/color.h"

// ── Property 13: Output Resolution Consistency ──────────────────────────────
//
// **Validates: Requirements 8.5**
//
// For any user-configured output resolution (width W in [8, 512],
// height H in [8, 512]), the rendered output image should have
// width == W, height == H, and pixels.size() == W * H.

RC_GTEST_PROP(ImageWriterProps, OutputResolutionConsistency, ()) {
    const int W = *rc::gen::inRange(8, 513);
    const int H = *rc::gen::inRange(8, 513);

    // Build a minimal scene: empty meshes, just camera + light + background
    Scene scene;
    scene.backgroundColor = Color(0.1f, 0.1f, 0.2f);
    scene.light = Light{Vec3(0, 50, 50), Color(1, 1, 1), 1.0f};
    scene.camera = Camera{Vec3(0, 18, 40), Vec3(0, 18, 0), Vec3(0, 1, 0), 60.0f};

    RayTracer::Config config;
    config.width = W;
    config.height = H;
    config.maxBounces = 0;
    config.tileSize = 16;
    config.threadCount = 1;

    Image result = TileRenderer::render(scene, config);

    RC_ASSERT(result.width == W);
    RC_ASSERT(result.height == H);
    RC_ASSERT(static_cast<int>(result.pixels.size()) == W * H);
}


// ── Property 14: Render Output Valid PNG ────────────────────────────────────
//
// **Validates: Requirements 8.2**
//
// For any successfully completed render, the output file should be a valid
// PNG file readable by a standard PNG decoder, and the image dimensions
// should match the render configuration.

RC_GTEST_PROP(ImageWriterProps, RenderOutputValidPNG, ()) {
    const int W = *rc::gen::inRange(16, 129);
    const int H = *rc::gen::inRange(16, 129);

    // Build a minimal scene
    Scene scene;
    scene.backgroundColor = Color(0.1f, 0.1f, 0.2f);
    scene.light = Light{Vec3(0, 50, 50), Color(1, 1, 1), 1.0f};
    scene.camera = Camera{Vec3(0, 18, 40), Vec3(0, 18, 0), Vec3(0, 1, 0), 60.0f};

    RayTracer::Config config;
    config.width = W;
    config.height = H;
    config.maxBounces = 0;
    config.tileSize = 16;
    config.threadCount = 1;

    // Render
    Image result = TileRenderer::render(scene, config);

    // Write to a temp PNG file
    namespace fs = std::filesystem;
    std::string tmpPath = (fs::temp_directory_path() / "rc_prop14_valid_png.png").string();

    bool writeOk = ImageWriter::writePNG(result, tmpPath);
    RC_ASSERT(writeOk);

    // Read back with stbi_load – a standard PNG decoder
    int readW = 0, readH = 0, channels = 0;
    unsigned char* data = stbi_load(tmpPath.c_str(), &readW, &readH, &channels, 4);

    // Valid PNG: stbi_load returns non-null
    RC_ASSERT(data != nullptr);

    // Dimensions match the render config
    RC_ASSERT(readW == W);
    RC_ASSERT(readH == H);

    stbi_image_free(data);
    std::remove(tmpPath.c_str());
}
