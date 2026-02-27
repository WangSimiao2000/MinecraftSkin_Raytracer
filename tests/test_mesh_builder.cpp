/**
 * Unit tests for MeshBuilder – standard character proportions.
 *
 * Validates: Requirement 2.4
 * THE Mesh_Builder SHALL 按照 Minecraft 标准角色比例定位各身体部位
 *
 * Expected body part positions and sizes (from design doc):
 *   Head:     8×8×8   center (0, 28, 0)
 *   Body:     8×12×4  center (0, 18, 0)
 *   RightArm: 4×12×4  center (-6, 18, 0)
 *   LeftArm:  4×12×4  center (6, 18, 0)
 *   RightLeg: 4×12×4  center (-2, 6, 0)
 *   LeftLeg:  4×12×4  center (2, 6, 0)
 */

#include <gtest/gtest.h>
#include <cmath>

#include "skin/skin_parser.h"
#include "skin/texture_region.h"
#include "scene/mesh_builder.h"
#include "scene/scene.h"
#include "math/color.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

// Create an opaque TextureRegion (all pixels white, alpha=1)
static TextureRegion makeOpaqueRegion(int w, int h) {
    TextureRegion region(w, h);
    for (int i = 0; i < w * h; ++i) {
        region.pixels[i] = Color(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return region;
}

// Create a fully transparent TextureRegion
static TextureRegion makeTransparentRegion(int w, int h) {
    TextureRegion region(w, h);
    for (int i = 0; i < w * h; ++i) {
        region.pixels[i] = Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return region;
}

// Build a BodyPartTexture with opaque faces for box dimensions (w, h, d)
static BodyPartTexture makeOpaqueBodyPart(int w, int h, int d) {
    BodyPartTexture tex;
    tex.top    = makeOpaqueRegion(w, d);
    tex.bottom = makeOpaqueRegion(w, d);
    tex.front  = makeOpaqueRegion(w, h);
    tex.back   = makeOpaqueRegion(w, h);
    tex.left   = makeOpaqueRegion(d, h);
    tex.right  = makeOpaqueRegion(d, h);
    return tex;
}

// Build a BodyPartTexture with transparent faces for box dimensions (w, h, d)
static BodyPartTexture makeTransparentBodyPart(int w, int h, int d) {
    BodyPartTexture tex;
    tex.top    = makeTransparentRegion(w, d);
    tex.bottom = makeTransparentRegion(w, d);
    tex.front  = makeTransparentRegion(w, h);
    tex.back   = makeTransparentRegion(w, h);
    tex.left   = makeTransparentRegion(d, h);
    tex.right  = makeTransparentRegion(d, h);
    return tex;
}

// Build a known SkinData with opaque inner layers and transparent outer layers
// so that buildScene produces exactly 6 inner meshes in deterministic order.
static SkinData makeTestSkinData() {
    SkinData skin;
    skin.format = SkinData::NEW_64x64;

    // Inner layers (opaque)
    skin.head     = makeOpaqueBodyPart(8, 8, 8);
    skin.body     = makeOpaqueBodyPart(8, 12, 4);
    skin.rightArm = makeOpaqueBodyPart(4, 12, 4);
    skin.leftArm  = makeOpaqueBodyPart(4, 12, 4);
    skin.rightLeg = makeOpaqueBodyPart(4, 12, 4);
    skin.leftLeg  = makeOpaqueBodyPart(4, 12, 4);

    // Outer layers (transparent → skipped)
    skin.headOuter     = makeTransparentBodyPart(8, 8, 8);
    skin.bodyOuter     = makeTransparentBodyPart(8, 12, 4);
    skin.rightArmOuter = makeTransparentBodyPart(4, 12, 4);
    skin.leftArmOuter  = makeTransparentBodyPart(4, 12, 4);
    skin.rightLegOuter = makeTransparentBodyPart(4, 12, 4);
    skin.leftLegOuter  = makeTransparentBodyPart(4, 12, 4);

    return skin;
}

struct AABB {
    Vec3 min;
    Vec3 max;
};

static AABB computeBoundingBox(const Mesh& mesh) {
    AABB box;
    box.min = Vec3( 1e30f,  1e30f,  1e30f);
    box.max = Vec3(-1e30f, -1e30f, -1e30f);
    for (const auto& tri : mesh.triangles) {
        const Vec3* verts[] = { &tri.v0, &tri.v1, &tri.v2 };
        for (const Vec3* v : verts) {
            if (v->x < box.min.x) box.min.x = v->x;
            if (v->y < box.min.y) box.min.y = v->y;
            if (v->z < box.min.z) box.min.z = v->z;
            if (v->x > box.max.x) box.max.x = v->x;
            if (v->y > box.max.y) box.max.y = v->y;
            if (v->z > box.max.z) box.max.z = v->z;
        }
    }
    return box;
}

static Vec3 bboxCenter(const AABB& box) {
    return Vec3(
        (box.min.x + box.max.x) / 2.0f,
        (box.min.y + box.max.y) / 2.0f,
        (box.min.z + box.max.z) / 2.0f
    );
}

static Vec3 bboxExtent(const AABB& box) {
    return Vec3(
        box.max.x - box.min.x,
        box.max.y - box.min.y,
        box.max.z - box.min.z
    );
}

// ── Test fixture ────────────────────────────────────────────────────────────

class MeshBuilderTest : public ::testing::Test {
protected:
    Scene scene;

    void SetUp() override {
        SkinData skin = makeTestSkinData();
        scene = MeshBuilder::buildScene(skin);
    }
};

// ── Tests ───────────────────────────────────────────────────────────────────

// With all outer layers transparent, we should get exactly 6 inner meshes.
TEST_F(MeshBuilderTest, ProducesExactlySixInnerMeshes) {
    ASSERT_EQ(scene.meshes.size(), 6u);
    for (const auto& mesh : scene.meshes) {
        EXPECT_FALSE(mesh.isOuterLayer);
    }
}

// Expected values per body part (order matches buildScene: head, body, rArm, lArm, rLeg, lLeg)
// After 180° Y-axis rotation, X coordinates are negated (character faces +Z toward camera)
struct BodyPartExpectation {
    const char* name;
    Vec3 center;
    Vec3 size;  // width, height, depth
};

static const BodyPartExpectation kExpected[] = {
    { "Head",     Vec3(0, 28, 0), Vec3(8, 8, 8)   },
    { "Body",     Vec3(0, 18, 0), Vec3(8, 12, 4)   },
    { "RightArm", Vec3(6, 18, 0), Vec3(4, 12, 4)  },
    { "LeftArm",  Vec3(-6, 18, 0), Vec3(4, 12, 4) },
    { "RightLeg", Vec3(2, 6, 0), Vec3(4, 12, 4)   },
    { "LeftLeg",  Vec3(-2, 6, 0), Vec3(4, 12, 4)  },
};

static constexpr float kTolerance = 1e-4f;

TEST_F(MeshBuilderTest, HeadPosition) {
    AABB box = computeBoundingBox(scene.meshes[0]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, 0.0f, kTolerance)  << "Head center X";
    EXPECT_NEAR(center.y, 28.0f, kTolerance) << "Head center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "Head center Z";
    EXPECT_NEAR(extent.x, 8.0f, kTolerance)  << "Head width";
    EXPECT_NEAR(extent.y, 8.0f, kTolerance)  << "Head height";
    EXPECT_NEAR(extent.z, 8.0f, kTolerance)  << "Head depth";
}

TEST_F(MeshBuilderTest, BodyPosition) {
    AABB box = computeBoundingBox(scene.meshes[1]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, 0.0f, kTolerance)  << "Body center X";
    EXPECT_NEAR(center.y, 18.0f, kTolerance) << "Body center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "Body center Z";
    EXPECT_NEAR(extent.x, 8.0f, kTolerance)  << "Body width";
    EXPECT_NEAR(extent.y, 12.0f, kTolerance) << "Body height";
    EXPECT_NEAR(extent.z, 4.0f, kTolerance)  << "Body depth";
}

TEST_F(MeshBuilderTest, RightArmPosition) {
    AABB box = computeBoundingBox(scene.meshes[2]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, 6.0f, kTolerance)  << "RightArm center X";
    EXPECT_NEAR(center.y, 18.0f, kTolerance) << "RightArm center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "RightArm center Z";
    EXPECT_NEAR(extent.x, 4.0f, kTolerance)  << "RightArm width";
    EXPECT_NEAR(extent.y, 12.0f, kTolerance) << "RightArm height";
    EXPECT_NEAR(extent.z, 4.0f, kTolerance)  << "RightArm depth";
}

TEST_F(MeshBuilderTest, LeftArmPosition) {
    AABB box = computeBoundingBox(scene.meshes[3]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, -6.0f, kTolerance) << "LeftArm center X";
    EXPECT_NEAR(center.y, 18.0f, kTolerance) << "LeftArm center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "LeftArm center Z";
    EXPECT_NEAR(extent.x, 4.0f, kTolerance)  << "LeftArm width";
    EXPECT_NEAR(extent.y, 12.0f, kTolerance) << "LeftArm height";
    EXPECT_NEAR(extent.z, 4.0f, kTolerance)  << "LeftArm depth";
}

TEST_F(MeshBuilderTest, RightLegPosition) {
    AABB box = computeBoundingBox(scene.meshes[4]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, 2.0f, kTolerance)  << "RightLeg center X";
    EXPECT_NEAR(center.y, 6.0f, kTolerance)  << "RightLeg center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "RightLeg center Z";
    EXPECT_NEAR(extent.x, 4.0f, kTolerance)  << "RightLeg width";
    EXPECT_NEAR(extent.y, 12.0f, kTolerance) << "RightLeg height";
    EXPECT_NEAR(extent.z, 4.0f, kTolerance)  << "RightLeg depth";
}

TEST_F(MeshBuilderTest, LeftLegPosition) {
    AABB box = computeBoundingBox(scene.meshes[5]);
    Vec3 center = bboxCenter(box);
    Vec3 extent = bboxExtent(box);
    EXPECT_NEAR(center.x, -2.0f, kTolerance) << "LeftLeg center X";
    EXPECT_NEAR(center.y, 6.0f, kTolerance)  << "LeftLeg center Y";
    EXPECT_NEAR(center.z, 0.0f, kTolerance)  << "LeftLeg center Z";
    EXPECT_NEAR(extent.x, 4.0f, kTolerance)  << "LeftLeg width";
    EXPECT_NEAR(extent.y, 12.0f, kTolerance) << "LeftLeg height";
    EXPECT_NEAR(extent.z, 4.0f, kTolerance)  << "LeftLeg depth";
}
