#pragma once

#include "math/vec3.h"
#include "math/color.h"
#include "scene/triangle.h"
#include "scene/scene.h"

// Shading parameters with sensible defaults
struct ShadingParams {
    float kd = 0.7f;         // Diffuse coefficient
    float ks = 0.3f;         // Specular coefficient
    float ambient = 0.1f;    // Ambient light coefficient
    float shininess = 32.0f; // Specular exponent
};

// Check if a point is in shadow from a light source.
// Casts a shadow ray from the point toward the light; returns true if
// any mesh blocks the path (intersection with t < distance to light).
// The origin is offset slightly along the normal to avoid shadow acne.
bool isInShadow(const Vec3& point, const Vec3& normal, const Vec3& lightPos, const Scene& scene);

// Compute Blinn-Phong shading at a hit point.
//
// color = ambient * textureColor
//       + kd * max(0, dot(N, L)) * textureColor * lightColor
//       + ks * pow(max(0, dot(N, H)), shininess) * lightColor
//
// If the point is in shadow, only the ambient term is applied.
Color shade(const HitResult& hit, const Vec3& viewDir, const Light& light,
            const Scene& scene, const ShadingParams& params = ShadingParams{});
