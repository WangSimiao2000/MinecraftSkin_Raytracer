/**
 * Property-based tests for CameraController.
 *
 * Tests the core camera movement, mouse look, and state invariants
 * using RapidCheck for randomized input generation.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "gui/camera_controller.h"
#include "math/vec3.h"

#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr float TOL = 1e-4f;

static float toRadians(float degrees) {
    return degrees * static_cast<float>(M_PI) / 180.0f;
}

// ── Property 1: 方向移动正确性 ──────────────────────────────────────────────
//
// **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.6**
//
// For any camera state (position, yaw, pitch) and for any single WASD
// direction key, calling update() with that key active SHALL move the camera
// position by exactly moveSpeed units along the corresponding direction vector.

RC_GTEST_PROP(CameraControllerProps, DirectionMovementCorrectness, ()) {
    // Generate random position
    float px = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float py = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float pz = *rc::gen::inRange(-1000, 1001) / 10.0f;

    // Generate yaw in [-180, 180] and pitch in [-89, 89] using integers / 10
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;

    // Test each direction separately
    CameraController::MoveDirection dirs[] = {
        CameraController::Forward,
        CameraController::Backward,
        CameraController::Left,
        CameraController::Right
    };

    for (auto dir : dirs) {
        CameraController cam;
        cam.setPosition(Vec3(px, py, pz));
        cam.setYawPitch(yaw, pitch);

        Vec3 posBefore = cam.position();
        Vec3 fwd = cam.forward();
        Vec3 rgt = cam.right();

        cam.setMoveFlag(dir, true);
        cam.update();

        Vec3 posAfter = cam.position();
        Vec3 displacement = posAfter - posBefore;

        // Expected direction for each key
        Vec3 expected;
        if (dir == CameraController::Forward)       expected = fwd;
        else if (dir == CameraController::Backward)  expected = fwd * -1.0f;
        else if (dir == CameraController::Left)      expected = rgt * -1.0f;
        else /* Right */                             expected = rgt;

        // Displacement should be moveSpeed (0.5) along expected direction
        Vec3 expectedDisplacement = expected * 0.5f;

        RC_ASSERT(std::abs(displacement.x - expectedDisplacement.x) < TOL);
        RC_ASSERT(std::abs(displacement.y - expectedDisplacement.y) < TOL);
        RC_ASSERT(std::abs(displacement.z - expectedDisplacement.z) < TOL);

        // Magnitude should be exactly moveSpeed
        RC_ASSERT(std::abs(displacement.length() - 0.5f) < TOL);
    }
}


// ── Property 2: 鼠标朝向更新 ────────────────────────────────────────────────
//
// **Validates: Requirements 3.1, 3.2, 3.4**
//
// For any camera state and for any mouse delta (deltaX, deltaY), calling
// handleMouseMove(deltaX, deltaY) SHALL change yaw by deltaX * sensitivity
// and change pitch by deltaY * sensitivity (before clamping).

RC_GTEST_PROP(CameraControllerProps, MouseOrientationUpdate, ()) {
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;

    // Generate small deltas so pitch won't hit the clamp boundary
    // With sensitivity 0.15, delta of ±50 gives ±7.5 degrees change
    // Restrict initial pitch to [-80, 80] and delta to [-50, 50] to stay unclamped
    RC_PRE(pitch >= -80.0f && pitch <= 80.0f);
    float deltaX = *rc::gen::inRange(-500, 501) / 10.0f;
    float deltaY = *rc::gen::inRange(-500, 501) / 10.0f;

    float sensitivity = 0.15f;
    float expectedNewPitch = pitch - deltaY * sensitivity;  // negated: mouse-down = look-down
    RC_PRE(expectedNewPitch > -89.0f && expectedNewPitch < 89.0f);

    CameraController cam;
    cam.setYawPitch(yaw, pitch);
    cam.handleMouseMove(deltaX, deltaY);

    float expectedYaw = yaw + deltaX * sensitivity;

    RC_ASSERT(std::abs(cam.yaw() - expectedYaw) < TOL);
    RC_ASSERT(std::abs(cam.pitch() - expectedNewPitch) < TOL);
}

// ── Property 3: Pitch 角度限制不变量 ────────────────────────────────────────
//
// **Validates: Requirements 3.3**
//
// For any sequence of handleMouseMove calls with arbitrary delta values,
// the resulting pitch angle SHALL always remain within [-89, 89] degrees.

