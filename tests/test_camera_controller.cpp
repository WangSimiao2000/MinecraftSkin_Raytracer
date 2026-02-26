#include <gtest/gtest.h>
#include "gui/camera_controller.h"
#include <cmath>

static constexpr float kTol = 1e-4f;

// 1. DefaultConstructorState
TEST(CameraController, DefaultConstructorState) {
    CameraController cc;
    EXPECT_NEAR(cc.position().x, 0.0f, kTol);
    EXPECT_NEAR(cc.position().y, 18.0f, kTol);
    EXPECT_NEAR(cc.position().z, 60.0f, kTol);
    EXPECT_NEAR(cc.yaw(), 0.0f, kTol);
    EXPECT_NEAR(cc.pitch(), 0.0f, kTol);
}

// 2. ForwardMovement: W key at default yaw/pitch moves by (0, 0, -0.5)
TEST(CameraController, ForwardMovement) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.setMoveFlag(CameraController::Forward, true);
    cc.update();
    Vec3 after = cc.position();
    EXPECT_NEAR(after.x - before.x, 0.0f, kTol);
    EXPECT_NEAR(after.y - before.y, 0.0f, kTol);
    EXPECT_NEAR(after.z - before.z, -0.5f, kTol);
}

// 3. BackwardMovement: S key moves by (0, 0, 0.5)
TEST(CameraController, BackwardMovement) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.setMoveFlag(CameraController::Backward, true);
    cc.update();
    Vec3 after = cc.position();
    EXPECT_NEAR(after.x - before.x, 0.0f, kTol);
    EXPECT_NEAR(after.y - before.y, 0.0f, kTol);
    EXPECT_NEAR(after.z - before.z, 0.5f, kTol);
}

// 4. LeftMovement: A key moves by (-0.5, 0, 0)
TEST(CameraController, LeftMovement) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.setMoveFlag(CameraController::Left, true);
    cc.update();
    Vec3 after = cc.position();
    EXPECT_NEAR(after.x - before.x, -0.5f, kTol);
    EXPECT_NEAR(after.y - before.y, 0.0f, kTol);
    EXPECT_NEAR(after.z - before.z, 0.0f, kTol);
}

// 5. RightMovement: D key moves by (0.5, 0, 0)
TEST(CameraController, RightMovement) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.setMoveFlag(CameraController::Right, true);
    cc.update();
    Vec3 after = cc.position();
    EXPECT_NEAR(after.x - before.x, 0.5f, kTol);
    EXPECT_NEAR(after.y - before.y, 0.0f, kTol);
    EXPECT_NEAR(after.z - before.z, 0.0f, kTol);
}

// 6. PitchClampAtMax: pitch=85, handleMouseMove(0, -100) -> pitch clamped to 89
//    (negative deltaY = mouse up = look up = increase pitch)
TEST(CameraController, PitchClampAtMax) {
    CameraController cc;
    cc.setYawPitch(0.0f, 85.0f);
    cc.handleMouseMove(0.0f, -100.0f); // pitch -= (-100) * 0.15 = +15 -> 85+15=100 -> clamped to 89
    EXPECT_NEAR(cc.pitch(), 89.0f, kTol);
}

// 7. PitchClampAtMin: pitch=-85, handleMouseMove(0, 100) -> pitch clamped to -89
//    (positive deltaY = mouse down = look down = decrease pitch)
TEST(CameraController, PitchClampAtMin) {
    CameraController cc;
    cc.setYawPitch(0.0f, -85.0f);
    cc.handleMouseMove(0.0f, 100.0f); // pitch -= 100 * 0.15 = -15 -> -85-15=-100 -> clamped to -89
    EXPECT_NEAR(cc.pitch(), -89.0f, kTol);
}

// 8. DiagonalMoveNormalized: W+D displacement magnitude should be 0.5
TEST(CameraController, DiagonalMoveNormalized) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.setMoveFlag(CameraController::Forward, true);
    cc.setMoveFlag(CameraController::Right, true);
    cc.update();
    Vec3 after = cc.position();
    Vec3 delta = after - before;
    float mag = delta.length();
    EXPECT_NEAR(mag, 0.5f, kTol);
}

// 9. HandleMouseMoveValues: handleMouseMove(10, 20) -> yaw=1.5, pitch=-3.0
//    (deltaY negated: mouse down = look down)
TEST(CameraController, HandleMouseMoveValues) {
    CameraController cc;
    cc.handleMouseMove(10.0f, 20.0f);
    EXPECT_NEAR(cc.yaw(), 1.5f, kTol);    // 10 * 0.15
    EXPECT_NEAR(cc.pitch(), -3.0f, kTol);  // -(20 * 0.15)
}

// 10. ZeroDeltaMouseMove: handleMouseMove(0, 0) should not change yaw or pitch
TEST(CameraController, ZeroDeltaMouseMove) {
    CameraController cc;
    float yawBefore = cc.yaw();
    float pitchBefore = cc.pitch();
    cc.handleMouseMove(0.0f, 0.0f);
    EXPECT_NEAR(cc.yaw(), yawBefore, kTol);
    EXPECT_NEAR(cc.pitch(), pitchBefore, kTol);
}

// 11. SetPositionWorks: setPosition then position() returns the set value
TEST(CameraController, SetPositionWorks) {
    CameraController cc;
    cc.setPosition(Vec3(1.0f, 2.0f, 3.0f));
    EXPECT_NEAR(cc.position().x, 1.0f, kTol);
    EXPECT_NEAR(cc.position().y, 2.0f, kTol);
    EXPECT_NEAR(cc.position().z, 3.0f, kTol);
}

// 12. NoKeysNoMovement: with no keys pressed, update() should not change position
TEST(CameraController, NoKeysNoMovement) {
    CameraController cc;
    Vec3 before = cc.position();
    cc.update();
    Vec3 after = cc.position();
    EXPECT_NEAR(after.x, before.x, kTol);
    EXPECT_NEAR(after.y, before.y, kTol);
    EXPECT_NEAR(after.z, before.z, kTol);
}
