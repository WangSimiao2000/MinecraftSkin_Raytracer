#include <gtest/gtest.h>
#include <cmath>
#include "raytracer/shading.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/triangle.h"
#include "math/vec3.h"
#include "math/color.h"

// Helper: create a minimal HitResult
static HitResult makeHit(const Vec3& point, const Vec3& normal, const Color& texColor) {
    HitResult h;
    h.hit = true;
    h.t = 1.0f;
    h.point = point;
    h.normal = normal;
    h.textureColor = texColor;
    h.isOuterLayer = false;
    return h;
}

// Helper: create an empty scene with a light
static Scene makeEmptyScene(const Vec3& lightPos) {
    Scene s;
    s.light.position = lightPos;
    s.light.color = Color(1, 1, 1, 1);
    s.light.intensity = 1.0f;
    s.backgroundColor = Color(0, 0, 0, 1);
    return s;
}

// Helper: build a simple axis-aligned box mesh for shadow testing
static Mesh makeBoxMesh(const Vec3& center, float halfSize) {
    Mesh m;
    m.isOuterLayer = false;
    float hs = halfSize;
    Vec3 mn = center - Vec3(hs, hs, hs);
    Vec3 mx = center + Vec3(hs, hs, hs);

    // We only need the AABB bounds to be correct for intersection.
    // Create 12 triangles forming a box (2 per face).
    auto addFace = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n) {
        Triangle t1, t2;
        t1.v0 = a; t1.v1 = b; t1.v2 = c; t1.normal = n;
        t1.u0 = 0; t1.v0_uv = 0; t1.u1 = 1; t1.v1_uv = 0; t1.u2 = 1; t1.v2_uv = 1;
        t1.texture = nullptr;
        t2.v0 = a; t2.v1 = c; t2.v2 = d; t2.normal = n;
        t2.u0 = 0; t2.v0_uv = 0; t2.u1 = 1; t2.v1_uv = 1; t2.u2 = 0; t2.v2_uv = 1;
        t2.texture = nullptr;
        m.triangles.push_back(t1);
        m.triangles.push_back(t2);
    };

    Vec3 v000(mn.x, mn.y, mn.z), v100(mx.x, mn.y, mn.z);
    Vec3 v010(mn.x, mx.y, mn.z), v110(mx.x, mx.y, mn.z);
    Vec3 v001(mn.x, mn.y, mx.z), v101(mx.x, mn.y, mx.z);
    Vec3 v011(mn.x, mx.y, mx.z), v111(mx.x, mx.y, mx.z);

    addFace(v010, v110, v100, v000, Vec3(0, 0, -1)); // front
    addFace(v111, v011, v001, v101, Vec3(0, 0, 1));  // back
    addFace(v110, v111, v101, v100, Vec3(1, 0, 0));  // +X
    addFace(v011, v010, v000, v001, Vec3(-1, 0, 0)); // -X
    addFace(v011, v111, v110, v010, Vec3(0, 1, 0));  // top
    addFace(v000, v100, v101, v001, Vec3(0, -1, 0)); // bottom

    return m;
}

// ---- Ambient-only tests ----

TEST(ShadingTest, AmbientOnlyWhenLightBehindSurface) {
    // Light is behind the surface (opposite side of normal)
    // dot(N, L) < 0, so diffuse and specular should be 0
    Scene scene = makeEmptyScene(Vec3(0, 0, -10));
    HitResult hit = makeHit(Vec3(0, 0, 0), Vec3(0, 0, 1), Color(1, 1, 1, 1));
    Vec3 viewDir(0, 0, 1);

    ShadingParams params;
    Color result = shade(hit, viewDir, scene.light, scene, params);

    // Only ambient: 0.1 * (1,1,1)
    EXPECT_NEAR(result.r, 0.1f, 1e-4f);
    EXPECT_NEAR(result.g, 0.1f, 1e-4f);
    EXPECT_NEAR(result.b, 0.1f, 1e-4f);
}

// ---- Full Blinn-Phong (no shadow) ----

TEST(ShadingTest, DiffuseAndSpecularWithDirectLight) {
    // Light directly above, normal pointing up, view from above
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    HitResult hit = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(0.8f, 0.6f, 0.4f, 1));
    Vec3 viewDir(0, 1, 0); // looking down from above

    ShadingParams params;
    Color result = shade(hit, viewDir, scene.light, scene, params);

    // N = (0,1,0), L = (0,1,0), V = (0,1,0)
    // dot(N,L) = 1.0
    // H = normalize(L+V) = (0,1,0), dot(N,H) = 1.0
    // ambient = 0.1 * texColor
    // diffuse = 0.7 * 1.0 * texColor * lightColor
    // specular = 0.3 * pow(1.0, 32) * lightColor = 0.3
    float expectedR = 0.1f * 0.8f + 0.7f * 0.8f + 0.3f;
    float expectedG = 0.1f * 0.6f + 0.7f * 0.6f + 0.3f;
    float expectedB = 0.1f * 0.4f + 0.7f * 0.4f + 0.3f;

    EXPECT_NEAR(result.r, std::min(expectedR, 1.0f), 1e-3f);
    EXPECT_NEAR(result.g, std::min(expectedG, 1.0f), 1e-3f);
    EXPECT_NEAR(result.b, std::min(expectedB, 1.0f), 1e-3f);
}

