/**
 * Property-based tests for Blinn-Phong shading.
 *
 * **Validates: Requirements 3.2, 3.4**
 *
 * Property 7: Blinn-Phong Shading Correctness (Blinn-Phong 着色正确性)
 * For any valid normal vector N, light direction L, view direction V, and
 * texture color, the shade() function output should equal the Blinn-Phong
 * formula:
 *   ambient * textureColor
 *   + kd * max(0, dot(N, L)) * textureColor * lightColor
 *   + ks * pow(max(0, dot(N, H)), shininess) * lightColor
 * where H = normalize(L + V).
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <cmath>
#include <algorithm>

#include "raytracer/shading.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "math/vec3.h"
#include "math/color.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

// Generate a random unit vector by sampling xyz in [-1, 1] and normalizing.
// Uses RC_PRE to discard zero-length vectors.
static Vec3 genUnitVector() {
    float x = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float y = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float z = *rc::gen::inRange(-1000, 1001) / 1000.0f;
    float len = std::sqrt(x * x + y * y + z * z);
    RC_PRE(len > 0.01f);
    return Vec3(x / len, y / len, z / len);
}

// Generate a random color with components in [0, 1] and alpha > 0.
static Color genTextureColor() {
    float r = *rc::gen::inRange(0, 256) / 255.0f;
    float g = *rc::gen::inRange(0, 256) / 255.0f;
    float b = *rc::gen::inRange(0, 256) / 255.0f;
    float a = *rc::gen::inRange(1, 256) / 255.0f;  // alpha > 0
    return Color(r, g, b, a);
}

// Create a HitResult at the origin with given normal and texture color.
static HitResult makeHit(const Vec3& normal, const Color& texColor) {
    HitResult h;
    h.hit = true;
    h.t = 1.0f;
    h.point = Vec3(0.0f, 0.0f, 0.0f);
    h.normal = normal;
    h.textureColor = texColor;
    h.isOuterLayer = false;
    return h;
}

// ── Property 7: Blinn-Phong Shading Correctness ─────────────────────────────
//
// **Validates: Requirements 3.2, 3.4**
//
// For any valid normal N, light position (placed so dot(N, L) > 0),
// view direction V, and texture color, shade() with an empty scene
// (no shadow) should match the independently computed Blinn-Phong result
// within tolerance.

RC_GTEST_PROP(ShadingProps, BlinnPhongShadingCorrectness, ()) {
    // Generate random inputs
    Vec3 N = genUnitVector();
    Vec3 V = genUnitVector();
    Color texColor = genTextureColor();

    // Generate a light position that is in front of the surface (dot(N, L) > 0).
    // Place the light along N + some random perturbation, then ensure dot(N, L) > 0.
    float dist = *rc::gen::inRange(1, 100) / 10.0f;  // distance 0.1 to 10.0
    Vec3 perturbation(
        *rc::gen::inRange(-500, 501) / 1000.0f,
        *rc::gen::inRange(-500, 501) / 1000.0f,
        *rc::gen::inRange(-500, 501) / 1000.0f
    );
    Vec3 lightPos = N * dist + perturbation;

    // Compute L from hit point (origin) to light
    Vec3 L = lightPos.normalize();
    float lenLP = lightPos.length();
    RC_PRE(lenLP > 0.01f);
    L = lightPos / lenLP;

    // Ensure the light is in front of the surface
    float NdotL = N.dot(L);
    RC_PRE(NdotL > 0.01f);

    // Create an empty scene (no meshes → no shadow)
    Scene scene;
    scene.light.position = lightPos;
    scene.light.color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    scene.light.intensity = 1.0f;
    scene.backgroundColor = Color(0.0f, 0.0f, 0.0f, 1.0f);

    HitResult hit = makeHit(N, texColor);
    ShadingParams params;  // defaults: kd=0.7, ks=0.3, ambient=0.1, shininess=32

    // Call shade()
    Color actual = shade(hit, V, scene.light, scene, params);

    // Independently compute expected Blinn-Phong result
    // ambient = 0.1 * texColor
    float ambR = params.ambient * texColor.r;
    float ambG = params.ambient * texColor.g;
    float ambB = params.ambient * texColor.b;

    // diffuse = kd * max(0, dot(N, L)) * texColor * lightColor
    float diffFactor = params.kd * std::max(0.0f, N.dot(L));
    float diffR = diffFactor * texColor.r * scene.light.color.r;
    float diffG = diffFactor * texColor.g * scene.light.color.g;
    float diffB = diffFactor * texColor.b * scene.light.color.b;

    // H = normalize(L + V)
    Vec3 Hv = (L + V);
    float hLen = Hv.length();
    RC_PRE(hLen > 0.001f);
    Hv = Hv / hLen;

    float NdotH = std::max(0.0f, N.dot(Hv));
    float specFactor = params.ks * std::pow(NdotH, params.shininess);

    float specR = specFactor * scene.light.color.r;
    float specG = specFactor * scene.light.color.g;
    float specB = specFactor * scene.light.color.b;

    // expected = clamp(ambient + diffuse + specular)
    float expR = std::clamp(ambR + diffR + specR, 0.0f, 1.0f);
    float expG = std::clamp(ambG + diffG + specG, 0.0f, 1.0f);
    float expB = std::clamp(ambB + diffB + specB, 0.0f, 1.0f);

    // Verify within tolerance
    constexpr float TOL = 1e-3f;
    RC_ASSERT(std::abs(actual.r - expR) < TOL);
    RC_ASSERT(std::abs(actual.g - expG) < TOL);
    RC_ASSERT(std::abs(actual.b - expB) < TOL);
}

// ── Property 8: Shadow Ray Correctness ──────────────────────────────────────
//
// **Validates: Requirements 3.3**
//
// For any intersection point P and light position in a scene, if there is a
// mesh blocking the path from P to the light, isInShadow should return true;
// if the path is unobstructed, isInShadow should return false.

#include "raytracer/intersection.h"
#include "scene/mesh_builder.h"
#include "skin/texture_region.h"

// Create a BodyPartTexture where all 6 faces are filled with an opaque color.
static BodyPartTexture makeOpaqueBodyPartTexture(int faceW, int faceH) {
    std::vector<Color> pixels(faceW * faceH, Color(1.0f, 0.0f, 0.0f, 1.0f));
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

// Sub-property 1: No blocker → not in shadow
// With no meshes in the scene, isInShadow should always return false.
RC_GTEST_PROP(ShadowRayProps, NoBlockerNotInShadow, ()) {
    // Generate a random surface point
    float px = *rc::gen::inRange(-100, 101) / 10.0f;
    float py = *rc::gen::inRange(-100, 101) / 10.0f;
    float pz = *rc::gen::inRange(-100, 101) / 10.0f;
    Vec3 point(px, py, pz);

    // Generate a random unit normal
    Vec3 normal = genUnitVector();

    // Generate a light position away from the point
    float lx = *rc::gen::inRange(-200, 201) / 10.0f;
    float ly = *rc::gen::inRange(-200, 201) / 10.0f;
    float lz = *rc::gen::inRange(-200, 201) / 10.0f;
    Vec3 lightPos(lx, ly, lz);

    // Ensure light is not at the same position as the point
    float dist = (lightPos - point).length();
    RC_PRE(dist > 0.1f);

    // Empty scene — no meshes to block
    Scene scene;
    scene.light.position = lightPos;
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    bool shadow = isInShadow(point, normal, lightPos, scene);
    RC_ASSERT(!shadow);
}

// Sub-property 2: Blocker between P and light → in shadow
// Place a box mesh on the line between P and the light, large enough to block.
RC_GTEST_PROP(ShadowRayProps, BlockerBetweenPointAndLight, ()) {
    // Use a fixed surface point at origin with normal +Y for simplicity,
    // then randomize the light direction.
    Vec3 point(0.0f, 0.0f, 0.0f);
    Vec3 normal(0.0f, 1.0f, 0.0f);

    // Generate a light position in the upper hemisphere (so the shadow ray
    // goes upward, matching the normal direction for a valid offset).
    float lx = *rc::gen::inRange(-50, 51) / 10.0f;   // -5 to 5
    float ly = *rc::gen::inRange(100, 301) / 10.0f;   // 10 to 30 (well above)
    float lz = *rc::gen::inRange(-50, 51) / 10.0f;    // -5 to 5
    Vec3 lightPos(lx, ly, lz);

    // Place the blocker at the midpoint between point and light
    Vec3 midpoint = (point + lightPos) * 0.5f;

    // Make the blocker box large enough to reliably block the ray.
    // The box size is 4x4x4 centered at the midpoint.
    float boxSize = 4.0f;
    BodyPartTexture blockerTex = makeOpaqueBodyPartTexture(4, 4);
    Mesh blocker = MeshBuilder::buildBox(
        blockerTex,
        midpoint,
        Vec3(boxSize, boxSize, boxSize),
        0.0f
    );

    Scene scene;
    scene.meshes.push_back(blocker);
    scene.light.position = lightPos;
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    bool shadow = isInShadow(point, normal, lightPos, scene);
    RC_ASSERT(shadow);
}

