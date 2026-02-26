#include "scene/scene.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Ray Camera::generateRay(float u, float v, float aspectRatio) const {
    // Compute camera basis vectors using look-at model
    Vec3 forward = (target - position).normalize();
    Vec3 right = forward.cross(up).normalize();
    Vec3 trueUp = right.cross(forward);

    // Compute half-extents of the image plane from FOV
    float halfH = std::tan(fov * 0.5f * static_cast<float>(M_PI) / 180.0f);
    float halfW = halfH * aspectRatio;

    // Map (u, v) from [0,1] to [-1,1], invert v so v=0 is top of image
    float su = (2.0f * u - 1.0f) * halfW;
    float sv = (2.0f * (1.0f - v) - 1.0f) * halfH;

    // Ray direction
    Vec3 dir = (forward + right * su + trueUp * sv).normalize();

    return Ray(position, dir);
}
