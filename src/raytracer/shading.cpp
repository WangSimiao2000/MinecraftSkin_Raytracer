#include "raytracer/shading.h"
#include "raytracer/intersection.h"
#include <cmath>
#include <algorithm>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Small offset to avoid shadow acne (self-intersection)
static constexpr float SHADOW_EPSILON = 1e-3f;

bool isInShadow(const Vec3& point, const Vec3& normal, const Vec3& lightPos, const Scene& scene) {
    Vec3 origin = point + normal * SHADOW_EPSILON;
    Vec3 toLight = lightPos - origin;
    float distToLight = toLight.length();

    if (distToLight < 1e-6f) return false;

    Vec3 dir = toLight / distToLight;
    Ray shadowRay(origin, dir);
    HitResult hit = intersectScene(shadowRay, scene);

    return hit.hit && hit.t < distToLight;
}

float computeSoftShadow(const Vec3& point, const Vec3& normal, const Light& light,
                        const Scene& scene, int samples, unsigned int seed) {
    if (samples <= 1 || light.radius < 1e-4f) {
        return isInShadow(point, normal, light.position, scene) ? 0.0f : 1.0f;
    }

    // Build a local frame at the light position for disk sampling
    Vec3 toPoint = (point - light.position).normalize();
    Vec3 tangent;
    if (std::fabs(toPoint.x) < 0.9f)
        tangent = Vec3(1, 0, 0).cross(toPoint).normalize();
    else
        tangent = Vec3(0, 1, 0).cross(toPoint).normalize();
    Vec3 bitangent = toPoint.cross(tangent);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int lit = 0;
    for (int i = 0; i < samples; ++i) {
        // Stratified disk sampling
        float angle = 2.0f * static_cast<float>(M_PI) * dist(rng);
        float r = light.radius * std::sqrt(dist(rng));
        Vec3 offset = tangent * (r * std::cos(angle)) + bitangent * (r * std::sin(angle));
        Vec3 samplePos = light.position + offset;

        if (!isInShadow(point, normal, samplePos, scene)) {
            ++lit;
        }
    }

    return static_cast<float>(lit) / static_cast<float>(samples);
}

Color shade(const HitResult& hit, const Vec3& viewDir, const Light& light,
            const Scene& scene, const ShadingParams& params,
            float shadowFactor) {
    Color texColor = hit.textureColor;
    float originalAlpha = texColor.a;

    // Ambient component
    Color ambient = texColor * params.ambient;

    // Light direction: from hit point toward the light
    Vec3 L = (light.position - hit.point).normalize();
    Vec3 N = hit.normal.normalize();
    Vec3 V = viewDir.normalize();

    // Shadow visibility: 0 = fully shadowed, 1 = fully lit
    float visibility = shadowFactor;
    if (visibility < 0.0f) {
        // Fallback: hard shadow check
        visibility = isInShadow(hit.point, N, light.position, scene) ? 0.0f : 1.0f;
    }

    // Diffuse component
    float NdotL = std::max(0.0f, N.dot(L));
    Color diffuse = texColor * light.color * (params.kd * NdotL * visibility);

    // Blinn-Phong specular
    Vec3 H = (L + V).normalize();
    float NdotH = std::max(0.0f, N.dot(H));
    float specFactor = std::pow(NdotH, params.shininess);
    Color specular = light.color * (params.ks * specFactor * visibility);

    Color result = ambient + diffuse + specular;
    result.a = originalAlpha;
    return result.clamp();
}
