/**
 * Property-based tests for ray recursion depth (Property 9).
 *
 * **Validates: Requirements 3.5, 3.6, 6.3**
 *
 * Property 9: Ray Recursion Depth (光线递归深度)
 * For any bounce count configuration N (0 <= N <= 10), traceRay's recursion
 * depth should not exceed N. When N = 0, no reflection rays should be
 * produced. When a ray misses all objects, the background color is returned.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <cmath>
#include <algorithm>

#include "raytracer/raytracer.h"
#include "raytracer/shading.h"
#include "raytracer/intersection.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/mesh_builder.h"
#include "math/vec3.h"
#include "math/color.h"
#include "math/ray.h"
#include "skin/texture_region.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

// Generate a random unit vector by sampling xyz in [-1, 1] and normalizing.
static Vec3 genUnitVector() {
    float x = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float y = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float z = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float len = std::sqrt(x * x + y * y + z * z);
    RC_PRE(len > 0.01f);
    return Vec3(x / len, y / len, z / len);
}

// Create a BodyPartTexture where all 6 faces are filled with an opaque color.
static BodyPartTexture makeOpaqueBodyPartTexture(int faceW, int faceH) {
    std::vector<Color> pixels(faceW * faceH, Color(0.8f, 0.2f, 0.2f, 1.0f));
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

// Create a scene with a single opaque box at the origin.
static Scene makeSceneWithBox() {
    Scene scene;
    scene.backgroundColor = Color(0.1f, 0.2f, 0.4f, 1.0f);
    scene.light.position = Vec3(10, 10, -10);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    BodyPartTexture tex = makeOpaqueBodyPartTexture(4, 4);
    Mesh box = MeshBuilder::buildBox(tex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.0f);
    scene.meshes.push_back(box);
    return scene;
}

// Create an empty scene (no meshes).
static Scene makeEmptyScene() {
    Scene scene;
    scene.backgroundColor = Color(0.1f, 0.2f, 0.4f, 1.0f);
    scene.light.position = Vec3(10, 10, -10);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;
    return scene;
}

// ── Sub-property 1: Miss returns background color ───────────────────────────
//
// **Validates: Requirements 3.5, 3.6, 6.3**
//
// For any random ray that doesn't hit any mesh, traceRay returns
// backgroundColor regardless of maxBounces.

RC_GTEST_PROP(RayRecursionDepthProps, MissReturnsBackground, ()) {
    int maxBounces = *rc::gen::inRange(0, 11);

    // Generate a random ray origin and direction
    float ox = *rc::gen::inRange(-500, 501) / 10.0f;
    float oy = *rc::gen::inRange(-500, 501) / 10.0f;
    float oz = *rc::gen::inRange(-500, 501) / 10.0f;
    Vec3 origin(ox, oy, oz);

    Vec3 dir = genUnitVector();

    // Empty scene — no meshes, so any ray must miss
    Scene scene = makeEmptyScene();
    Ray ray(origin, dir);

    Color result = RayTracer::traceRay(ray, scene, 0, maxBounces);

    RC_ASSERT(std::abs(result.r - scene.backgroundColor.r) < 1e-5f);
    RC_ASSERT(std::abs(result.g - scene.backgroundColor.g) < 1e-5f);
    RC_ASSERT(std::abs(result.b - scene.backgroundColor.b) < 1e-5f);
}

// ── Sub-property 2: Depth exceeded returns background ───────────────────────
//
// **Validates: Requirements 3.5, 3.6, 6.3**
//
// For any maxBounces N in [0, 10], calling traceRay with depth > N
// returns backgroundColor immediately, even if the ray would hit geometry.

RC_GTEST_PROP(RayRecursionDepthProps, DepthExceededReturnsBackground, ()) {
    int maxBounces = *rc::gen::inRange(0, 11);
    // depth must be strictly greater than maxBounces
    int depth = maxBounces + *rc::gen::inRange(1, 6);

    Scene scene = makeSceneWithBox();

    // Ray aimed directly at the box
    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    Color result = RayTracer::traceRay(ray, scene, depth, maxBounces);

    RC_ASSERT(std::abs(result.r - scene.backgroundColor.r) < 1e-5f);
    RC_ASSERT(std::abs(result.g - scene.backgroundColor.g) < 1e-5f);
    RC_ASSERT(std::abs(result.b - scene.backgroundColor.b) < 1e-5f);
}

// ── Sub-property 3: Zero bounces — no reflection component ──────────────────
//
// **Validates: Requirements 3.5, 3.6, 6.3**
//
// When maxBounces = 0, the result should be pure direct shading with no
// reflection component. We verify this by comparing traceRay(depth=0,
// maxBounces=0) with the direct shade() result for the same hit.

RC_GTEST_PROP(RayRecursionDepthProps, ZeroBouncesNoReflection, ()) {
    Scene scene = makeSceneWithBox();

    // Generate a random ray origin behind the box, aimed at it
    float ox = *rc::gen::inRange(-10, 11) / 10.0f;  // small offset
    float oy = *rc::gen::inRange(-10, 11) / 10.0f;
    Vec3 origin(ox, oy, -10.0f);
    Vec3 dir = (Vec3(0, 0, 0) - origin).normalize();
    RC_PRE(dir.length() > 0.5f);

    Ray ray(origin, dir);

    // First check that the ray actually hits the box
    HitResult hit = intersectScene(ray, scene);
    RC_PRE(hit.hit);

    // Compute direct shade result
    Vec3 viewDir = (ray.origin - hit.point).normalize();
    ShadingParams params;
    Color directShade = shade(hit, viewDir, scene.light, scene, params);
    directShade = directShade.clamp();

    // traceRay with maxBounces=0 should produce the same result
    Color traceResult = RayTracer::traceRay(ray, scene, 0, 0, params);

    constexpr float TOL = 1e-4f;
    RC_ASSERT(std::abs(traceResult.r - directShade.r) < TOL);
    RC_ASSERT(std::abs(traceResult.g - directShade.g) < TOL);
    RC_ASSERT(std::abs(traceResult.b - directShade.b) < TOL);
}

// ── Property 10: Transparent Pixel Pass-Through ─────────────────────────────
//
// **Validates: Requirements 3.7**
//
// Property 10: 透明像素穿透
// For any scene where the outer layer mesh has alpha = 0 pixels, a ray fired
// from the camera should pass through the transparent outer layer and
// intersect the inner layer mesh. The resulting color should be based on the
// inner layer's texture (not the background color).

// Create a BodyPartTexture where all 6 faces are fully transparent (alpha=0).
static BodyPartTexture makeTransparentBodyPartTexture(int faceW, int faceH) {
    std::vector<Color> pixels(faceW * faceH, Color(0.0f, 0.0f, 0.0f, 0.0f));
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

// Sub-property 1: Ray passes through transparent outer layer and hits inner
//
// **Validates: Requirements 3.7**
//
// For a scene with an opaque inner box and a transparent outer box (offset=0.5),
// intersectScene should return a hit on the inner layer, not the outer layer.

RC_GTEST_PROP(TransparentPixelPassThroughProps, HitInnerNotOuter, ()) {
    Scene scene;
    scene.backgroundColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    scene.light.position = Vec3(10, 10, -10);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    // Random opaque color for the inner layer
    float r = *rc::gen::inRange(10, 256) / 255.0f;
    float g = *rc::gen::inRange(10, 256) / 255.0f;
    float b = *rc::gen::inRange(10, 256) / 255.0f;
    std::vector<Color> innerPixels(4 * 4, Color(r, g, b, 1.0f));
    TextureRegion innerFace(4, 4, innerPixels);
    BodyPartTexture innerTex;
    innerTex.top = innerFace;
    innerTex.bottom = innerFace;
    innerTex.front = innerFace;
    innerTex.back = innerFace;
    innerTex.left = innerFace;
    innerTex.right = innerFace;

    // Inner box at origin, size 4x4x4, offset=0
    Mesh innerBox = MeshBuilder::buildBox(innerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.0f);
    innerBox.isOuterLayer = false;

    // Outer box at same position, size 4x4x4, offset=0.5 (fully transparent)
    BodyPartTexture outerTex = makeTransparentBodyPartTexture(4, 4);
    Mesh outerBox = MeshBuilder::buildBox(outerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.5f);
    outerBox.isOuterLayer = true;

    scene.meshes.push_back(innerBox);
    scene.meshes.push_back(outerBox);

    // Fire a ray from the front, aimed at the center of the box
    // Randomize the ray origin slightly to test different angles
    float ox = *rc::gen::inRange(-10, 11) / 10.0f;
    float oy = *rc::gen::inRange(-10, 11) / 10.0f;
    Vec3 origin(ox, oy, -10.0f);
    Vec3 dir = (Vec3(0, 0, 0) - origin).normalize();
    RC_PRE(dir.length() > 0.5f);

    Ray ray(origin, dir);

    HitResult hit = intersectScene(ray, scene);

    // The ray should hit the inner layer (pass through transparent outer)
    RC_ASSERT(hit.hit);
    RC_ASSERT(!hit.isOuterLayer);
    // The texture color should match the inner layer's opaque color
    RC_ASSERT(hit.textureColor.a > 0.0f);
}

// Sub-property 2: traceRay color is based on inner layer, not background
//
// **Validates: Requirements 3.7**
//
// When the outer layer is fully transparent, traceRay should return a color
// based on the inner layer's texture, not the background color.

RC_GTEST_PROP(TransparentPixelPassThroughProps, TraceRayUsesInnerColor, ()) {
    Scene scene;
    scene.backgroundColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    scene.light.position = Vec3(10, 10, -10);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    // Use a distinct opaque color for the inner layer
    float r = *rc::gen::inRange(50, 256) / 255.0f;
    float g = *rc::gen::inRange(50, 256) / 255.0f;
    float b = *rc::gen::inRange(50, 256) / 255.0f;
    std::vector<Color> innerPixels(4 * 4, Color(r, g, b, 1.0f));
    TextureRegion innerFace(4, 4, innerPixels);
    BodyPartTexture innerTex;
    innerTex.top = innerFace;
    innerTex.bottom = innerFace;
    innerTex.front = innerFace;
    innerTex.back = innerFace;
    innerTex.left = innerFace;
    innerTex.right = innerFace;

    Mesh innerBox = MeshBuilder::buildBox(innerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.0f);
    innerBox.isOuterLayer = false;

    BodyPartTexture outerTex = makeTransparentBodyPartTexture(4, 4);
    Mesh outerBox = MeshBuilder::buildBox(outerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.5f);
    outerBox.isOuterLayer = true;

    scene.meshes.push_back(innerBox);
    scene.meshes.push_back(outerBox);

    // Fire ray at the box center
    Ray ray(Vec3(0, 0, -10), Vec3(0, 0, 1));

    // traceRay with the transparent outer layer should NOT return background
    Color result = RayTracer::traceRay(ray, scene, 0, 0);

    // The result should differ from the background color (it should be shaded
    // based on the inner layer's texture)
    bool isBackground =
        std::abs(result.r - scene.backgroundColor.r) < 1e-5f &&
        std::abs(result.g - scene.backgroundColor.g) < 1e-5f &&
        std::abs(result.b - scene.backgroundColor.b) < 1e-5f;
    RC_ASSERT(!isBackground);
}

// Sub-property 3: Transparent outer layer does not affect inner hit
//
// **Validates: Requirements 3.7**
//
// The intersection result for the inner layer should be the same whether or
// not a transparent outer layer is present.

RC_GTEST_PROP(TransparentPixelPassThroughProps, TransparentOuterDoesNotAffectInnerHit, ()) {
    // Random opaque color
    float r = *rc::gen::inRange(10, 256) / 255.0f;
    float g = *rc::gen::inRange(10, 256) / 255.0f;
    float b = *rc::gen::inRange(10, 256) / 255.0f;
    std::vector<Color> innerPixels(4 * 4, Color(r, g, b, 1.0f));
    TextureRegion innerFace(4, 4, innerPixels);
    BodyPartTexture innerTex;
    innerTex.top = innerFace;
    innerTex.bottom = innerFace;
    innerTex.front = innerFace;
    innerTex.back = innerFace;
    innerTex.left = innerFace;
    innerTex.right = innerFace;

    // Scene with only inner box
    Scene sceneInnerOnly;
    sceneInnerOnly.backgroundColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    sceneInnerOnly.light.position = Vec3(10, 10, -10);
    sceneInnerOnly.light.color = Color(1, 1, 1, 1);
    sceneInnerOnly.light.intensity = 1.0f;

    Mesh innerBox = MeshBuilder::buildBox(innerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.0f);
    innerBox.isOuterLayer = false;
    sceneInnerOnly.meshes.push_back(innerBox);

    // Scene with inner + transparent outer
    Scene sceneBoth = sceneInnerOnly;
    BodyPartTexture outerTex = makeTransparentBodyPartTexture(4, 4);
    Mesh outerBox = MeshBuilder::buildBox(outerTex, Vec3(0, 0, 0), Vec3(4, 4, 4), 0.5f);
    outerBox.isOuterLayer = true;
    sceneBoth.meshes.push_back(outerBox);

    // Random ray aimed at the box
    float ox = *rc::gen::inRange(-10, 11) / 10.0f;
    float oy = *rc::gen::inRange(-10, 11) / 10.0f;
    Vec3 origin(ox, oy, -10.0f);
    Vec3 dir = (Vec3(0, 0, 0) - origin).normalize();
    RC_PRE(dir.length() > 0.5f);
    Ray ray(origin, dir);

    HitResult hitInnerOnly = intersectScene(ray, sceneInnerOnly);
    HitResult hitBoth = intersectScene(ray, sceneBoth);

    // Both should hit (or both miss)
    RC_ASSERT(hitInnerOnly.hit == hitBoth.hit);

    if (hitInnerOnly.hit) {
        // Same intersection point (within tolerance)
        constexpr float TOL = 1e-4f;
        RC_ASSERT(std::abs(hitInnerOnly.t - hitBoth.t) < TOL);
        RC_ASSERT(!hitBoth.isOuterLayer);
        // Same texture color
        RC_ASSERT(std::abs(hitInnerOnly.textureColor.r - hitBoth.textureColor.r) < TOL);
        RC_ASSERT(std::abs(hitInnerOnly.textureColor.g - hitBoth.textureColor.g) < TOL);
        RC_ASSERT(std::abs(hitInnerOnly.textureColor.b - hitBoth.textureColor.b) < TOL);
    }
}
