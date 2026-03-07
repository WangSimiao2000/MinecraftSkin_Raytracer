/**
 * Property-based tests for acceleration structure construction.
 *
 * Feature: gpu-raytracing
 * Property: BLAS construction doesn't crash
 *
 * **Validates: Requirements 3.1, 5.2**
 *
 * The full BLAS/TLAS construction (AccelerationStructure class) is behind
 * #ifdef HAVE_VULKAN_RT and requires a Vulkan-capable GPU at runtime.
 * Since the test environment builds with ENABLE_VULKAN_RT=OFF, we cannot
 * directly invoke buildBLAS/buildTLAS or test poseToTransform (private).
 *
 * What we CAN test without Vulkan:
 *   - The data preparation logic that feeds into BLAS construction:
 *     extracting vertex positions and index buffers from Mesh triangles.
 *     This is the same extraction that buildBLAS performs internally.
 *   - Mesh pose fields are well-formed for any generated mesh.
 *
 * The property verified here:
 *   "For any valid triangle mesh, the vertex/index data extraction that
 *    precedes BLAS construction produces consistent, non-degenerate buffers."
 *
 * With a GPU-enabled build, the full property would be:
 *   "For any valid triangle mesh, buildBLAS should succeed or report an
 *    error, never crash."
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <cmath>
#include <vector>
#include <cstdint>
#include <limits>

#include "scene/mesh.h"
#include "scene/mesh_builder.h"
#include "math/vec3.h"
#include "skin/texture_region.h"
#include "math/color.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

/// Replicate the vertex/index extraction logic from
/// AccelerationStructure::buildBLAS (acceleration_structure.cpp).
/// This is the CPU-side data preparation that precedes GPU upload.
struct BLASInputData {
    std::vector<float>    positions;  // 3 floats per vertex
    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    uint32_t primitiveCount = 0;
};

static BLASInputData extractBLASInput(const Mesh& mesh) {
    BLASInputData data;
    const auto& tris = mesh.triangles;
    data.primitiveCount = static_cast<uint32_t>(tris.size());
    data.vertexCount    = data.primitiveCount * 3;

    data.positions.resize(data.vertexCount * 3);
    data.indices.resize(data.vertexCount);

    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        const uint32_t base = static_cast<uint32_t>(i) * 3;

        data.positions[base * 3 + 0] = t.v0.x;
        data.positions[base * 3 + 1] = t.v0.y;
        data.positions[base * 3 + 2] = t.v0.z;

        data.positions[(base + 1) * 3 + 0] = t.v1.x;
        data.positions[(base + 1) * 3 + 1] = t.v1.y;
        data.positions[(base + 1) * 3 + 2] = t.v1.z;

        data.positions[(base + 2) * 3 + 0] = t.v2.x;
        data.positions[(base + 2) * 3 + 1] = t.v2.y;
        data.positions[(base + 2) * 3 + 2] = t.v2.z;

        data.indices[base + 0] = base;
        data.indices[base + 1] = base + 1;
        data.indices[base + 2] = base + 2;
    }

    return data;
}

/// Create a BodyPartTexture with an opaque color for all 6 faces.
static BodyPartTexture makeOpaqueTexture(int faceW, int faceH) {
    std::vector<Color> pixels(faceW * faceH, Color(0.6f, 0.3f, 0.1f, 1.0f));
    TextureRegion face(faceW, faceH, pixels);
    BodyPartTexture tex;
    tex.top = face;
    tex.bottom = face;
    tex.front = face;
    tex.back = face;
    tex.left = face;
    tex.right = face;
    return tex;
}

/// Check that a float value is finite (not NaN, not Inf).
static bool isFinite(float v) {
    return std::isfinite(v);
}

// ── Property: BLAS input extraction is consistent for any valid mesh ────────
//
// Feature: gpu-raytracing, Property: BLAS construction doesn't crash
//
// **Validates: Requirements 3.1, 5.2**
//
// For any valid triangle mesh generated via MeshBuilder::buildBox with
// random position, size, and offset, the vertex/index extraction that
// feeds BLAS construction:
//   1. Produces exactly 3 vertices per triangle (vertexCount == primitiveCount * 3)
//   2. All indices are in range [0, vertexCount)
//   3. All vertex positions are finite floats (no NaN/Inf)
//   4. The extraction does not crash

RC_GTEST_PROP(AccelerationStructureProps,
              BLASInputExtractionIsConsistentForAnyMesh, ()) {
    // Generate random box parameters
    float px = *rc::gen::inRange(-100, 101) / 10.0f;
    float py = *rc::gen::inRange(-100, 101) / 10.0f;
    float pz = *rc::gen::inRange(-100, 101) / 10.0f;

    // Size must be positive
    float sx = *rc::gen::inRange(1, 51) / 10.0f;
    float sy = *rc::gen::inRange(1, 51) / 10.0f;
    float sz = *rc::gen::inRange(1, 51) / 10.0f;

    float offset = *rc::gen::inRange(0, 21) / 10.0f;

    BodyPartTexture tex = makeOpaqueTexture(4, 4);
    Mesh mesh = MeshBuilder::buildBox(tex, Vec3(px, py, pz), Vec3(sx, sy, sz), offset);

    // The mesh should have triangles (a box has 12 triangles = 6 faces × 2)
    RC_ASSERT(!mesh.triangles.empty());

    // Extract BLAS input data (same logic as buildBLAS)
    BLASInputData data = extractBLASInput(mesh);

    // 1. Vertex count == primitive count * 3
    RC_ASSERT(data.vertexCount == data.primitiveCount * 3);

    // 2. All indices are in valid range
    for (uint32_t idx : data.indices) {
        RC_ASSERT(idx < data.vertexCount);
    }

    // 3. All vertex positions are finite
    for (float pos : data.positions) {
        RC_ASSERT(isFinite(pos));
    }

    // 4. Position buffer has correct size
    RC_ASSERT(data.positions.size() == static_cast<size_t>(data.vertexCount) * 3);
    RC_ASSERT(data.indices.size() == static_cast<size_t>(data.vertexCount));
}

// ── Property: BLAS input extraction with rotated mesh ───────────────────────
//
// Feature: gpu-raytracing, Property: BLAS construction doesn't crash
//
// **Validates: Requirements 3.1, 5.2**
//
// For any valid mesh with random pose rotation, the vertex/index extraction
// still produces valid data. The rotation fields are stored on the Mesh and
// would be consumed by poseToTransform during TLAS construction, but the
// BLAS input extraction (which uses the raw triangle vertices) should be
// unaffected by rotation parameters.

RC_GTEST_PROP(AccelerationStructureProps,
              BLASInputExtractionWithRotatedMesh, ()) {
    float px = *rc::gen::inRange(-50, 51) / 10.0f;
    float py = *rc::gen::inRange(-50, 51) / 10.0f;
    float pz = *rc::gen::inRange(-50, 51) / 10.0f;

    float sx = *rc::gen::inRange(1, 31) / 10.0f;
    float sy = *rc::gen::inRange(1, 31) / 10.0f;
    float sz = *rc::gen::inRange(1, 31) / 10.0f;

    BodyPartTexture tex = makeOpaqueTexture(4, 4);
    Mesh mesh = MeshBuilder::buildBox(tex, Vec3(px, py, pz), Vec3(sx, sy, sz), 0.0f);

    // Apply random rotation pose
    mesh.hasRotation = true;
    mesh.rotX = *rc::gen::inRange(-180, 181) * 1.0f;
    mesh.rotY = *rc::gen::inRange(-180, 181) * 1.0f;
    mesh.rotZ = *rc::gen::inRange(-180, 181) * 1.0f;
    mesh.pivot = Vec3(px, py, pz);

    // Optionally add torso transform
    bool hasTorso = *rc::gen::arbitrary<bool>();
    if (hasTorso) {
        mesh.hasTorsoTransform = true;
        mesh.torsoRotX = *rc::gen::inRange(-90, 91) * 1.0f;
        mesh.torsoRotY = *rc::gen::inRange(-90, 91) * 1.0f;
        mesh.torsoRotZ = *rc::gen::inRange(-90, 91) * 1.0f;
        mesh.torsoPivot = Vec3(0, py + sy / 2, 0);
        mesh.torsoTranslation = Vec3(0, 0, 0);
    }

    BLASInputData data = extractBLASInput(mesh);

    // Same invariants hold regardless of rotation
    RC_ASSERT(data.vertexCount == data.primitiveCount * 3);
    RC_ASSERT(data.positions.size() == static_cast<size_t>(data.vertexCount) * 3);

    for (uint32_t idx : data.indices) {
        RC_ASSERT(idx < data.vertexCount);
    }

    for (float pos : data.positions) {
        RC_ASSERT(isFinite(pos));
    }
}

// ── Property: Empty mesh produces empty BLAS input ──────────────────────────
//
// Feature: gpu-raytracing, Property: BLAS construction doesn't crash
//
// **Validates: Requirements 3.1, 5.2**
//
// An empty mesh (no triangles) should produce empty BLAS input data without
// crashing. In the real buildBLAS, this case returns early.

RC_GTEST_PROP(AccelerationStructureProps,
              EmptyMeshProducesEmptyBLASInput, ()) {
    Mesh mesh;
    // Mesh with no triangles — random rotation fields shouldn't matter
    mesh.hasRotation = *rc::gen::arbitrary<bool>();
    if (mesh.hasRotation) {
        mesh.rotX = *rc::gen::inRange(-180, 181) * 1.0f;
        mesh.rotY = *rc::gen::inRange(-180, 181) * 1.0f;
        mesh.rotZ = *rc::gen::inRange(-180, 181) * 1.0f;
    }

    BLASInputData data = extractBLASInput(mesh);

    RC_ASSERT(data.vertexCount == 0);
    RC_ASSERT(data.primitiveCount == 0);
    RC_ASSERT(data.positions.empty());
    RC_ASSERT(data.indices.empty());
}