TEST(ShadingTest, DiffuseOnlyAtGrazingAngle) {
    // Light at 90 degrees from view direction — specular should be minimal
    Scene scene = makeEmptyScene(Vec3(10, 0, 0));
    HitResult hit = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(1, 1, 1, 1));
    Vec3 viewDir(0, 1, 0);

    ShadingParams params;
    Color result = shade(hit, viewDir, scene.light, scene, params);

    // L = (1,0,0), N = (0,1,0), dot(N,L) = 0 → diffuse = 0
    // H = normalize((1,0,0)+(0,1,0)) = normalize(1,1,0), dot(N,H) = 1/sqrt(2)
    // specular = 0.3 * pow(1/sqrt(2), 32) ≈ very small
    // Result should be close to ambient only
    EXPECT_NEAR(result.r, 0.1f, 0.05f);
}

// ---- Shadow tests ----

TEST(ShadingTest, InShadowReturnsAmbientOnly) {
    // Place a blocker between the hit point and the light
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    scene.meshes.push_back(makeBoxMesh(Vec3(0, 5, 0), 2.0f)); // blocker

    HitResult hit = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(1, 1, 1, 1));
    Vec3 viewDir(0, 1, 0);

    ShadingParams params;
    Color result = shade(hit, viewDir, scene.light, scene, params);

    // Should be ambient only since blocker is in the way
    EXPECT_NEAR(result.r, 0.1f, 1e-4f);
    EXPECT_NEAR(result.g, 0.1f, 1e-4f);
    EXPECT_NEAR(result.b, 0.1f, 1e-4f);
}

TEST(ShadingTest, NotInShadowWhenPathClear) {
    // No blockers in the scene
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));

    Vec3 point(0, 0, 0);
    Vec3 normal(0, 1, 0);

    bool shadow = isInShadow(point, normal, scene.light.position, scene);
    EXPECT_FALSE(shadow);
}

TEST(ShadingTest, InShadowWhenBlocked) {
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    scene.meshes.push_back(makeBoxMesh(Vec3(0, 5, 0), 1.0f));

    Vec3 point(0, 0, 0);
    Vec3 normal(0, 1, 0);

    bool shadow = isInShadow(point, normal, scene.light.position, scene);
    EXPECT_TRUE(shadow);
}

TEST(ShadingTest, NotInShadowWhenBlockerBehindLight) {
    // Blocker is behind the light — should not cast shadow
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    scene.meshes.push_back(makeBoxMesh(Vec3(0, 20, 0), 1.0f));

    Vec3 point(0, 0, 0);
    Vec3 normal(0, 1, 0);

    bool shadow = isInShadow(point, normal, scene.light.position, scene);
    EXPECT_FALSE(shadow);
}

// ---- Texture color influence ----

TEST(ShadingTest, TextureColorAffectsResult) {
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    Vec3 viewDir(0, 1, 0);
    ShadingParams params;

    HitResult hitRed = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(1, 0, 0, 1));
    HitResult hitBlue = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(0, 0, 1, 1));

    Color resultRed = shade(hitRed, viewDir, scene.light, scene, params);
    Color resultBlue = shade(hitBlue, viewDir, scene.light, scene, params);

    // Red texture should have high R, low B
    EXPECT_GT(resultRed.r, resultRed.b);
    // Blue texture should have high B, low R
    EXPECT_GT(resultBlue.b, resultBlue.r);
}

// ---- Custom shading params ----

TEST(ShadingTest, ZeroDiffuseAndSpecularGivesAmbientOnly) {
    Scene scene = makeEmptyScene(Vec3(0, 10, 0));
    HitResult hit = makeHit(Vec3(0, 0, 0), Vec3(0, 1, 0), Color(1, 1, 1, 1));
    Vec3 viewDir(0, 1, 0);

    ShadingParams params;
    params.kd = 0.0f;
    params.ks = 0.0f;
    params.ambient = 0.5f;

    Color result = shade(hit, viewDir, scene.light, scene, params);

    EXPECT_NEAR(result.r, 0.5f, 1e-4f);
    EXPECT_NEAR(result.g, 0.5f, 1e-4f);
    EXPECT_NEAR(result.b, 0.5f, 1e-4f);
}
