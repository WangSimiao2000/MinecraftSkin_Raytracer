#include "raytracer/raytracer.h"
#include "raytracer/intersection.h"
#include <cmath>
#include <algorithm>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr float SKIN_REFLECTIVITY = 0.1f;
static constexpr float REFLECT_EPSILON = 1e-3f;

// ── Background ──────────────────────────────────────────────────────────────

Color RayTracer::backgroundColor(const Scene& scene, float u, float v,
                                 const Config* config) {
    if (config && config->gradientBg) {
        // Radial gradient: center bright, edges dark
        float cx = u - 0.5f;
        float cy = v - 0.5f;
        float dist = std::sqrt(cx * cx + cy * cy) * 2.0f; // 0 at center, ~1.41 at corners
        dist = std::clamp(dist, 0.0f, 1.0f);
        // Smooth falloff
        float t = dist * dist;
        Color c;
        c.r = config->bgCenter.r * (1.0f - t) + config->bgEdge.r * t;
        c.g = config->bgCenter.g * (1.0f - t) + config->bgEdge.g * t;
        c.b = config->bgCenter.b * (1.0f - t) + config->bgEdge.b * t;
        c.a = 1.0f;
        return c;
    }
    return scene.backgroundColor;
}

// ── Ambient Occlusion ───────────────────────────────────────────────────────

float RayTracer::computeAO(const Vec3& point, const Vec3& normal,
                           const Scene& scene, int samples, float radius,
                           unsigned int seed) {
    // Build local coordinate frame from normal
    Vec3 N = normal.normalize();
    Vec3 T;
    if (std::fabs(N.x) < 0.9f)
        T = Vec3(1, 0, 0).cross(N).normalize();
    else
        T = Vec3(0, 1, 0).cross(N).normalize();
    Vec3 B = N.cross(T);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int occluded = 0;
    for (int i = 0; i < samples; ++i) {
        // Cosine-weighted hemisphere sampling
        float r1 = dist(rng);
        float r2 = dist(rng);
        float sinTheta = std::sqrt(1.0f - r1);
        float cosTheta = std::sqrt(r1);
        float phi = 2.0f * static_cast<float>(M_PI) * r2;

        Vec3 localDir(sinTheta * std::cos(phi),
                      cosTheta,
                      sinTheta * std::sin(phi));

        // Transform to world space
        Vec3 worldDir = T * localDir.x + N * localDir.y + B * localDir.z;
        worldDir = worldDir.normalize();

        Ray aoRay(point + N * 1e-3f, worldDir);
        HitResult hit = intersectScene(aoRay, scene);
        if (hit.hit && hit.t < radius) {
            ++occluded;
        }
    }

    return 1.0f - static_cast<float>(occluded) / static_cast<float>(samples);
}

// ── Ray Tracing ─────────────────────────────────────────────────────────────

Color RayTracer::traceRay(const Ray& ray, const Scene& scene,
                          int depth, int maxBounces,
                          const ShadingParams& params,
                          const Config* config) {
    if (depth > maxBounces) {
        // For background on bounced rays, use a neutral color
        return config ? backgroundColor(scene, 0.5f, 0.5f, config)
                      : scene.backgroundColor;
    }

    HitResult hit = intersectScene(ray, scene);

    if (!hit.hit) {
        // Primary rays get proper gradient; bounced rays get center color
        if (depth == 0 && config) {
            // u,v not available here — approximate from ray direction
            // This is a fallback; the tile renderer handles primary bg properly
            return backgroundColor(scene, 0.5f, 0.5f, config);
        }
        return scene.backgroundColor;
    }

    Vec3 viewDir = (ray.origin - hit.point).normalize();
    Color shadedColor = shade(hit, viewDir, scene.light, scene, params);
    float originalAlpha = shadedColor.a;

    // Ambient occlusion
    if (config && config->aoEnabled && depth == 0) {
        unsigned int aoSeed = static_cast<unsigned int>(
            hit.point.x * 73856093.0f + hit.point.y * 19349663.0f + hit.point.z * 83492791.0f);
        float ao = computeAO(hit.point, hit.normal, scene,
                             config->aoSamples, config->aoRadius, aoSeed);
        float aoFactor = 1.0f - config->aoIntensity * (1.0f - ao);
        shadedColor.r *= aoFactor;
        shadedColor.g *= aoFactor;
        shadedColor.b *= aoFactor;
    }

    // Reflection
    if (depth < maxBounces) {
        Vec3 N = hit.normal.normalize();
        Vec3 D = ray.direction.normalize();
        Vec3 reflectDir = D - N * (2.0f * D.dot(N));
        reflectDir = reflectDir.normalize();

        Vec3 reflectOrigin = hit.point + N * REFLECT_EPSILON;
        Ray reflectRay(reflectOrigin, reflectDir);

        Color reflectedColor = traceRay(reflectRay, scene, depth + 1, maxBounces, params, config);
        shadedColor = shadedColor * (1.0f - SKIN_REFLECTIVITY) + reflectedColor * SKIN_REFLECTIVITY;
    }

    shadedColor.a = originalAlpha;
    return shadedColor.clamp();
}
