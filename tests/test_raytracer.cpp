#include <gtest/gtest.h>
#include "raytracer/raytracer.h"
#include "scene/scene.h"
#include "scene/mesh_builder.h"
#include "math/ray.h"
#include "math/vec3.h"
#include "math/color.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Camera::generateRay tests ──────────────────────────────────────────────

TEST(CameraGenerateRay, CenterRayPointsForward) {
    Camera cam;
    cam.position = Vec3(0, 0, -10);
    cam.target   = Vec3(0, 0, 0);
    cam.up       = Vec3(0, 1, 0);
    cam.fov      = 90.0f;

    // u=0.5, v=0.5 should produce a ray pointing straight forward (+Z)
    Ray ray = cam.generateRay(0.5f, 0.5f, 1.0f);

    EXPECT_FLOAT_EQ(ray.origin.x, 0.0f);
    EXPECT_FLOAT_EQ(ray.origin.y, 0.0f);
    EXPECT_FLOAT_EQ(ray.origin.z, -10.0f);

    // Direction should be approximately (0, 0, 1)
    EXPECT_NEAR(ray.direction.x, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.y, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.z, 1.0f, 1e-5f);
}

TEST(CameraGenerateRay, CornerRaysAreNormalized) {
    Camera cam;
    cam.position = Vec3(0, 0, -5);
    cam.target   = Vec3(0, 0, 0);
    cam.up       = Vec3(0, 1, 0);
    cam.fov      = 60.0f;

    // Test all four corners
    float corners[][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (auto& c : corners) {
        Ray ray = cam.generateRay(c[0], c[1], 1.0f);
        float len = ray.direction.length();
        EXPECT_NEAR(len, 1.0f, 1e-5f);
    }
}

TEST(CameraGenerateRay, AspectRatioStretchesHorizontally) {
    Camera cam;
    cam.position = Vec3(0, 0, -5);
    cam.target   = Vec3(0, 0, 0);
    cam.up       = Vec3(0, 1, 0);
    cam.fov      = 90.0f;

    // With aspect ratio 2:1, the horizontal extent should be wider
    Ray rayWide = cam.generateRay(1.0f, 0.5f, 2.0f);
    Ray raySquare = cam.generateRay(1.0f, 0.5f, 1.0f);

    // The wide ray should have a larger x component
    EXPECT_GT(std::fabs(rayWide.direction.x), std::fabs(raySquare.direction.x));
}

TEST(CameraGenerateRay, VInvertedTopIsV0) {
    Camera cam;
    cam.position = Vec3(0, 0, -5);
    cam.target   = Vec3(0, 0, 0);
    cam.up       = Vec3(0, 1, 0);
    cam.fov      = 90.0f;

    // v=0 should be top of image (positive Y direction)
    Ray rayTop = cam.generateRay(0.5f, 0.0f, 1.0f);
    // v=1 should be bottom of image (negative Y direction)
    Ray rayBottom = cam.generateRay(0.5f, 1.0f, 1.0f);

    EXPECT_GT(rayTop.direction.y, 0.0f);
    EXPECT_LT(rayBottom.direction.y, 0.0f);
}

// ─── traceRay tests ─────────────────────────────────────────────────────────

// Helper: create a minimal scene with one small box
static Scene makeSimpleScene() {
    Scene scene;
    scene.backgroundColor = Color(0.2f, 0.3f, 0.5f, 1.0f);
    scene.light.position = Vec3(10, 10, -10);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;
    scene.camera.position = Vec3(0, 0, -10);
    scene.camera.target = Vec3(0, 0, 0);
    scene.camera.up = Vec3(0, 1, 0);
    scene.camera.fov = 60.0f;
    return scene;
}

// Helper: create a simple opaque textured mesh (a 2x2x2 box at origin)
static Mesh makeTestBox() {
    // We'll build a minimal box mesh manually
    // Using the same structure as mesh_builder: 12 triangles, 6 faces
    // For simplicity, create a static texture region
    static TextureRegion tex;
    tex.width = 2;
    tex.height = 2;
    tex.pixels = {
        Color(1, 0, 0, 1), Color(0, 1, 0, 1),
        Color(0, 0, 1, 1), Color(1, 1, 0, 1)
    };

    Mesh mesh;
    mesh.isOuterLayer = false;

    float hw = 1.0f, hh = 1.0f, hd = 1.0f;
    Vec3 v000(-hw, -hh, -hd), v100(hw, -hh, -hd);
    Vec3 v010(-hw,  hh, -hd), v110(hw,  hh, -hd);
    Vec3 v001(-hw, -hh,  hd), v101(hw, -hh,  hd);
    Vec3 v011(-hw,  hh,  hd), v111(hw,  hh,  hd);

    auto addFace = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n) {
        Triangle t1, t2;
        t1.v0 = a; t1.v1 = b; t1.v2 = c;
        t1.normal = n;
        t1.u0 = 0; t1.v0_uv = 0; t1.u1 = 1; t1.v1_uv = 0; t1.u2 = 1; t1.v2_uv = 1;
        t1.texture = &tex;

        t2.v0 = a; t2.v1 = c; t2.v2 = d;
        t2.normal = n;
        t2.u0 = 0; t2.v0_uv = 0; t2.u1 = 1; t2.v1_uv = 1; t2.u2 = 0; t2.v2_uv = 1;
        t2.texture = &tex;

        mesh.triangles.push_back(t1);
        mesh.triangles.push_back(t2);
    };

    // front (-Z), back (+Z), left (+X), right (-X), top (+Y), bottom (-Y)
    addFace(v010, v110, v100, v000, Vec3(0,0,-1));
    addFace(v111, v011, v001, v101, Vec3(0,0,1));
    addFace(v110, v111, v101, v100, Vec3(1,0,0));
    addFace(v011, v010, v000, v001, Vec3(-1,0,0));
    addFace(v011, v111, v110, v010, Vec3(0,1,0));
    addFace(v000, v100, v101, v001, Vec3(0,-1,0));

    return mesh;
}

