#pragma once

#include "vec3.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() = default;
    Ray(const Vec3& origin, const Vec3& direction)
        : origin(origin), direction(direction) {}

    // Point along the ray at parameter t: origin + t * direction
    Vec3 at(float t) const { return origin + direction * t; }
};
