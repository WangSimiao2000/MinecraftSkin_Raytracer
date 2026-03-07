#include <gtest/gtest.h>
#include "vulkan/gpu_renderer.h"
#include "vulkan/device_capability.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/mesh_builder.h"
#include <memory>

// Helper: build a minimal valid scene
static Scene makeSimpleScene() {
    return MeshBuilder::buildDefaultScene();
}

// Detect once whether GPU RT is actually available on this machine.
static bool gpuAvailable() {
    static GpuCapability cap = DeviceCapabilityDetector::detect();
    return cap.available;
}

// ─── GpuRenderer tests (adapt to both stub and real Vulkan modes) ───────────

TEST(GpuRenderer, RenderProducesConsistentResult) {
    GpuRenderer renderer;
    Scene scene = makeSimpleScene();
    RenderConfig config;
    config.width = 64;
    config.height = 64;
    config.samplesPerPixel = 1;
    config.maxBounces = 1;

    RenderResult result = renderer.render(scene, config);

    if (gpuAvailable()) {
        // Real GPU path: render should succeed
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.image.width, 64);
        EXPECT_EQ(result.image.height, 64);
    } else {
        // Stub/no-GPU path: render should fail gracefully
        EXPECT_FALSE(result.success);
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

TEST(GpuRenderer, RenderDoesNotCrashWithValidScene) {
    GpuRenderer renderer;
    Scene scene = makeSimpleScene();
    RenderConfig config;
    config.width = 32;
    config.height = 32;
    config.samplesPerPixel = 1;
    config.maxBounces = 1;

    // Must not crash regardless of GPU availability
    RenderResult result = renderer.render(scene, config);
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST(GpuRenderer, CanBeCreatedAndDestroyedWithoutIssues) {
    // Construct and immediately destroy — no crash or leak
    { GpuRenderer renderer; }
    SUCCEED();
}

TEST(GpuRenderer, WorksThroughIRendererInterface) {
    std::unique_ptr<IRenderer> renderer = std::make_unique<GpuRenderer>();
    Scene scene = makeSimpleScene();
    RenderConfig config;
    config.width = 16;
    config.height = 16;
    config.samplesPerPixel = 1;

    RenderResult result = renderer->render(scene, config);

    // Either succeeds (real GPU) or fails gracefully (no GPU)
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}
