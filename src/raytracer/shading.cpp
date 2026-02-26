#include "raytracer/shading.h"
#include "raytracer/intersection.h"
#include <cmath>
#include <algorithm>

// Small offset to avoid shadow acne (self-intersection)
static constexpr float SHADOW_EPSILON = 1e-3f;

bool isInShadow(const Vec3& point, const Vec3& normal, const Vec3& lightPos, const Scene& scene) {
    // Offset origin slightly along the surface normal
    Vec3 origin = point + normal * SHADOW_EPSILON;

    Vec3 toLight = lightPos - origin;
    float distToLight = toLight.length();

    // Degenerate case: light is essentially at the surface point
    if (distToLight < 1e-6f) {
        return false;
    }

    Vec3 dir = toLight / distToLight; // normalize
    Ray shadowRay(origin, dir);

    HitResult hit = intersectScene(shadowRay, scene);

    // In shadow if something blocks the path before reaching the light
    return hit.hit && hit.t < distToLight;
}

Color shade(const HitResult& hit, const Vec3& viewDir, const Light& light,
            const Scene& scene, const ShadingParams& params) {
    Color texColor = hit.textureColor;
    float originalAlpha = texColor.a;

    // Ambient component
    Color ambient = texColor * params.ambient;

    // Light direction: from hit point toward the light
    Vec3 L = (light.position - hit.point).normalize();
    Vec3 N = hit.normal.normalize();
    Vec3 V = viewDir.normalize();

    // Shadow check — if in shadow, return ambient only
    if (isInShadow(hit.point, N, light.position, scene)) {
        Color result = ambient.clamp();
        result.a = originalAlpha;
        return result;
    }

    // Diffuse component: kd * max(0, dot(N, L)) * textureColor * lightColor
    float NdotL = std::max(0.0f, N.dot(L));
    Color diffuse = texColor * light.color * (params.kd * NdotL);

    // Half-angle vector for Blinn-Phong specular
    Vec3 H = (L + V).normalize();
    float NdotH = std::max(0.0f, N.dot(H));
    float specFactor = std::pow(NdotH, params.shininess);

    // Specular component: ks * pow(max(0, dot(N, H)), shininess) * lightColor
    Color specular = light.color * (params.ks * specFactor);

    Color result = ambient + diffuse + specular;
    result.a = originalAlpha; // Preserve texture alpha — lighting only affects RGB
    return result.clamp();
}
