#pragma once

#include "math/ray.h"
#include "math/color.h"
#include "scene/scene.h"
#include "raytracer/shading.h"

class RayTracer {
public:
    struct Config {
        int width = 256;
        int height = 256;
        int maxBounces = 3;
        int samplesPerPixel = 1;
        int tileSize = 32;
        int threadCount = 0; // 0 = auto

        // Soft shadows (area light)
        bool softShadows = true;
        int shadowSamples = 8;   // area light samples

        // Ambient occlusion
        bool aoEnabled = false;
        int aoSamples = 8;        // hemisphere samples per hit
        float aoRadius = 3.0f;    // max occlusion distance
        float aoIntensity = 0.5f; // strength of darkening

        // Depth of field
        bool dofEnabled = false;
        float aperture = 0.5f;    // lens radius (0 = pinhole)
        float focusDistance = 0.0f; // 0 = auto-focus on target

        // Background radial gradient (Morandi palette)
        bool gradientBg = true;
        float gradientScale = 1.0f;
        Color bgCenter{0.91f, 0.89f, 0.86f, 1.0f};  // warm white
        Color bgEdge{0.56f, 0.63f, 0.71f, 1.0f};     // muted blue
    };

    // Trace a single ray, returning the color.
    static Color traceRay(const Ray& ray, const Scene& scene,
                          int depth, int maxBounces,
                          const ShadingParams& params = ShadingParams{},
                          const Config* config = nullptr);

    // Compute background color for a ray (gradient or flat).
    static Color backgroundColor(const Scene& scene, float u, float v,
                                 const Config* config);

    // Compute ambient occlusion factor at a hit point.
    // Returns a value in [0, 1] where 0 = fully occluded, 1 = no occlusion.
    static float computeAO(const Vec3& point, const Vec3& normal,
                           const Scene& scene, int samples, float radius,
                           unsigned int seed);
};