TEST(TraceRay, MissReturnsBackgroundColor) {
    Scene scene = makeSimpleScene();
    // No meshes in scene — any ray should miss
    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    Color result = RayTracer::traceRay(ray, scene, 0, 3);

    EXPECT_FLOAT_EQ(result.r, scene.backgroundColor.r);
    EXPECT_FLOAT_EQ(result.g, scene.backgroundColor.g);
    EXPECT_FLOAT_EQ(result.b, scene.backgroundColor.b);
}

TEST(TraceRay, DepthExceedsMaxBouncesReturnsBackground) {
    Scene scene = makeSimpleScene();
    scene.meshes.push_back(makeTestBox());

    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    // depth > maxBounces should immediately return background
    Color result = RayTracer::traceRay(ray, scene, 5, 3);

    EXPECT_FLOAT_EQ(result.r, scene.backgroundColor.r);
    EXPECT_FLOAT_EQ(result.g, scene.backgroundColor.g);
    EXPECT_FLOAT_EQ(result.b, scene.backgroundColor.b);
}

TEST(TraceRay, HitReturnsNonBackgroundColor) {
    Scene scene = makeSimpleScene();
    scene.meshes.push_back(makeTestBox());

    // Ray aimed at the box at origin
    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    Color result = RayTracer::traceRay(ray, scene, 0, 3);

    // Should NOT be the background color (we hit the box)
    bool isBackground = (std::fabs(result.r - scene.backgroundColor.r) < 1e-5f &&
                         std::fabs(result.g - scene.backgroundColor.g) < 1e-5f &&
                         std::fabs(result.b - scene.backgroundColor.b) < 1e-5f);
    EXPECT_FALSE(isBackground);
}

TEST(TraceRay, ZeroBouncesNoReflection) {
    Scene scene = makeSimpleScene();
    scene.meshes.push_back(makeTestBox());

    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    // maxBounces=0: direct lighting only, no reflection
    Color result0 = RayTracer::traceRay(ray, scene, 0, 0);

    // maxBounces=5: with reflections
    Color result5 = RayTracer::traceRay(ray, scene, 0, 5);

    // With 0 bounces, the result should be pure direct shading.
    // With more bounces, reflections may slightly alter the color.
    // They should be close but potentially different.
    // At minimum, the 0-bounce result should be valid (not background).
    bool isBackground = (std::fabs(result0.r - scene.backgroundColor.r) < 1e-5f &&
                         std::fabs(result0.g - scene.backgroundColor.g) < 1e-5f &&
                         std::fabs(result0.b - scene.backgroundColor.b) < 1e-5f);
    EXPECT_FALSE(isBackground);
}

TEST(TraceRay, RayMissingSideReturnsBackground) {
    Scene scene = makeSimpleScene();
    scene.meshes.push_back(makeTestBox());

    // Ray aimed away from the box
    Ray ray(Vec3(0, 0, -10), Vec3(0, 1, 0));

    Color result = RayTracer::traceRay(ray, scene, 0, 3);

    EXPECT_FLOAT_EQ(result.r, scene.backgroundColor.r);
    EXPECT_FLOAT_EQ(result.g, scene.backgroundColor.g);
    EXPECT_FLOAT_EQ(result.b, scene.backgroundColor.b);
}
