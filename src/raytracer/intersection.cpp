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
// Face convention (matching mesh_builder.cpp addFace order):
//   face 0,1   = back   (-Z), normal (0,0,-1)
//   face 2,3   = front  (+Z), normal (0,0,1)
//   face 4,5   = left   (+X), normal (1,0,0)
//   face 6,7   = right  (-X), normal (-1,0,0)
//   face 8,9   = top    (+Y), normal (0,1,0)
//   face 10,11 = bottom (-Y), normal (0,-1,0)
//
// Character faces +Z (toward camera at default position).
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
            // Hit the -Z face = back face, triangles 0,1
            info.normal = Vec3(0, 0, -1);
            info.faceIndex = 0;
        } else {
            // Hit the +Z face = front face, triangles 2,3
            info.normal = Vec3(0, 0, 1);
            info.faceIndex = 1;
        }
    } else if (hitAxis == 0) {
        if (!hitNegSide) {
            // Hit the +X face = left face (character's left), triangles 4,5
            info.normal = Vec3(1, 0, 0);
            info.faceIndex = 2;
        } else {
            // Hit the -X face = right face (character's right), triangles 6,7
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
        // Back (-Z) or Front (+Z) face
        // U maps along X, V maps along Y (inverted: top=0, bottom=1)
        float localX = (hitPoint.x - boxMin.x) / sx;
        float localY = (hitPoint.y - boxMin.y) / sy;
        if (hitNegSide) {
            // Back face (-Z): looking from -Z toward +Z
            // U goes from +X to -X (character's left to right as seen from behind)
            u = 1.0f - localX;
            v = 1.0f - localY;
        } else {
            // Front face (+Z): looking from +Z toward -Z (camera default)
            // U goes from -X to +X (left to right as viewer sees it)
            u = localX;
            v = 1.0f - localY;
        }
    } else if (hitAxis == 0) {
        // Left (+X) or Right (-X) face
        // U maps along Z, V maps along Y
        float localZ = (hitPoint.z - boxMin.z) / sz;
        float localY = (hitPoint.y - boxMin.y) / sy;
        if (!hitNegSide) {
            // Left face (+X): character's left side, looking from +X
            // U=0 at front (+Z), U=1 at back (-Z)
            u = 1.0f - localZ;
            v = 1.0f - localY;
        } else {
            // Right face (-X): character's right side, looking from -X
            // U=1 at front (+Z), U=0 at back (-Z)
            u = localZ;
            v = 1.0f - localY;
        }
    } else { // hitAxis == 1
        // Top (+Y) or Bottom (-Y) face
        // U maps along X, V maps along Z
        float localX = (hitPoint.x - boxMin.x) / sx;
        float localZ = (hitPoint.z - boxMin.z) / sz;
        if (!hitNegSide) {
            // Top face (+Y): looking from +Y down
            // Front of character is +Z, V=1 at front
            u = localX;
            v = localZ;
        } else {
            // Bottom face (-Y): looking from -Y up
            u = localX;
            v = 1.0f - localZ;
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

    // Transparent pixel (alpha == 0) treated as miss.
    // For outer-layer meshes, fall through to the back face so that
    // the far side of the box is still visible (no backface culling).
    if (texColor.a == 0.0f) {
        if (!mesh.isOuterLayer) {
            return result; // inner layer: miss
        }

        // Outer layer: try the exit (back) face
        if (tmax > tHit) {
            // Recalculate exit face axis/side
            float tmaxRecalc2 = std::numeric_limits<float>::max();
            int exitAxis2 = 0;
            bool exitNegSide2 = false;

            for (int i = 0; i < 3; ++i) {
                if (std::fabs(dirArr[i]) < 1e-8f) continue;
                float invD = 1.0f / dirArr[i];
                float t0b = (minArr[i] - oriArr[i]) * invD;
                float t1b = (maxArr[i] - oriArr[i]) * invD;
                bool eNeg = false;
                if (t0b > t1b) { std::swap(t0b, t1b); eNeg = true; }
                if (t1b < tmaxRecalc2) {
                    tmaxRecalc2 = t1b;
                    exitAxis2 = i;
                    exitNegSide2 = eNeg;
                }
            }

            Vec3 backHitPoint = ray.at(tmax);
            FaceInfo backFace = determineFace(mesh, exitAxis2, exitNegSide2);
            float bu, bv;
            computeFaceUV(backHitPoint, boxMin, boxMax, exitAxis2, exitNegSide2, bu, bv);

            Color backTexColor;
            if (backFace.texture) {
                backTexColor = backFace.texture->sample(bu, bv);
            } else {
                backTexColor = Color(1, 0, 1, 1);
            }

            if (backTexColor.a > 0.0f) {
                result.hit = true;
                result.t = tmax;
                result.point = backHitPoint;
                // Flip normal to face the ray (back face)
                result.normal = backFace.normal * -1.0f;
                result.textureColor = backTexColor;
                result.isOuterLayer = true;
                return result;
            }
        }
        return result; // both faces transparent
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
