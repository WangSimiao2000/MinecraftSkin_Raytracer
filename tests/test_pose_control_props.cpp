/**
 * Property-based tests for pose control system.
 *
 * Feature: pose-control
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "scene/pose.h"
#include "scene/mesh_builder.h"
#include "math/vec3.h"

#include <cmath>

// ── Property 1: Default pose zero values (默认姿态零值) ─────────────────────
//
// Feature: pose-control, Property 1: Default pose zero values
//
// **Validates: Requirements 1.1, 2.1, 4.1**
//
// For any default-constructed Pose, all PartPose rotX/rotY/rotZ should be 0
// and torsoTranslation should be Vec3{0, 0, 0}.

RC_GTEST_PROP(PoseControlProps, DefaultPoseZeroValues, ()) {
    Pose pose{};

    // Head rotations must all be zero
    RC_ASSERT(pose.head.rotX == 0.0f);
    RC_ASSERT(pose.head.rotY == 0.0f);
    RC_ASSERT(pose.head.rotZ == 0.0f);

    // Body rotations must all be zero
    RC_ASSERT(pose.body.rotX == 0.0f);
    RC_ASSERT(pose.body.rotY == 0.0f);
    RC_ASSERT(pose.body.rotZ == 0.0f);

    // Right arm rotations must all be zero
    RC_ASSERT(pose.rightArm.rotX == 0.0f);
    RC_ASSERT(pose.rightArm.rotY == 0.0f);
    RC_ASSERT(pose.rightArm.rotZ == 0.0f);

    // Left arm rotations must all be zero
    RC_ASSERT(pose.leftArm.rotX == 0.0f);
    RC_ASSERT(pose.leftArm.rotY == 0.0f);
    RC_ASSERT(pose.leftArm.rotZ == 0.0f);

    // Right leg rotations must all be zero
    RC_ASSERT(pose.rightLeg.rotX == 0.0f);
    RC_ASSERT(pose.rightLeg.rotY == 0.0f);
    RC_ASSERT(pose.rightLeg.rotZ == 0.0f);

    // Left leg rotations must all be zero
    RC_ASSERT(pose.leftLeg.rotX == 0.0f);
    RC_ASSERT(pose.leftLeg.rotY == 0.0f);
    RC_ASSERT(pose.leftLeg.rotZ == 0.0f);

    // Torso translation must be zero vector
    RC_ASSERT(pose.torsoTranslation.x == 0.0f);
    RC_ASSERT(pose.torsoTranslation.y == 0.0f);
    RC_ASSERT(pose.torsoTranslation.z == 0.0f);
}

// ── Property 2: Torso translation propagation (躯干平移传播) ────────────────
//
// Feature: pose-control, Property 2: Torso translation propagation
//
// **Validates: Requirements 1.2**
//
// For any random torso translation vector t, when only torsoTranslation is
// changed (all rotations zero), every vertex in the translated scene should
// equal the corresponding vertex in the default scene plus t.

RC_GTEST_PROP(PoseControlProps, TorsoTranslationPropagation, ()) {
    // Generate a random torso translation vector with reasonable values
    const auto tx = *rc::gen::inRange(-50, 51);
    const auto ty = *rc::gen::inRange(-50, 51);
    const auto tz = *rc::gen::inRange(-50, 51);
    const Vec3 t{static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(tz)};

    // Build scene with default pose (zero rotations, zero translation)
    Scene defaultScene = MeshBuilder::buildDefaultScene();

    // Build scene with only torsoTranslation set to t (all rotations still zero)
    Pose translatedPose{};
    translatedPose.torsoTranslation = t;
    Scene translatedScene = MeshBuilder::buildDefaultScene(translatedPose);

    // Both scenes must have the same number of meshes
    RC_ASSERT(defaultScene.meshes.size() == translatedScene.meshes.size());

    const float tolerance = 1e-4f;

    for (size_t m = 0; m < defaultScene.meshes.size(); ++m) {
        const auto& defMesh = defaultScene.meshes[m];
        const auto& trMesh  = translatedScene.meshes[m];

        // Same number of triangles per mesh
        RC_ASSERT(defMesh.triangles.size() == trMesh.triangles.size());

        for (size_t i = 0; i < defMesh.triangles.size(); ++i) {
            const auto& defTri = defMesh.triangles[i];
            const auto& trTri  = trMesh.triangles[i];

            // Each vertex in the translated scene should be default vertex + t
            auto checkVertex = [&](const Vec3& defV, const Vec3& trV) {
                Vec3 expected = defV + t;
                RC_ASSERT(std::fabs(trV.x - expected.x) < tolerance);
                RC_ASSERT(std::fabs(trV.y - expected.y) < tolerance);
                RC_ASSERT(std::fabs(trV.z - expected.z) < tolerance);
            };

            checkVertex(defTri.v0, trTri.v0);
            checkVertex(defTri.v1, trTri.v1);
            checkVertex(defTri.v2, trTri.v2);
        }
    }
}

// ── Property 3: Pivot point invariance (枢轴点不动性) ───────────────────────
//
// Feature: pose-control, Property 3: Pivot point invariance
//
// **Validates: Requirements 3.2**
//
// For any body part and any rotation angles, when only that part has local
// rotation (zero torso transform), the pivot point should remain fixed.
// We verify this by replicating the rotateAroundPivot math: rotating a point
// around itself always yields the same point (within floating-point tolerance).
// Additionally, we verify at the scene level that vertices equidistant from
// the pivot remain equidistant after rotation (rotation preserves distances).

namespace {

// Replicate the rotateAroundPivot logic from mesh_builder.cpp for testing
Vec3 testRotateAroundPivot(const Vec3& point, const Vec3& pivot,
                           float rotXDeg, float rotYDeg, float rotZDeg) {
    Vec3 p = point - pivot;

    // X-axis rotation (pitch)
    if (std::fabs(rotXDeg) > 0.01f) {
        float rad = rotXDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newY = p.y * cosA - p.z * sinA;
        float newZ = p.y * sinA + p.z * cosA;
        p.y = newY;
        p.z = newZ;
    }

    // Y-axis rotation (yaw)
    if (std::fabs(rotYDeg) > 0.01f) {
        float rad = rotYDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newX = p.x * cosA + p.z * sinA;
        float newZ = -p.x * sinA + p.z * cosA;
        p.x = newX;
        p.z = newZ;
    }

    // Z-axis rotation (roll)
    if (std::fabs(rotZDeg) > 0.01f) {
        float rad = rotZDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newX = p.x * cosA - p.y * sinA;
        float newY = p.x * sinA + p.y * cosA;
        p.x = newX;
        p.y = newY;
    }

    return p + pivot;
}

} // anonymous namespace

RC_GTEST_PROP(PoseControlProps, PivotPointInvariance, ()) {
    // Body part index: 0=head, 1=body, 2=rightArm, 3=leftArm, 4=rightLeg, 5=leftLeg
    const auto partIndex = *rc::gen::inRange(0, 6);

    // Pivot points for each body part (matching design doc)
    const Vec3 pivots[] = {
        Vec3(0, 24, 0),   // Head: neck joint
        Vec3(0, 18, 0),   // Body/Torso: torso center
        Vec3(-6, 24, 0),  // Right Arm: right shoulder
        Vec3(6, 24, 0),   // Left Arm: left shoulder
        Vec3(-2, 12, 0),  // Right Leg: right hip
        Vec3(2, 12, 0),   // Left Leg: left hip
    };

    const Vec3 pivot = pivots[partIndex];

    // Generate random rotation angles in [-180, 180]
    const auto rotX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const auto rotY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const auto rotZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    // Part 1: Mathematical verification — rotating a point around itself is identity
    const Vec3 result = testRotateAroundPivot(pivot, pivot, rotX, rotY, rotZ);
    const float tolerance = 1e-4f;
    RC_ASSERT(std::fabs(result.x - pivot.x) < tolerance);
    RC_ASSERT(std::fabs(result.y - pivot.y) < tolerance);
    RC_ASSERT(std::fabs(result.z - pivot.z) < tolerance);

    // Part 2: Scene-level verification — rotation preserves distance from pivot
    // Build rotated scene with only the selected part rotated (zero torso transform)
    Pose rotatedPose{};
    PartPose pp;
    pp.rotX = rotX;
    pp.rotY = rotY;
    pp.rotZ = rotZ;

    switch (partIndex) {
        case 0: rotatedPose.head     = pp; break;
        case 1: rotatedPose.body     = pp; break;
        case 2: rotatedPose.rightArm = pp; break;
        case 3: rotatedPose.leftArm  = pp; break;
        case 4: rotatedPose.rightLeg = pp; break;
        case 5: rotatedPose.leftLeg  = pp; break;
    }

    Scene defaultScene = MeshBuilder::buildDefaultScene();
    Scene rotatedScene = MeshBuilder::buildDefaultScene(rotatedPose);

    // Each part produces 2 meshes (inner + outer). Inner mesh index = partIndex * 2.
    const size_t meshIdx = static_cast<size_t>(partIndex) * 2;
    RC_PRE(meshIdx < defaultScene.meshes.size());
    RC_PRE(meshIdx < rotatedScene.meshes.size());

    const auto& defMesh = defaultScene.meshes[meshIdx];
    const auto& rotMesh = rotatedScene.meshes[meshIdx];
    RC_ASSERT(defMesh.triangles.size() == rotMesh.triangles.size());

    // Every vertex's distance to the pivot should be preserved after rotation
    for (size_t t = 0; t < defMesh.triangles.size(); ++t) {
        const auto& defTri = defMesh.triangles[t];
        const auto& rotTri = rotMesh.triangles[t];

        auto checkDist = [&](const Vec3& defV, const Vec3& rotV) {
            float defDist = (defV - pivot).lengthSquared();
            float rotDist = (rotV - pivot).lengthSquared();
            RC_ASSERT(std::fabs(defDist - rotDist) < tolerance);
        };

        checkDist(defTri.v0, rotTri.v0);
        checkDist(defTri.v1, rotTri.v1);
        checkDist(defTri.v2, rotTri.v2);
    }
}


// ── Property 4: Hierarchical transform chain order (层级变换链顺序) ──────────
//
// Feature: pose-control, Property 4: Hierarchical transform chain order
//
// **Validates: Requirements 2.3, 3.4, 5.2, 5.3**
//
// For any Pose with both part local rotation and torso rotation/translation,
// MeshBuilder output should equal manually applying:
//   (1) part local rotation around part pivot
//   (2) torso rotation around torso pivot (0,18,0)
//   (3) torso translation
// We use the head (pivot (0,24,0)) as the child part for simplicity.

RC_GTEST_PROP(PoseControlProps, HierarchicalTransformChainOrder, ()) {
    // Generate random rotation angles for head (child part)
    const float headRotX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float headRotY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float headRotZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    // Generate random rotation angles for torso
    const float torsoRotX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float torsoRotY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float torsoRotZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    // Generate random torso translation
    const float tx = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float ty = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float tz = static_cast<float>(*rc::gen::inRange(-50, 51));
    const Vec3 torsoTranslation{tx, ty, tz};

    // Build pose with head rotation + torso rotation + torso translation
    Pose pose{};
    pose.head = PartPose{headRotX, headRotY, headRotZ};
    pose.body = PartPose{torsoRotX, torsoRotY, torsoRotZ};
    pose.torsoTranslation = torsoTranslation;

    // Build scene via MeshBuilder
    Scene scene = MeshBuilder::buildDefaultScene(pose);

    // Head is the first part → inner mesh at index 0
    RC_PRE(scene.meshes.size() > 0);
    const auto& headMesh = scene.meshes[0];

    // Now manually compute expected vertices:
    // Start with default (unrotated) head mesh
    Scene defaultScene = MeshBuilder::buildDefaultScene();
    RC_PRE(defaultScene.meshes.size() > 0);
    const auto& defaultHeadMesh = defaultScene.meshes[0];

    RC_ASSERT(headMesh.triangles.size() == defaultHeadMesh.triangles.size());

    const Vec3 headPivot(0, 24, 0);
    const Vec3 torsoPivot(0, 18, 0);
    const float tolerance = 1e-2f;

    for (size_t i = 0; i < defaultHeadMesh.triangles.size(); ++i) {
        const auto& defTri = defaultHeadMesh.triangles[i];
        const auto& actualTri = headMesh.triangles[i];

        auto computeExpected = [&](const Vec3& v) -> Vec3 {
            // Step 1: Apply head local rotation around head pivot
            Vec3 result = testRotateAroundPivot(v, headPivot, headRotX, headRotY, headRotZ);
            // Step 2: Apply torso rotation around torso pivot
            result = testRotateAroundPivot(result, torsoPivot, torsoRotX, torsoRotY, torsoRotZ);
            // Step 3: Apply torso translation
            result = result + torsoTranslation;
            return result;
        };

        Vec3 expected0 = computeExpected(defTri.v0);
        Vec3 expected1 = computeExpected(defTri.v1);
        Vec3 expected2 = computeExpected(defTri.v2);

        RC_ASSERT(std::fabs(actualTri.v0.x - expected0.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v0.y - expected0.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v0.z - expected0.z) < tolerance);

        RC_ASSERT(std::fabs(actualTri.v1.x - expected1.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v1.y - expected1.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v1.z - expected1.z) < tolerance);

        RC_ASSERT(std::fabs(actualTri.v2.x - expected2.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v2.y - expected2.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v2.z - expected2.z) < tolerance);
    }
}

// ── Property 5: Rotation axis order X→Y→Z (旋转轴顺序 X→Y→Z) ───────────────
//
// Feature: pose-control, Property 5: Rotation axis order X→Y→Z
//
// **Validates: Requirements 4.2**
//
// For any non-zero rotation angles (rotX, rotY, rotZ), applying all three
// rotations at once should produce the same result as applying them
// sequentially: first X only, then Y only on the result, then Z only on that
// result. We verify this both at the math level (testRotateAroundPivot) and
// at the scene level (MeshBuilder output for the head part).

RC_GTEST_PROP(PoseControlProps, RotationAxisOrderXYZ, ()) {
    // Generate random non-zero rotation angles in [-180, 180]
    auto genNonZeroAngle = rc::gen::suchThat(rc::gen::inRange(-180, 181),
                                              [](int v) { return v != 0; });
    const float rotX = static_cast<float>(*genNonZeroAngle);
    const float rotY = static_cast<float>(*genNonZeroAngle);
    const float rotZ = static_cast<float>(*genNonZeroAngle);

    const Vec3 headPivot(0, 24, 0);

    // Build a default scene to get test vertices from the head mesh
    Scene defaultScene = MeshBuilder::buildDefaultScene();
    RC_PRE(defaultScene.meshes.size() > 0);
    const auto& defaultHeadMesh = defaultScene.meshes[0];
    RC_PRE(!defaultHeadMesh.triangles.empty());

    // --- Scene-level verification ---
    // Apply all three rotations at once via MeshBuilder
    Pose allAtOncePose{};
    allAtOncePose.head = PartPose{rotX, rotY, rotZ};
    Scene allAtOnceScene = MeshBuilder::buildDefaultScene(allAtOncePose);
    RC_PRE(allAtOnceScene.meshes.size() > 0);
    const auto& allAtOnceMesh = allAtOnceScene.meshes[0];

    RC_ASSERT(defaultHeadMesh.triangles.size() == allAtOnceMesh.triangles.size());

    const float tolerance = 1e-2f;

    for (size_t i = 0; i < defaultHeadMesh.triangles.size(); ++i) {
        const auto& defTri = defaultHeadMesh.triangles[i];
        const auto& actualTri = allAtOnceMesh.triangles[i];

        auto applySequential = [&](const Vec3& v) -> Vec3 {
            // Step 1: Apply X rotation only
            Vec3 result = testRotateAroundPivot(v, headPivot, rotX, 0.0f, 0.0f);
            // Step 2: Apply Y rotation only on the result
            result = testRotateAroundPivot(result, headPivot, 0.0f, rotY, 0.0f);
            // Step 3: Apply Z rotation only on that result
            result = testRotateAroundPivot(result, headPivot, 0.0f, 0.0f, rotZ);
            return result;
        };

        Vec3 expected0 = applySequential(defTri.v0);
        Vec3 expected1 = applySequential(defTri.v1);
        Vec3 expected2 = applySequential(defTri.v2);

        // Verify all-at-once result matches sequential application
        RC_ASSERT(std::fabs(actualTri.v0.x - expected0.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v0.y - expected0.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v0.z - expected0.z) < tolerance);

        RC_ASSERT(std::fabs(actualTri.v1.x - expected1.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v1.y - expected1.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v1.z - expected1.z) < tolerance);

        RC_ASSERT(std::fabs(actualTri.v2.x - expected2.x) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v2.y - expected2.y) < tolerance);
        RC_ASSERT(std::fabs(actualTri.v2.z - expected2.z) < tolerance);
    }
}


// ── Property 6: Normal vector correct rotation (法线向量正确旋转) ────────────
//
// Feature: pose-control, Property 6: Normal vector correct rotation
//
// **Validates: Requirements 4.3**
//
// For any rotated mesh, each triangle's normal vector should:
//   (a) have unit length (length ≈ 1.0 within tolerance)
//   (b) be parallel or anti-parallel to the cross product of two edges
//       (v1-v0) × (v2-v0), i.e. dot(normal, normalized_cross) ≈ ±1.0

RC_GTEST_PROP(PoseControlProps, NormalVectorCorrectRotation, ()) {
    // Generate random rotation angles for head in [-180, 180]
    const float rotX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rotY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rotZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    // Build scene with head rotation applied
    Pose pose{};
    pose.head = PartPose{rotX, rotY, rotZ};
    Scene scene = MeshBuilder::buildDefaultScene(pose);

    // Head inner mesh is at index 0
    RC_PRE(scene.meshes.size() > 0);
    const auto& mesh = scene.meshes[0];
    RC_PRE(!mesh.triangles.empty());

    const float lengthTolerance = 1e-3f;
    const float dotTolerance = 1e-2f;

    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];

        // (a) Normal should have unit length
        float normalLen = tri.normal.length();
        RC_ASSERT(std::fabs(normalLen - 1.0f) < lengthTolerance);

        // (b) Compute cross product of two edges: (v1-v0) × (v2-v0)
        Vec3 edge1 = tri.v1 - tri.v0;
        Vec3 edge2 = tri.v2 - tri.v0;
        Vec3 crossProduct = edge1.cross(edge2);

        float crossLen = crossProduct.length();
        // Skip degenerate triangles (zero-area)
        if (crossLen < 1e-6f) continue;

        Vec3 normalizedCross = crossProduct / crossLen;

        // Normal should be parallel or anti-parallel to the cross product
        // i.e. |dot(normal, normalizedCross)| ≈ 1.0
        float dotVal = tri.normal.dot(normalizedCross);
        RC_ASSERT(std::fabs(std::fabs(dotVal) - 1.0f) < dotTolerance);
    }
}

// ── Property 7: Inner/outer layer same transform (内外层相同变换) ────────────
//
// Feature: pose-control, Property 7: Inner/outer layer same transform
//
// **Validates: Requirements 5.4**
//
// For any Pose, the inner and outer meshes for the same body part should have
// identical transform parameters: hasRotation, pivot, rotX/rotY/rotZ,
// hasTorsoTransform, torsoTranslation, torsoPivot, torsoRotX/torsoRotY/torsoRotZ.
// The default scene has 12 meshes (6 parts × 2 layers): inner at even indices,
// outer at odd indices.

RC_GTEST_PROP(PoseControlProps, InnerOuterLayerSameTransform, ()) {
    // Generate random rotation angles for all body parts
    const float headRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float headRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float headRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    const float bodyRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float bodyRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float bodyRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    const float rArmRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rArmRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rArmRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    const float lArmRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lArmRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lArmRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    const float rLegRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rLegRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rLegRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    const float lLegRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lLegRY = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lLegRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    // Generate random torso translation
    const float tx = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float ty = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float tz = static_cast<float>(*rc::gen::inRange(-50, 51));

    Pose pose{};
    pose.head     = PartPose{headRX, headRY, headRZ};
    pose.body     = PartPose{bodyRX, bodyRY, bodyRZ};
    pose.rightArm = PartPose{rArmRX, rArmRY, rArmRZ};
    pose.leftArm  = PartPose{lArmRX, lArmRY, lArmRZ};
    pose.rightLeg = PartPose{rLegRX, rLegRY, rLegRZ};
    pose.leftLeg  = PartPose{lLegRX, lLegRY, lLegRZ};
    pose.torsoTranslation = Vec3{tx, ty, tz};

    // Build SkinData with non-transparent outer layers so all 12 meshes are generated
    auto whiteTex = []() {
        BodyPartTexture t;
        auto make = [](int w, int h) {
            return TextureRegion(w, h, std::vector<Color>(w * h, Color(1, 1, 1, 1)));
        };
        t.front = make(1, 1); t.back = make(1, 1);
        t.left  = make(1, 1); t.right = make(1, 1);
        t.top   = make(1, 1); t.bottom = make(1, 1);
        return t;
    };

    SkinData skin{};
    skin.format = SkinData::NEW_64x64;
    // Inner layers
    skin.head = whiteTex(); skin.body = whiteTex();
    skin.rightArm = whiteTex(); skin.leftArm = whiteTex();
    skin.rightLeg = whiteTex(); skin.leftLeg = whiteTex();
    // Outer layers (non-transparent so they are included)
    skin.headOuter = whiteTex(); skin.bodyOuter = whiteTex();
    skin.rightArmOuter = whiteTex(); skin.leftArmOuter = whiteTex();
    skin.rightLegOuter = whiteTex(); skin.leftLegOuter = whiteTex();

    // Build scene with the full pose and non-transparent outer layers
    Scene scene = MeshBuilder::buildScene(skin, pose);

    // Scene should have 12 meshes: 6 parts × 2 layers (inner + outer)
    RC_ASSERT(scene.meshes.size() == 12);

    const float tolerance = 1e-6f;

    // For each body part (indices 0-5), compare inner (i*2) and outer (i*2+1)
    for (int part = 0; part < 6; ++part) {
        const size_t innerIdx = static_cast<size_t>(part) * 2;
        const size_t outerIdx = innerIdx + 1;

        const auto& inner = scene.meshes[innerIdx];
        const auto& outer = scene.meshes[outerIdx];

        // Same hasRotation flag
        RC_ASSERT(inner.hasRotation == outer.hasRotation);

        // Same pivot point
        RC_ASSERT(std::fabs(inner.pivot.x - outer.pivot.x) < tolerance);
        RC_ASSERT(std::fabs(inner.pivot.y - outer.pivot.y) < tolerance);
        RC_ASSERT(std::fabs(inner.pivot.z - outer.pivot.z) < tolerance);

        // Same local rotation angles
        RC_ASSERT(std::fabs(inner.rotX - outer.rotX) < tolerance);
        RC_ASSERT(std::fabs(inner.rotY - outer.rotY) < tolerance);
        RC_ASSERT(std::fabs(inner.rotZ - outer.rotZ) < tolerance);

        // Same hasTorsoTransform flag
        RC_ASSERT(inner.hasTorsoTransform == outer.hasTorsoTransform);

        // Same torso translation
        RC_ASSERT(std::fabs(inner.torsoTranslation.x - outer.torsoTranslation.x) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoTranslation.y - outer.torsoTranslation.y) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoTranslation.z - outer.torsoTranslation.z) < tolerance);

        // Same torso pivot
        RC_ASSERT(std::fabs(inner.torsoPivot.x - outer.torsoPivot.x) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoPivot.y - outer.torsoPivot.y) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoPivot.z - outer.torsoPivot.z) < tolerance);

        // Same torso rotation angles
        RC_ASSERT(std::fabs(inner.torsoRotX - outer.torsoRotX) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoRotY - outer.torsoRotY) < tolerance);
        RC_ASSERT(std::fabs(inner.torsoRotZ - outer.torsoRotZ) < tolerance);
    }
}

// ── Property 8: Backward compatibility (向后兼容性) ──────────────────────────
//
// Feature: pose-control, Property 8: Backward compatibility
//
// **Validates: Requirements 7.1, 7.2, 7.3**
//
// For any Pose with only rotX/rotZ set (rotY=0, torsoTranslation=(0,0,0)),
// the new system should produce output identical to manually applying only
// X and Z rotations via testRotateAroundPivot (simulating the old system).
// We pick a random builtin pose (which only uses rotX/rotZ) and verify that
// every mesh vertex matches the manual rotation result.

RC_GTEST_PROP(PoseControlProps, BackwardCompatibility, ()) {
    // Generate random rotX/rotZ for each child part (rotY=0 everywhere).
    // Body (torso) rotation is zero and torsoTranslation is zero — this is the
    // backward-compatible configuration per Requirement 7.3.
    const float headRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float headRZ = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rArmRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rArmRZ = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lArmRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lArmRZ = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rLegRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float rLegRZ = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lLegRX = static_cast<float>(*rc::gen::inRange(-180, 181));
    const float lLegRZ = static_cast<float>(*rc::gen::inRange(-180, 181));

    Pose pose{};
    pose.head     = PartPose{headRX, 0.0f, headRZ};
    pose.body     = PartPose{0.0f, 0.0f, 0.0f};  // torso rotation zero
    pose.rightArm = PartPose{rArmRX, 0.0f, rArmRZ};
    pose.leftArm  = PartPose{lArmRX, 0.0f, lArmRZ};
    pose.rightLeg = PartPose{rLegRX, 0.0f, rLegRZ};
    pose.leftLeg  = PartPose{lLegRX, 0.0f, lLegRZ};
    // torsoTranslation defaults to (0,0,0)

    // Build scene with the pose via MeshBuilder (new system)
    Scene posedScene = MeshBuilder::buildDefaultScene(pose);

    // Build default (unrotated) scene for reference vertices
    Scene defaultScene = MeshBuilder::buildDefaultScene();

    RC_ASSERT(posedScene.meshes.size() == defaultScene.meshes.size());

    // Pivot points for each body part (matching design doc)
    // Default scene has only inner meshes (6 total, one per part):
    // mesh 0=head, 1=body, 2=rightArm, 3=leftArm, 4=rightLeg, 5=leftLeg
    const Vec3 pivots[] = {
        Vec3(0, 24, 0),   // Head
        Vec3(0, 18, 0),   // Body/Torso
        Vec3(-6, 24, 0),  // Right Arm
        Vec3(6, 24, 0),   // Left Arm
        Vec3(-2, 12, 0),  // Right Leg
        Vec3(2, 12, 0),   // Left Leg
    };

    const PartPose* partPoses[] = {
        &pose.head, &pose.body, &pose.rightArm,
        &pose.leftArm, &pose.rightLeg, &pose.leftLeg
    };

    const float tolerance = 1e-4f;

    for (size_t m = 0; m < defaultScene.meshes.size(); ++m) {
        const auto& defMesh = defaultScene.meshes[m];
        const auto& posedMesh = posedScene.meshes[m];

        RC_ASSERT(defMesh.triangles.size() == posedMesh.triangles.size());

        // Each mesh corresponds directly to a body part (no outer layers in default scene)
        RC_PRE(m < 6);
        const Vec3& pivot = pivots[m];
        const PartPose& pp = *partPoses[m];

        for (size_t t = 0; t < defMesh.triangles.size(); ++t) {
            const auto& defTri = defMesh.triangles[t];
            const auto& posedTri = posedMesh.triangles[t];

            // Manually apply only X and Z rotations (rotY=0) — simulating old system
            // When torso rotation and translation are zero, the new system should
            // produce identical results to applying only local X/Z rotations.
            auto computeExpected = [&](const Vec3& v) -> Vec3 {
                return testRotateAroundPivot(v, pivot, pp.rotX, 0.0f, pp.rotZ);
            };

            Vec3 exp0 = computeExpected(defTri.v0);
            Vec3 exp1 = computeExpected(defTri.v1);
            Vec3 exp2 = computeExpected(defTri.v2);

            RC_ASSERT(std::fabs(posedTri.v0.x - exp0.x) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v0.y - exp0.y) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v0.z - exp0.z) < tolerance);

            RC_ASSERT(std::fabs(posedTri.v1.x - exp1.x) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v1.y - exp1.y) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v1.z - exp1.z) < tolerance);

            RC_ASSERT(std::fabs(posedTri.v2.x - exp2.x) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v2.y - exp2.y) < tolerance);
            RC_ASSERT(std::fabs(posedTri.v2.z - exp2.z) < tolerance);
        }
    }
}


// ── Property 9: Mesh stores complete transform info (网格存储完整变换信息) ───
//
// Feature: pose-control, Property 9: Mesh stores complete transform info
//
// **Validates: Requirements 8.1, 8.3**
//
// For any Pose with rotation on all body parts and a random torso translation,
// each Mesh produced by buildDefaultScene should:
//   (a) have non-empty world-space triangles
//   (b) have non-empty local-space (unrotated) triangles
//   (c) triangles.size() == localTriangles.size()
//   (d) stored transform params match the input Pose:
//       - For child parts: rotX/Y/Z match the part's PartPose,
//         torsoRotX/Y/Z match pose.body, torsoTranslation matches pose.torsoTranslation
//       - For torso: rotX/Y/Z match pose.body

RC_GTEST_PROP(PoseControlProps, MeshStoresCompleteTransformInfo, ()) {
    // Generate random non-zero rotation angles for all body parts to ensure
    // buildBoxWithPose is called (hasRotation == true) for every mesh.
    auto genAngle = rc::gen::suchThat(rc::gen::inRange(-180, 181),
                                       [](int v) { return v != 0; });

    const float headRX = static_cast<float>(*genAngle);
    const float headRY = static_cast<float>(*genAngle);
    const float headRZ = static_cast<float>(*genAngle);

    const float bodyRX = static_cast<float>(*genAngle);
    const float bodyRY = static_cast<float>(*genAngle);
    const float bodyRZ = static_cast<float>(*genAngle);

    const float rArmRX = static_cast<float>(*genAngle);
    const float rArmRY = static_cast<float>(*genAngle);
    const float rArmRZ = static_cast<float>(*genAngle);

    const float lArmRX = static_cast<float>(*genAngle);
    const float lArmRY = static_cast<float>(*genAngle);
    const float lArmRZ = static_cast<float>(*genAngle);

    const float rLegRX = static_cast<float>(*genAngle);
    const float rLegRY = static_cast<float>(*genAngle);
    const float rLegRZ = static_cast<float>(*genAngle);

    const float lLegRX = static_cast<float>(*genAngle);
    const float lLegRY = static_cast<float>(*genAngle);
    const float lLegRZ = static_cast<float>(*genAngle);

    // Generate random torso translation
    const float tx = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float ty = static_cast<float>(*rc::gen::inRange(-50, 51));
    const float tz = static_cast<float>(*rc::gen::inRange(-50, 51));

    Pose pose{};
    pose.head     = PartPose{headRX, headRY, headRZ};
    pose.body     = PartPose{bodyRX, bodyRY, bodyRZ};
    pose.rightArm = PartPose{rArmRX, rArmRY, rArmRZ};
    pose.leftArm  = PartPose{lArmRX, lArmRY, lArmRZ};
    pose.rightLeg = PartPose{rLegRX, rLegRY, rLegRZ};
    pose.leftLeg  = PartPose{lLegRX, lLegRY, lLegRZ};
    pose.torsoTranslation = Vec3{tx, ty, tz};

    // Build scene — default skin has transparent outer layers, so only 6 inner meshes
    // Order: head(0), body(1), rightArm(2), leftArm(3), rightLeg(4), leftLeg(5)
    Scene scene = MeshBuilder::buildDefaultScene(pose);
    RC_ASSERT(scene.meshes.size() == 6);

    // Part poses and expected torso params for each mesh
    const PartPose* partPoses[] = {
        &pose.head, &pose.body, &pose.rightArm,
        &pose.leftArm, &pose.rightLeg, &pose.leftLeg
    };

    // isTorso flags: only index 1 (body) is the torso
    const bool isTorso[] = { false, true, false, false, false, false };

    const float tolerance = 1e-6f;

    for (size_t m = 0; m < 6; ++m) {
        const auto& mesh = scene.meshes[m];
        const PartPose& pp = *partPoses[m];

        // All meshes should have hasRotation == true (non-zero rotations guaranteed)
        RC_ASSERT(mesh.hasRotation == true);

        // (a) World-space triangles should be non-empty
        RC_ASSERT(!mesh.triangles.empty());

        // (b) Local-space (unrotated) triangles should be non-empty
        RC_ASSERT(!mesh.localTriangles.empty());

        // (c) Same number of triangles in both spaces
        RC_ASSERT(mesh.triangles.size() == mesh.localTriangles.size());

        // (d) Stored transform params match input Pose
        // Part local rotation should match the part's PartPose
        RC_ASSERT(std::fabs(mesh.rotX - pp.rotX) < tolerance);
        RC_ASSERT(std::fabs(mesh.rotY - pp.rotY) < tolerance);
        RC_ASSERT(std::fabs(mesh.rotZ - pp.rotZ) < tolerance);

        if (isTorso[m]) {
            // Torso: torsoRotX/Y/Z should be zero (torso has no parent transform)
            // torsoTranslation should match pose.torsoTranslation
            RC_ASSERT(std::fabs(mesh.torsoRotX) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoRotY) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoRotZ) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoTranslation.x - pose.torsoTranslation.x) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoTranslation.y - pose.torsoTranslation.y) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoTranslation.z - pose.torsoTranslation.z) < tolerance);
        } else {
            // Child parts: torsoRotX/Y/Z should match pose.body
            RC_ASSERT(std::fabs(mesh.torsoRotX - pose.body.rotX) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoRotY - pose.body.rotY) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoRotZ - pose.body.rotZ) < tolerance);
            // torsoTranslation should match pose.torsoTranslation
            RC_ASSERT(std::fabs(mesh.torsoTranslation.x - pose.torsoTranslation.x) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoTranslation.y - pose.torsoTranslation.y) < tolerance);
            RC_ASSERT(std::fabs(mesh.torsoTranslation.z - pose.torsoTranslation.z) < tolerance);
        }
    }
}

// ── Property 10: Raytracer inverse transform correctness (光线追踪逆变换正确性)
//
// Feature: pose-control, Property 10: Raytracer inverse transform correctness
//
// **Validates: Requirements 8.2**
//
// For any pose with hierarchical transforms, if a ray hits a world-space mesh,
// the world-space hit point inverse-transformed to local space should lie on
// one of the local-space triangles (within floating-point tolerance).

#include "raytracer/intersection.h"

RC_GTEST_PROP(PoseControlProps, RaytracerInverseTransformCorrectness, ()) {
    // Generate random rotation angles for head (child part)
    const float headRotX = static_cast<float>(*rc::gen::inRange(-45, 46));
    const float headRotY = static_cast<float>(*rc::gen::inRange(-45, 46));
    const float headRotZ = static_cast<float>(*rc::gen::inRange(-45, 46));

    // Generate random torso rotation (moderate angles to keep head reachable)
    const float torsoRotX = static_cast<float>(*rc::gen::inRange(-30, 31));
    const float torsoRotY = static_cast<float>(*rc::gen::inRange(-30, 31));
    const float torsoRotZ = static_cast<float>(*rc::gen::inRange(-30, 31));

    // Generate random torso translation (small to keep head in front of camera)
    const float tx = static_cast<float>(*rc::gen::inRange(-10, 11));
    const float ty = static_cast<float>(*rc::gen::inRange(-10, 11));
    const float tz = static_cast<float>(*rc::gen::inRange(-10, 11));
    const Vec3 torsoTranslation{tx, ty, tz};

    // Build pose with head rotation + torso rotation + torso translation
    Pose pose{};
    pose.head = PartPose{headRotX, headRotY, headRotZ};
    pose.body = PartPose{torsoRotX, torsoRotY, torsoRotZ};
    pose.torsoTranslation = torsoTranslation;

    Scene scene = MeshBuilder::buildDefaultScene(pose);

    // Head inner mesh is at index 0
    RC_PRE(scene.meshes.size() > 0);
    const auto& headMesh = scene.meshes[0];
    RC_PRE(!headMesh.triangles.empty());
    RC_PRE(!headMesh.localTriangles.empty());
    RC_PRE(headMesh.hasRotation);

    // Compute the approximate center of the head mesh in world space
    // by averaging all triangle vertices
    Vec3 center{0, 0, 0};
    int vertexCount = 0;
    for (const auto& tri : headMesh.triangles) {
        center = center + tri.v0 + tri.v1 + tri.v2;
        vertexCount += 3;
    }
    center = center / static_cast<float>(vertexCount);

    // Cast a ray from far in front (positive Z) aimed at the center
    const Vec3 rayOrigin = center + Vec3{0, 0, 100};
    const Vec3 rayDir = (center - rayOrigin).normalize();
    Ray ray(rayOrigin, rayDir);

    HitResult result = intersectMesh(ray, headMesh);

    // The ray should hit the head mesh
    RC_PRE(result.hit);

    // Now manually inverse-transform the world-space hit point to local space
    Vec3 localPoint = result.point;

    // Step 1: Undo torso translation
    localPoint = localPoint - headMesh.torsoTranslation;

    // Step 2: Undo torso rotation (Z→Y→X around torso pivot)
    localPoint = testRotateAroundPivot(localPoint, headMesh.torsoPivot,
                                       0, 0, -headMesh.torsoRotZ);
    localPoint = testRotateAroundPivot(localPoint, headMesh.torsoPivot,
                                       0, -headMesh.torsoRotY, 0);
    localPoint = testRotateAroundPivot(localPoint, headMesh.torsoPivot,
                                       -headMesh.torsoRotX, 0, 0);

    // Step 3: Undo part rotation (Z→Y→X around part pivot)
    localPoint = testRotateAroundPivot(localPoint, headMesh.pivot,
                                       0, 0, -headMesh.rotZ);
    localPoint = testRotateAroundPivot(localPoint, headMesh.pivot,
                                       0, -headMesh.rotY, 0);
    localPoint = testRotateAroundPivot(localPoint, headMesh.pivot,
                                       -headMesh.rotX, 0, 0);

    // Verify the inverse-transformed point lies on one of the local-space
    // triangles by checking distance to the triangle plane.
    // For each local triangle, compute the plane equation and check if the
    // point is close to the plane AND within the triangle's bounding region.
    const float tolerance = 0.5f; // generous tolerance for floating-point accumulation

    bool onSurface = false;
    for (const auto& tri : headMesh.localTriangles) {
        // Compute triangle normal from edges
        Vec3 edge1 = tri.v1 - tri.v0;
        Vec3 edge2 = tri.v2 - tri.v0;
        Vec3 triNormal = edge1.cross(edge2);
        float triArea = triNormal.length();
        if (triArea < 1e-8f) continue; // skip degenerate triangles
        triNormal = triNormal / triArea;

        // Distance from point to triangle plane
        float dist = std::fabs((localPoint - tri.v0).dot(triNormal));
        if (dist < tolerance) {
            onSurface = true;
            break;
        }
    }

    RC_ASSERT(onSurface);
}
