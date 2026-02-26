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
    };

    // Trace a single ray, returning the color.
    // depth starts at 0 and increments with each reflection bounce.
    static Color traceRay(const Ray& ray, const Scene& scene,
                          int depth, int maxBounces,
                          const ShadingParams& params = ShadingParams{});
};
