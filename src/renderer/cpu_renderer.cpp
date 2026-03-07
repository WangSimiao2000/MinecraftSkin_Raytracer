#include "renderer/cpu_renderer.h"
#include "raytracer/tile_renderer.h"

/// Map the unified RenderConfig to the existing RayTracer::Config.
static RayTracer::Config toRayTracerConfig(const RenderConfig& rc) {
    RayTracer::Config cfg;
    cfg.width           = rc.width;
    cfg.height          = rc.height;
    cfg.samplesPerPixel = rc.samplesPerPixel;
    cfg.maxBounces      = rc.maxBounces;

    // Soft shadows / area light
    cfg.softShadows   = rc.softShadows;
    cfg.shadowSamples = rc.shadowSamples;

    // Ambient occlusion
    cfg.aoEnabled   = rc.aoEnabled;
    cfg.aoSamples   = rc.aoSamples;
    cfg.aoRadius    = rc.aoRadius;
    cfg.aoIntensity = rc.aoIntensity;

    // Depth of field
    cfg.dofEnabled     = rc.dofEnabled;
    cfg.aperture       = rc.aperture;
    cfg.focusDistance   = rc.focusDistance;

    // Background gradient
    cfg.gradientBg    = rc.gradientBg;
    cfg.gradientScale = rc.gradientScale;
    cfg.bgCenter      = rc.bgCenter;
    cfg.bgEdge        = rc.bgEdge;

    // Keep tile/thread defaults from RayTracer::Config
    return cfg;
}

RenderResult CpuRenderer::render(const Scene& scene,
                                 const RenderConfig& config,
                                 ProgressCallback progress) {
    RayTracer::Config rtConfig = toRayTracerConfig(config);

    Image image = TileRenderer::render(scene, rtConfig, progress);

    RenderResult result;
    result.success = true;
    result.image   = std::move(image);
    return result;
}
