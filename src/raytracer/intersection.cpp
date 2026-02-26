#include "raytracer/intersection.h"
#include "skin/texture_region.h"
#include <cmath>
#include <limits>
#include <algorithm>

// Compute AABB bounds from a mesh's triangles
static void computeAABB(const Mesh& mesh, Vec3& boxMin, Vec3& boxMax) {
    boxMin = Vec3( std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max());
    boxMax = Vec3(-std::numeric_limits<float>::max(),
                  -std::numeric_limits<float>::max(),
                  -std::numeric_limits<float>::max());

    for (const auto& tri : mesh.triangles) {
        const Vec3* verts[] = { &tri.v0, &tri.v1, &tri.v2 };
        for (const Vec3* v : verts) {
            boxMin.x = std::min(boxMin.x, v->x);
            boxMin.y = std::min(boxMin.y, v->y);
            boxMin.z = std::min(boxMin.z, v->z);
            boxMax.x = std::max(boxMax.x, v->x);
            boxMax.y = std::max(boxMax.y, v->y);
            boxMax.z = std::max(boxMax.z, v->z);
        }
    }
}

// Identify which face of the box was hit based on the axis and sign
// of the tmin component, then find the corresponding texture from the mesh triangles.
// Face convention (matching mesh_builder.cpp):
//   face 0,1   = front  (-Z), normal (0,0,-1)
//   face 2,3   = back   (+Z), normal (0,0,1)
//   face 4,5   = left   (+X), normal (1,0,0)
//   face 6,7   = right  (-X), normal (-1,0,0)
//   face 8,9   = top    (+Y), normal (0,1,0)
//   face 10,11 = bottom (-Y), normal (0,-1,0)
//
// The "hit axis" is the axis whose slab produced tmin.
// The sign tells us which side of that axis was hit.

struct FaceInfo {
    Vec3 normal;
    const TextureRegion* texture;
    int faceIndex; // index into the 6 faces (0-5)
};

static FaceInfo determineFace(const Mesh& mesh, int hitAxis, bool hitNegSide) {
    FaceInfo info;
    // Map (axis, sign) to face pair index in the triangle array
    // hitAxis: 0=X, 1=Y, 2=Z
    // hitNegSide: true means ray hit the min side of that axis
    if (hitAxis == 2) {
        if (hitNegSide) {
            // Hit the -Z face = front face, triangles 0,1
            info.normal = Vec3(0, 0, -1);
            info.faceIndex = 0;
        } else {
            // Hit the +Z face = back face, triangles 2,3
            info.normal = Vec3(0, 0, 1);
            info.faceIndex = 1;
        }
    } else if (hitAxis == 0) {
        if (!hitNegSide) {
            // Hit the +X face = left face, triangles 4,5
            info.normal = Vec3(1, 0, 0);
            info.faceIndex = 2;
        } else {
            // Hit the -X face = right face, triangles 6,7
            info.normal = Vec3(-1, 0, 0);
            info.faceIndex = 3;
        }
    } else { // hitAxis == 1
        if (!hitNegSide) {
            // Hit the +Y face = top face, triangles 8,9
            info.normal = Vec3(0, 1, 0);
            info.faceIndex = 4;
        } else {
            // Hit the -Y face = bottom face, triangles 10,11
            info.normal = Vec3(0, -1, 0);
            info.faceIndex = 5;
        }
    }

    // Get texture from the first triangle of the face pair
    int triIndex = info.faceIndex * 2;
    if (triIndex < static_cast<int>(mesh.triangles.size())) {
        info.texture = mesh.triangles[triIndex].texture;
    } else {
        info.texture = nullptr;
    }

    return info;
}

