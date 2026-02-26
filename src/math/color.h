#pragma once

#include <algorithm>

struct Color {
    float r, g, b, a;

    Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    // Arithmetic operators
    Color operator+(const Color& c) const { return {r + c.r, g + c.g, b + c.b, a + c.a}; }
    Color operator-(const Color& c) const { return {r - c.r, g - c.g, b - c.b, a - c.a}; }

    Color& operator+=(const Color& c) { r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
    Color& operator-=(const Color& c) { r -= c.r; g -= c.g; b -= c.b; a -= c.a; return *this; }

    // Scalar operations
    Color operator*(float s) const { return {r * s, g * s, b * s, a * s}; }
    Color operator/(float s) const { float inv = 1.0f / s; return {r * inv, g * inv, b * inv, a * inv}; }

    Color& operator*=(float s) { r *= s; g *= s; b *= s; a *= s; return *this; }

    friend Color operator*(float s, const Color& c) { return {s * c.r, s * c.g, s * c.b, s * c.a}; }

    // Component-wise multiplication
    Color operator*(const Color& c) const { return {r * c.r, g * c.g, b * c.b, a * c.a}; }
    Color& operator*=(const Color& c) { r *= c.r; g *= c.g; b *= c.b; a *= c.a; return *this; }

    bool operator==(const Color& c) const { return r == c.r && g == c.g && b == c.b && a == c.a; }
    bool operator!=(const Color& c) const { return !(*this == c); }

    // Clamp all components to [0, 1]
    Color clamp() const {
        return {
            std::clamp(r, 0.0f, 1.0f),
            std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f),
            std::clamp(a, 0.0f, 1.0f)
        };
    }
};
