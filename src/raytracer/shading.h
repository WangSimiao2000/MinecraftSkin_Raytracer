#pragma once

#include "math/vec3.h"
#include "math/color.h"
#include "scene/triangle.h"
#include "scene/scene.h"

// Shading parameters with sensible defaults
struct ShadingParams {
    float kd = 0.75f;        // Diffuse coefficient
    float ks = 0.15f;        // Specular coefficient
    float ambient = 0.20f;   // Ambient light coefficient
    float shininess = 16.0f; // Specular exponent (lower = softer highlight)
};

// Check if a point is in shadow from a light source.
// Casts a shadow ray from the point toward the light; returns true if
// any mesh blocks the path (intersection with t < distance to light).
// The origin is offset slightly along the normal to avoid shadow acne.
bool isInShadow(const Vec3& point, const Vec3& normal, const Vec3& lightPos, const Scene& scene);

// Compute soft shadow factor by sampling an area light.
// Returns a value in [0, 1] where 0 = fully shadowed, 1 = fully lit.
float computeSoftShadow(const Vec3& point, const Vec3& normal, const Light& light,
                        const Scene& scene, int samples, unsigned int seed);

// Compute Blinn-Phong shading at a hit point.
//
// color = ambient * textureColor
//       + kd * max(0, dot(N, L)) * textureColor * lightColor
//       + ks * pow(max(0, dot(N, H)), shininess) * lightColor
//
// If soft shadows are enabled, the diffuse/specular terms are scaled
// by the shadow visibility factor.
Color shade(const HitResult& hit, const Vec3& viewDir, const Light& light,
            const Scene& scene, const ShadingParams& params = ShadingParams{},
            float shadowFactor = -1.0f);
