#pragma once

#include "math/vec3.h"
#include "math/color.h"

struct TextureRegion;  // forward declaration

// 三角形面（盒体的每个面由2个三角形组成）
struct Triangle {
    Vec3 v0, v1, v2;       // 顶点
    Vec3 normal;            // 面法线
    float u0, v0_uv;       // 顶点0的UV坐标
    float u1, v1_uv;       // 顶点1的UV坐标
    float u2, v2_uv;       // 顶点2的UV坐标
    const TextureRegion* texture = nullptr;  // 纹理引用
};

// 求交结果
struct HitResult {
    bool hit = false;
    float t = 0.0f;        // 光线参数
    Vec3 point;             // 交点
    Vec3 normal;            // 法线
    Color textureColor;     // 纹理颜色（含 alpha）
    bool isOuterLayer = false;
};
