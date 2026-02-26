#pragma once

#include "math/ray.h"
#include "math/vec3.h"
#include "scene/triangle.h"
#include "scene/mesh.h"
#include "scene/scene.h"

// Intersect a ray with a single mesh (treated as an AABB box).
// Uses the slab method for fast ray-box intersection.
// After intersection, determines the hit face, computes UV coordinates,
// and samples the texture. If the sampled pixel has alpha == 0,
// the hit is treated as a miss (transparent pixel pass-through).
HitResult intersectMesh(const Ray& ray, const Mesh& mesh);

// Intersect a ray with the entire scene, finding the closest
// non-transparent hit across all meshes.
HitResult intersectScene(const Ray& ray, const Scene& scene);
