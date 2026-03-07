#pragma once

#include <functional>
#include <string>

#include "math/color.h"
#include "scene/scene.h"
#include "skin/image.h"

/// Rendering configuration parameters shared by all backends.
struct RenderConfig {
    int width = 1920;
    int height = 1080;
    int samplesPerPixel = 64;
    int maxBounces = 4;

    // Light source
    float lightRadius = 3.0f;

    // Soft shadows
    bool softShadows = true;
    int shadowSamples = 8;

    // Ambient occlusion
    bool aoEnabled = false;
    int aoSamples = 8;
    float aoRadius = 3.0f;
    float aoIntensity = 0.5f;

    // Depth of field
    bool dofEnabled = false;
    float aperture = 0.5f;
    float focusDistance = 0.0f;

    // Background gradient
    bool gradientBg = true;
    float gradientScale = 1.0f;
    Color bgCenter{0.91f, 0.89f, 0.86f, 1.0f};
    Color bgEdge{0.56f, 0.63f, 0.71f, 1.0f};
};

/// Result returned by any IRenderer::render() call.
struct RenderResult {
    bool success = false;
    Image image;
    std::string errorMessage;
};

/// Abstract rendering interface implemented by CPU and GPU backends.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    using ProgressCallback = std::function<void(int completed, int total)>;

    virtual RenderResult render(const Scene& scene,
                                const RenderConfig& config,
                                ProgressCallback progress = nullptr) = 0;
};
