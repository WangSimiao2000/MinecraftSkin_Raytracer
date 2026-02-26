#include "raytracer/raytracer.h"
#include "raytracer/intersection.h"
#include <cmath>
#include <algorithm>

// Small reflectivity for Minecraft skin materials
static constexpr float SKIN_REFLECTIVITY = 0.1f;
// Offset to avoid self-intersection when spawning reflection rays
static constexpr float REFLECT_EPSILON = 1e-3f;

Color RayTracer::traceRay(const Ray& ray, const Scene& scene,
                          int depth, int maxBounces,
                          const ShadingParams& params) {
    // Exceeded max bounces — return background
    if (depth > maxBounces) {
        return scene.backgroundColor;
    }

    // Intersect ray with scene
    HitResult hit = intersectScene(ray, scene);

    // No hit — return background
    if (!hit.hit) {
        return scene.backgroundColor;
    }

    // Compute view direction (from hit point toward ray origin)
    Vec3 viewDir = (ray.origin - hit.point).normalize();

    // Compute direct shading (Blinn-Phong + shadow)
    Color shadedColor = shade(hit, viewDir, scene.light, scene, params);
    float originalAlpha = shadedColor.a;

    // If we still have bounces left, compute reflection
    if (depth < maxBounces) {
        Vec3 N = hit.normal.normalize();
        Vec3 D = ray.direction.normalize();

        // Reflect direction: D - 2 * dot(D, N) * N
        Vec3 reflectDir = D - N * (2.0f * D.dot(N));
        reflectDir = reflectDir.normalize();

        // Offset origin along normal to avoid self-intersection
        Vec3 reflectOrigin = hit.point + N * REFLECT_EPSILON;
        Ray reflectRay(reflectOrigin, reflectDir);

        Color reflectedColor = traceRay(reflectRay, scene, depth + 1, maxBounces, params);

        // Blend: finalColor = shadedColor * (1 - reflectivity) + reflectedColor * reflectivity
        shadedColor = shadedColor * (1.0f - SKIN_REFLECTIVITY) + reflectedColor * SKIN_REFLECTIVITY;
    }

    // Preserve the texture alpha — reflection blending should not affect opacity
    shadedColor.a = originalAlpha;
    return shadedColor.clamp();
}