RC_GTEST_PROP(CameraControllerProps, PitchClampInvariant, ()) {
    CameraController cam;

    // Generate a random initial state
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;
    cam.setYawPitch(yaw, pitch);

    // Apply a random sequence of mouse moves (5 to 20 moves)
    int numMoves = *rc::gen::inRange(5, 21);
    for (int i = 0; i < numMoves; ++i) {
        float dx = *rc::gen::inRange(-5000, 5001) / 10.0f;
        float dy = *rc::gen::inRange(-5000, 5001) / 10.0f;
        cam.handleMouseMove(dx, dy);

        // Pitch must always be in [-89, 89] after every move
        RC_ASSERT(cam.pitch() >= -89.0f - TOL);
        RC_ASSERT(cam.pitch() <=  89.0f + TOL);
    }
}

// ── Property 4: 相机基向量正交归一性 ────────────────────────────────────────
//
// **Validates: Requirements 4.1, 4.2**
//
// For any valid yaw and pitch (pitch in [-89, 89]), the computed forward
// vector SHALL be a unit vector, the computed right vector SHALL be a unit
// vector, and forward and right SHALL be perpendicular (dot product ≈ 0.0).

RC_GTEST_PROP(CameraControllerProps, BasisVectorsOrthonormal, ()) {
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;

    CameraController cam;
    cam.setYawPitch(yaw, pitch);

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();

    // Forward should be unit length
    RC_ASSERT(std::abs(fwd.length() - 1.0f) < TOL);

    // Right should be unit length
    RC_ASSERT(std::abs(rgt.length() - 1.0f) < TOL);

    // Forward and right should be perpendicular
    RC_ASSERT(std::abs(fwd.dot(rgt)) < TOL);
}

// ── Property 5: Target 等于 Position 加 Forward ─────────────────────────────
//
// **Validates: Requirements 4.4**
//
// For any camera state, target() SHALL equal position() + forward().

RC_GTEST_PROP(CameraControllerProps, TargetEqualsPositionPlusForward, ()) {
    float px = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float py = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float pz = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;

    CameraController cam;
    cam.setPosition(Vec3(px, py, pz));
    cam.setYawPitch(yaw, pitch);

    Vec3 expected = cam.position() + cam.forward();
    Vec3 actual   = cam.target();

    RC_ASSERT(std::abs(actual.x - expected.x) < TOL);
    RC_ASSERT(std::abs(actual.y - expected.y) < TOL);
    RC_ASSERT(std::abs(actual.z - expected.z) < TOL);
}

// ── Property 6: 多键同时按下时移动速度一致 ──────────────────────────────────
//
// **Validates: Requirements 6.2**
//
// For any non-empty combination of WASD keys pressed simultaneously, after
// update(), the displacement magnitude should equal moveSpeed. Exclude
// opposite-only pairs (W+S only, A+D only) since those cancel to zero.

RC_GTEST_PROP(CameraControllerProps, MultiKeyMoveSpeedConsistent, ()) {
    float px = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float py = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float pz = *rc::gen::inRange(-1000, 1001) / 10.0f;
    float yaw   = *rc::gen::inRange(-1800, 1801) / 10.0f;
    float pitch = *rc::gen::inRange(-890, 891) / 10.0f;

    // Generate a random non-empty subset of directions
    bool w = *rc::gen::arbitrary<bool>();
    bool s = *rc::gen::arbitrary<bool>();
    bool a = *rc::gen::arbitrary<bool>();
    bool d = *rc::gen::arbitrary<bool>();

    // At least one key must be pressed
    RC_PRE(w || s || a || d);

    // Exclude opposite-only pairs that cancel out to zero displacement
    // W+S only (no A or D)
    bool forwardBackwardOnly = (w && s && !a && !d);
    // A+D only (no W or S)
    bool leftRightOnly = (!w && !s && a && d);
    // All four cancel out too
    bool allFour = (w && s && a && d);
    RC_PRE(!forwardBackwardOnly && !leftRightOnly && !allFour);

    CameraController cam;
    cam.setPosition(Vec3(px, py, pz));
    cam.setYawPitch(yaw, pitch);

    if (w) cam.setMoveFlag(CameraController::Forward, true);
    if (s) cam.setMoveFlag(CameraController::Backward, true);
    if (a) cam.setMoveFlag(CameraController::Left, true);
    if (d) cam.setMoveFlag(CameraController::Right, true);

    Vec3 posBefore = cam.position();
    cam.update();
    Vec3 posAfter = cam.position();

    Vec3 displacement = posAfter - posBefore;
    float speed = displacement.length();

    RC_ASSERT(std::abs(speed - 0.5f) < TOL);
}
