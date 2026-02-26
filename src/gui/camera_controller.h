#pragma once
#include "math/vec3.h"
#include <cmath>

enum class CameraMode {
    Orbit,
    Free
};

class CameraController {
public:
    enum MoveDirection {
        Forward  = 1 << 0,
        Backward = 1 << 1,
        Left     = 1 << 2,
        Right    = 1 << 3
    };

    CameraController();

    void setPosition(const Vec3& pos);
    void setYawPitch(float yaw, float pitch);
    void setMoveFlag(MoveDirection dir, bool active);
    void handleMouseMove(float deltaX, float deltaY);
    void update();

    Vec3 position() const;
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 target() const;
    float yaw() const;
    float pitch() const;

    void setMoveSpeed(float speed);
    void setMouseSensitivity(float sensitivity);

private:
    Vec3 position_;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float moveSpeed_ = 0.5f;
    float mouseSensitivity_ = 0.15f;
    unsigned int moveFlags_ = 0;

    Vec3 computeForward() const;
    Vec3 computeRight() const;
};
