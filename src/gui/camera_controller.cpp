#include "gui/camera_controller.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float toRadians(float degrees) {
    return degrees * static_cast<float>(M_PI) / 180.0f;
}

CameraController::CameraController()
    : position_(0.0f, 18.0f, 60.0f)
{
}

void CameraController::setPosition(const Vec3& pos) {
    position_ = pos;
}

void CameraController::setYawPitch(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = pitch;
}

void CameraController::setMoveFlag(MoveDirection dir, bool active) {
    if (active) {
        moveFlags_ |= dir;
    } else {
        moveFlags_ &= ~dir;
    }
}

void CameraController::handleMouseMove(float deltaX, float deltaY) {
    yaw_ += deltaX * mouseSensitivity_;
    pitch_ -= deltaY * mouseSensitivity_;  // negate: mouse-down = look-down
    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
}

void CameraController::update() {
    Vec3 direction(0.0f, 0.0f, 0.0f);

    if (moveFlags_ & Forward)
        direction += computeForward();
    if (moveFlags_ & Backward)
        direction -= computeForward();
    if (moveFlags_ & Left)
        direction -= computeRight();
    if (moveFlags_ & Right)
        direction += computeRight();

    float len = direction.length();
    if (len > 0.0f) {
        direction = direction.normalize();
        position_ += direction * moveSpeed_;
    }
}

Vec3 CameraController::position() const {
    return position_;
}

Vec3 CameraController::forward() const {
    return computeForward();
}

Vec3 CameraController::right() const {
    return computeRight();
}

Vec3 CameraController::target() const {
    return position_ + computeForward();
}

float CameraController::yaw() const {
    return yaw_;
}

float CameraController::pitch() const {
    return pitch_;
}

void CameraController::setMoveSpeed(float speed) {
    moveSpeed_ = speed;
}

void CameraController::setMouseSensitivity(float sensitivity) {
    mouseSensitivity_ = sensitivity;
}

Vec3 CameraController::computeForward() const {
    float yawRad = toRadians(yaw_);
    float pitchRad = toRadians(pitch_);
    return Vec3(
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        -std::cos(pitchRad) * std::cos(yawRad)
    );
}

Vec3 CameraController::computeRight() const {
    Vec3 fwd = computeForward();
    Vec3 up(0.0f, 1.0f, 0.0f);
    return fwd.cross(up).normalize();
}
