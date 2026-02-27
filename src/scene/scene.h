#pragma once

#include <vector>
#include "math/vec3.h"
#include "math/color.h"
#include "math/ray.h"
#include "scene/mesh.h"

// 光源
struct Light {
    Vec3 position;
    Color color;
    float intensity = 1.0f;
    float radius = 3.0f;  // 面光源半径（用于软阴影）
};

// 相机
struct Camera {
    Vec3 position;
    Vec3 target;
    Vec3 up;
    float fov = 60.0f;  // 视场角（度）

    // 根据像素坐标生成光线（将在 Task 5.5 中实现）
    Ray generateRay(float u, float v, float aspectRatio) const;
};

// 场景
struct Scene {
    std::vector<Mesh> meshes;
    Light light;
    Camera camera;
    Color backgroundColor;
};