// Compute UV coordinates on the hit face.
// The hit point is projected onto the face plane and normalized to [0,1].
static void computeFaceUV(const Vec3& hitPoint, const Vec3& boxMin, const Vec3& boxMax,
                          int hitAxis, bool hitNegSide, float& u, float& v) {
    Vec3 size = boxMax - boxMin;

    // Avoid division by zero
    float sx = (size.x > 1e-8f) ? size.x : 1.0f;
    float sy = (size.y > 1e-8f) ? size.y : 1.0f;
    float sz = (size.z > 1e-8f) ? size.z : 1.0f;

    if (hitAxis == 2) {
        // Front (-Z) or Back (+Z) face
        // U maps along X, V maps along Y (inverted: top=0, bottom=1)
        float localX = (hitPoint.x - boxMin.x) / sx;
        float localY = (hitPoint.y - boxMin.y) / sy;
        if (hitNegSide) {
            // Front face (-Z): looking from -Z, left=+X, right=-X
            // In Minecraft skin, front face U goes left-to-right as viewed
            // mesh_builder: front face vertices are v010(top-left), v110(top-right), v100(bot-right), v000(bot-left)
            // where v010 = (-hw, +hh, -hd), v110 = (+hw, +hh, -hd)
            // So U goes from -X to +X = localX, V goes from +Y to -Y = 1-localY
            u = localX;
            v = 1.0f - localY;
        } else {
            // Back face (+Z): looking from +Z, left=-X (from viewer's perspective)
            // mesh_builder: back face vertices are v111(top-left), v011(top-right), v001(bot-right), v101(bot-left)
            // v111 = (+hw, +hh, +hd), v011 = (-hw, +hh, +hd)
            // So U goes from +X to -X = 1-localX, V goes from +Y to -Y = 1-localY
            u = 1.0f - localX;
            v = 1.0f - localY;
        }
    } else if (hitAxis == 0) {
        // Left (+X) or Right (-X) face
        // U maps along Z, V maps along Y
        float localZ = (hitPoint.z - boxMin.z) / sz;
        float localY = (hitPoint.y - boxMin.y) / sy;
        if (!hitNegSide) {
            // Left face (+X): looking from +X
            // mesh_builder: left face vertices are v110(top-left), v111(top-right), v101(bot-right), v100(bot-left)
            // v110 = (+hw, +hh, -hd), v111 = (+hw, +hh, +hd)
            // So U goes from -Z to +Z = localZ, V goes from +Y to -Y = 1-localY
            u = localZ;
            v = 1.0f - localY;
        } else {
            // Right face (-X): looking from -X
            // mesh_builder: right face vertices are v011(top-left), v010(top-right), v000(bot-right), v001(bot-left)
            // v011 = (-hw, +hh, +hd), v010 = (-hw, +hh, -hd)
            // So U goes from +Z to -Z = 1-localZ, V goes from +Y to -Y = 1-localY
            u = 1.0f - localZ;
            v = 1.0f - localY;
        }
    } else { // hitAxis == 1
        // Top (+Y) or Bottom (-Y) face
        // U maps along X, V maps along Z
        float localX = (hitPoint.x - boxMin.x) / sx;
        float localZ = (hitPoint.z - boxMin.z) / sz;
        if (!hitNegSide) {
            // Top face (+Y): looking from +Y down
            // mesh_builder: top face vertices are v011(front-left), v111(front-right), v110(back-right), v010(back-left)
            // v011 = (-hw, +hh, +hd), v111 = (+hw, +hh, +hd)
            // So U goes from -X to +X = localX, V goes from +Z to -Z = 1-localZ
            u = localX;
            v = 1.0f - localZ;
        } else {
            // Bottom face (-Y): looking from -Y up
            // mesh_builder: bottom face vertices are v000(front-left), v100(front-right), v101(back-right), v001(back-left)
            // v000 = (-hw, -hh, -hd), v100 = (+hw, -hh, -hd)
            // So U goes from -X to +X = localX, V goes from -Z to +Z = localZ
            u = localX;
            v = localZ;
        }
    }

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
}


