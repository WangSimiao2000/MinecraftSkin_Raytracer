#pragma once

#include "renderer/renderer.h"

/// CPU rendering backend that wraps the existing TileRenderer.
class CpuRenderer : public IRenderer {
public:
    RenderResult render(const Scene& scene,
                        const RenderConfig& config,
                        ProgressCallback progress = nullptr) override;
};
