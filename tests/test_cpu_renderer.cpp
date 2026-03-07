#include <gtest/gtest.h>
#include "renderer/cpu_renderer.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/mesh_builder.h"
#include <memory>
#include <vector>
#include <mutex>

// Helper: build a minimal scene with at least one mesh
static Scene makeSimpleScene() {
    // buildDefaultScene creates a full character with white textures
    return MeshBuilder::buildDefaultScene();
}

// ─── CpuRenderer through IRenderer interface ────────────────────────────────

TEST(CpuRenderer, RenderProducesCorrectDimensions) {
    Scene scene = makeSimpleScene();

    RenderConfig config;
    config.width = 32;
    config.height = 32;
    config.samplesPerPixel = 1;
    config.maxBounces = 0;

    // Use IRenderer pointer to verify polymorphic dispatch
    std::unique_ptr<IRenderer> renderer = std::make_unique<CpuRenderer>();

    RenderResult result = renderer->render(scene, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.image.width, 32);
    EXPECT_EQ(result.image.height, 32);
    EXPECT_EQ(static_cast<int>(result.image.pixels.size()), 32 * 32);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST(CpuRenderer, ProgressCallbackCalledWithValidValues) {
    Scene scene = makeSimpleScene();

    RenderConfig config;
    config.width = 32;
    config.height = 32;
    config.samplesPerPixel = 1;
    config.maxBounces = 0;

    std::unique_ptr<IRenderer> renderer = std::make_unique<CpuRenderer>();

    std::mutex mu;
    std::vector<std::pair<int, int>> progressCalls;

    RenderResult result = renderer->render(scene, config,
        [&](int completed, int total) {
            std::lock_guard<std::mutex> lock(mu);
            progressCalls.emplace_back(completed, total);
        });

    EXPECT_TRUE(result.success);

    // Progress callback should have been called at least once
    EXPECT_FALSE(progressCalls.empty());

    // Every call must satisfy: completed <= total and both > 0
    for (const auto& [completed, total] : progressCalls) {
        EXPECT_GT(total, 0);
        EXPECT_GE(completed, 1);
        EXPECT_LE(completed, total);
    }
}