HitResult intersectMesh(const Ray& ray, const Mesh& mesh) {
    HitResult result;
    result.hit = false;

    if (mesh.triangles.empty()) return result;

    // Compute AABB from mesh triangles
    Vec3 boxMin, boxMax;
    computeAABB(mesh, boxMin, boxMax);

    // Slab method for ray-AABB intersection
    float tmin = -std::numeric_limits<float>::max();
    float tmax =  std::numeric_limits<float>::max();
    int hitAxis = 0;
    bool hitNegSide = false;

    const float dirArr[3] = { ray.direction.x, ray.direction.y, ray.direction.z };
    const float oriArr[3] = { ray.origin.x, ray.origin.y, ray.origin.z };
    const float minArr[3] = { boxMin.x, boxMin.y, boxMin.z };
    const float maxArr[3] = { boxMax.x, boxMax.y, boxMax.z };

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dirArr[i]) < 1e-8f) {
            // Ray is parallel to this slab
            if (oriArr[i] < minArr[i] || oriArr[i] > maxArr[i]) {
                return result; // No intersection
            }
            // Otherwise the ray is within the slab for this axis, tmin/tmax unchanged
        } else {
            float invD = 1.0f / dirArr[i];
            float t0 = (minArr[i] - oriArr[i]) * invD;
            float t1 = (maxArr[i] - oriArr[i]) * invD;

            bool enterNeg = true; // entering from the min side
            if (t0 > t1) {
                std::swap(t0, t1);
                enterNeg = false;
            }

            if (t0 > tmin) {
                tmin = t0;
                hitAxis = i;
                hitNegSide = enterNeg;
            }
            tmax = std::min(tmax, t1);

            if (tmin > tmax || tmax < 0.0f) {
                return result; // No intersection
            }
        }
    }

    // Determine the actual t value for the hit
    // If tmin < 0, the ray starts inside the box; use tmax instead
    float tHit = tmin;
    if (tHit < 0.0f) {
        tHit = tmax;
        if (tHit < 0.0f) {
            return result; // Box is entirely behind the ray
        }
        // When using tmax, we need to recalculate which face was hit
        // tmax corresponds to the exit face
        // Recalculate hitAxis and hitNegSide for the exit point
        tmin = -std::numeric_limits<float>::max();
        float tmaxRecalc = std::numeric_limits<float>::max();
        int exitAxis = 0;
        bool exitNegSide = false;

        for (int i = 0; i < 3; ++i) {
            if (std::fabs(dirArr[i]) < 1e-8f) continue;
            float invD = 1.0f / dirArr[i];
            float t0 = (minArr[i] - oriArr[i]) * invD;
            float t1 = (maxArr[i] - oriArr[i]) * invD;

            bool exitNeg = false; // exiting from the max side
            if (t0 > t1) {
                std::swap(t0, t1);
                exitNeg = true;
            }

            if (t1 < tmaxRecalc) {
                tmaxRecalc = t1;
                exitAxis = i;
                exitNegSide = exitNeg;
            }
        }
        hitAxis = exitAxis;
        hitNegSide = exitNegSide;
    }

    // Compute hit point
    Vec3 hitPoint = ray.at(tHit);

    // Determine which face was hit and get its texture
    FaceInfo face = determineFace(mesh, hitAxis, hitNegSide);

    // Compute UV coordinates on the hit face
    float u, v;
    computeFaceUV(hitPoint, boxMin, boxMax, hitAxis, hitNegSide, u, v);

    // Sample texture
    Color texColor;
    if (face.texture) {
        texColor = face.texture->sample(u, v);
    } else {
        texColor = Color(1, 0, 1, 1); // Magenta for missing texture (debug)
    }

    // Transparent pixel (alpha == 0) treated as miss
    if (texColor.a == 0.0f) {
        return result; // Miss - transparent pixel
    }

    result.hit = true;
    result.t = tHit;
    result.point = hitPoint;
    result.normal = face.normal;
    result.textureColor = texColor;
    result.isOuterLayer = mesh.isOuterLayer;

    return result;
}

HitResult intersectScene(const Ray& ray, const Scene& scene) {
    HitResult closest;
    closest.hit = false;
    closest.t = std::numeric_limits<float>::max();

    for (const auto& mesh : scene.meshes) {
        HitResult hit = intersectMesh(ray, mesh);
        if (hit.hit && hit.t < closest.t) {
            closest = hit;
        }
    }

    return closest;
}
