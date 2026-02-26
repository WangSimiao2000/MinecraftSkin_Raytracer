#include <gtest/gtest.h>
#include "raytracer/intersection.h"
#include "scene/mesh_builder.h"
#include "skin/skin_parser.h"

// Helper: create a simple BodyPartTexture with solid color on all faces
static BodyPartTexture makeSolidTexture(const Color& color, int w, int h) {
    std::vector<Color> pixels(w * h, color);
    TextureRegion region(w, h, pixels);
    return { region, region, region, region, region, region };
}

// Helper: build a single box mesh at origin with given size and solid color
static Mesh buildTestBox(const Color& color, const Vec3& pos, const Vec3& size, float offset = 0.0f) {
    BodyPartTexture tex = makeSolidTexture(color, 8, 8);
    return MeshBuilder::buildBox(tex, pos, size, offset);
}

// ── Ray hitting a box from the front ────────────────────────────────────────
TEST(IntersectMesh, RayHitsBoxFront) {
    // Box centered at origin, size 2x2x2 → extends from (-1,-1,-1) to (1,1,1)
    Color red(1, 0, 0, 1);
    BodyPartTexture tex = makeSolidTexture(red, 4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f);

    // Ray from z=5 pointing toward -Z, should hit front face at z=-1
    Ray ray(Vec3(0, 0, 5), Vec3(0, 0, -1));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.t, 4.0f, 1e-4f); // 5 - 1 = 4
    EXPECT_NEAR(hit.point.z, 1.0f, 1e-4f);
    EXPECT_NEAR(hit.normal.z, 1.0f, 1e-4f); // +Z normal (back face from ray's perspective)
    EXPECT_FLOAT_EQ(hit.textureColor.r, 1.0f);
    EXPECT_FLOAT_EQ(hit.textureColor.a, 1.0f);
    EXPECT_FALSE(hit.isOuterLayer);
}

// ── Ray missing a box ───────────────────────────────────────────────────────
TEST(IntersectMesh, RayMissesBox) {
    Color blue(0, 0, 1, 1);
    BodyPartTexture tex = makeSolidTexture(blue, 4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f);

    // Ray going parallel, offset in Y so it misses
    Ray ray(Vec3(0, 5, 5), Vec3(0, 0, -1));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_FALSE(hit.hit);
}

// ── Ray hitting a box from the side (+X face) ───────────────────────────────
TEST(IntersectMesh, RayHitsBoxSide) {
    Color green(0, 1, 0, 1);
    BodyPartTexture tex = makeSolidTexture(green, 4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f);

    // Ray from +X direction
    Ray ray(Vec3(5, 0, 0), Vec3(-1, 0, 0));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.t, 4.0f, 1e-4f);
    EXPECT_NEAR(hit.point.x, 1.0f, 1e-4f);
    EXPECT_NEAR(hit.normal.x, 1.0f, 1e-4f); // +X normal (left face)
}

// ── Transparent pixel treated as miss ───────────────────────────────────────
TEST(IntersectMesh, TransparentPixelTreatedAsMiss) {
    // All pixels fully transparent
    Color transparent(0, 0, 0, 0);
    BodyPartTexture tex = makeSolidTexture(transparent, 4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f);

    Ray ray(Vec3(0, 0, 5), Vec3(0, 0, -1));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_FALSE(hit.hit); // Transparent = miss
}

// ── Outer layer flag propagated ─────────────────────────────────────────────
TEST(IntersectMesh, OuterLayerFlagPropagated) {
    Color white(1, 1, 1, 1);
    BodyPartTexture tex = makeSolidTexture(white, 4, 4);
    // offset > 0 marks it as outer layer
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.5f);

    Ray ray(Vec3(0, 0, 5), Vec3(0, 0, -1));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_TRUE(hit.hit);
    EXPECT_TRUE(hit.isOuterLayer);
}

// ── Ray behind box (no hit) ─────────────────────────────────────────────────
TEST(IntersectMesh, RayBehindBox) {
    Color red(1, 0, 0, 1);
    BodyPartTexture tex = makeSolidTexture(red, 4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f);

    // Ray pointing away from the box
    Ray ray(Vec3(0, 0, 5), Vec3(0, 0, 1));
    HitResult hit = intersectMesh(ray, box);

    EXPECT_FALSE(hit.hit);
}

// ── Scene intersection: closest hit ─────────────────────────────────────────
TEST(IntersectScene, FindsClosestHit) {
    Scene scene;
    scene.backgroundColor = Color(0, 0, 0, 1);

    Color red(1, 0, 0, 1);
    Color blue(0, 0, 1, 1);
    BodyPartTexture texRed = makeSolidTexture(red, 4, 4);
    BodyPartTexture texBlue = makeSolidTexture(blue, 4, 4);

    // Near box at z=2, far box at z=-5
    scene.meshes.push_back(MeshBuilder::buildBox(texRed, Vec3(0, 0, 2), Vec3(2, 2, 2), 0.0f));
    scene.meshes.push_back(MeshBuilder::buildBox(texBlue, Vec3(0, 0, -5), Vec3(2, 2, 2), 0.0f));

    Ray ray(Vec3(0, 0, 10), Vec3(0, 0, -1));
    HitResult hit = intersectScene(ray, scene);

    EXPECT_TRUE(hit.hit);
    // Should hit the near (red) box first
    EXPECT_FLOAT_EQ(hit.textureColor.r, 1.0f);
    EXPECT_FLOAT_EQ(hit.textureColor.b, 0.0f);
}

// ── Scene intersection: transparent outer, opaque inner ─────────────────────
TEST(IntersectScene, TransparentOuterHitsInner) {
    Scene scene;
    scene.backgroundColor = Color(0, 0, 0, 1);

    Color opaque(1, 0, 0, 1);
    Color transparent(0, 0, 0, 0);
    BodyPartTexture texOpaque = makeSolidTexture(opaque, 4, 4);
    BodyPartTexture texTransparent = makeSolidTexture(transparent, 4, 4);

    // Inner box (opaque)
    scene.meshes.push_back(MeshBuilder::buildBox(texOpaque, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.0f));
    // Outer box (transparent, slightly larger)
    scene.meshes.push_back(MeshBuilder::buildBox(texTransparent, Vec3(0, 0, 0), Vec3(2, 2, 2), 0.5f));

    Ray ray(Vec3(0, 0, 10), Vec3(0, 0, -1));
    HitResult hit = intersectScene(ray, scene);

    EXPECT_TRUE(hit.hit);
    // Should pass through transparent outer and hit opaque inner
    EXPECT_FLOAT_EQ(hit.textureColor.r, 1.0f);
    EXPECT_FLOAT_EQ(hit.textureColor.a, 1.0f);
    EXPECT_FALSE(hit.isOuterLayer);
}

// ── Scene intersection: no meshes ───────────────────────────────────────────
TEST(IntersectScene, EmptySceneNoHit) {
    Scene scene;
    scene.backgroundColor = Color(0, 0, 0, 1);

    Ray ray(Vec3(0, 0, 10), Vec3(0, 0, -1));
    HitResult hit = intersectScene(ray, scene);

    EXPECT_FALSE(hit.hit);
}
