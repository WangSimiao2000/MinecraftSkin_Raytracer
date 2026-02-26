#include <gtest/gtest.h>
#include "math/color.h"

TEST(Color, DefaultConstructor) {
    Color c;
    EXPECT_FLOAT_EQ(c.r, 0.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);
    EXPECT_FLOAT_EQ(c.b, 0.0f);
    EXPECT_FLOAT_EQ(c.a, 1.0f);  // default alpha is 1
}

TEST(Color, ParameterizedConstructor) {
    Color c(0.1f, 0.2f, 0.3f, 0.4f);
    EXPECT_FLOAT_EQ(c.r, 0.1f);
    EXPECT_FLOAT_EQ(c.g, 0.2f);
    EXPECT_FLOAT_EQ(c.b, 0.3f);
    EXPECT_FLOAT_EQ(c.a, 0.4f);
}

TEST(Color, DefaultAlpha) {
    Color c(0.5f, 0.6f, 0.7f);
    EXPECT_FLOAT_EQ(c.a, 1.0f);
}

TEST(Color, Addition) {
    Color a(0.1f, 0.2f, 0.3f, 0.4f);
    Color b(0.5f, 0.3f, 0.2f, 0.1f);
    Color c = a + b;
    EXPECT_FLOAT_EQ(c.r, 0.6f);
    EXPECT_FLOAT_EQ(c.g, 0.5f);
    EXPECT_FLOAT_EQ(c.b, 0.5f);
    EXPECT_FLOAT_EQ(c.a, 0.5f);
}

TEST(Color, Subtraction) {
    Color a(0.8f, 0.7f, 0.6f, 0.5f);
    Color b(0.3f, 0.2f, 0.1f, 0.1f);
    Color c = a - b;
    EXPECT_FLOAT_EQ(c.r, 0.5f);
    EXPECT_FLOAT_EQ(c.g, 0.5f);
    EXPECT_FLOAT_EQ(c.b, 0.5f);
    EXPECT_FLOAT_EQ(c.a, 0.4f);
}

TEST(Color, ScalarMultiply) {
    Color c(0.2f, 0.3f, 0.4f, 0.5f);
    Color result = c * 2.0f;
    EXPECT_FLOAT_EQ(result.r, 0.4f);
    EXPECT_FLOAT_EQ(result.g, 0.6f);
    EXPECT_FLOAT_EQ(result.b, 0.8f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

TEST(Color, ScalarMultiplyLeft) {
    Color c(0.1f, 0.2f, 0.3f, 0.4f);
    Color result = 3.0f * c;
    EXPECT_FLOAT_EQ(result.r, 0.3f);
    EXPECT_FLOAT_EQ(result.g, 0.6f);
    EXPECT_FLOAT_EQ(result.b, 0.9f);
    EXPECT_FLOAT_EQ(result.a, 1.2f);
}

TEST(Color, ScalarDivide) {
    Color c(0.4f, 0.6f, 0.8f, 1.0f);
    Color result = c / 2.0f;
    EXPECT_FLOAT_EQ(result.r, 0.2f);
    EXPECT_FLOAT_EQ(result.g, 0.3f);
    EXPECT_FLOAT_EQ(result.b, 0.4f);
    EXPECT_FLOAT_EQ(result.a, 0.5f);
}

TEST(Color, ComponentWiseMultiply) {
    Color a(0.5f, 0.6f, 0.8f, 1.0f);
    Color b(0.4f, 0.5f, 0.25f, 0.5f);
    Color c = a * b;
    EXPECT_FLOAT_EQ(c.r, 0.2f);
    EXPECT_FLOAT_EQ(c.g, 0.3f);
    EXPECT_FLOAT_EQ(c.b, 0.2f);
    EXPECT_FLOAT_EQ(c.a, 0.5f);
}

TEST(Color, ClampAboveOne) {
    Color c(1.5f, 2.0f, 3.0f, 1.1f);
    Color clamped = c.clamp();
    EXPECT_FLOAT_EQ(clamped.r, 1.0f);
    EXPECT_FLOAT_EQ(clamped.g, 1.0f);
    EXPECT_FLOAT_EQ(clamped.b, 1.0f);
    EXPECT_FLOAT_EQ(clamped.a, 1.0f);
}

TEST(Color, ClampBelowZero) {
    Color c(-0.5f, -1.0f, -0.1f, -0.3f);
    Color clamped = c.clamp();
    EXPECT_FLOAT_EQ(clamped.r, 0.0f);
    EXPECT_FLOAT_EQ(clamped.g, 0.0f);
    EXPECT_FLOAT_EQ(clamped.b, 0.0f);
    EXPECT_FLOAT_EQ(clamped.a, 0.0f);
}

TEST(Color, ClampWithinRange) {
    Color c(0.3f, 0.5f, 0.7f, 0.9f);
    Color clamped = c.clamp();
    EXPECT_FLOAT_EQ(clamped.r, 0.3f);
    EXPECT_FLOAT_EQ(clamped.g, 0.5f);
    EXPECT_FLOAT_EQ(clamped.b, 0.7f);
    EXPECT_FLOAT_EQ(clamped.a, 0.9f);
}

TEST(Color, ClampMixed) {
    Color c(-0.1f, 0.5f, 1.5f, 0.0f);
    Color clamped = c.clamp();
    EXPECT_FLOAT_EQ(clamped.r, 0.0f);
    EXPECT_FLOAT_EQ(clamped.g, 0.5f);
    EXPECT_FLOAT_EQ(clamped.b, 1.0f);
    EXPECT_FLOAT_EQ(clamped.a, 0.0f);
}

TEST(Color, ClampBoundaryValues) {
    Color c(0.0f, 1.0f, 0.0f, 1.0f);
    Color clamped = c.clamp();
    EXPECT_FLOAT_EQ(clamped.r, 0.0f);
    EXPECT_FLOAT_EQ(clamped.g, 1.0f);
    EXPECT_FLOAT_EQ(clamped.b, 0.0f);
    EXPECT_FLOAT_EQ(clamped.a, 1.0f);
}

TEST(Color, PlusEquals) {
    Color a(0.1f, 0.2f, 0.3f, 0.4f);
    a += Color(0.1f, 0.1f, 0.1f, 0.1f);
    EXPECT_FLOAT_EQ(a.r, 0.2f);
    EXPECT_FLOAT_EQ(a.g, 0.3f);
    EXPECT_FLOAT_EQ(a.b, 0.4f);
    EXPECT_FLOAT_EQ(a.a, 0.5f);
}

TEST(Color, MultiplyEquals) {
    Color a(0.5f, 0.6f, 0.7f, 0.8f);
    a *= 2.0f;
    EXPECT_FLOAT_EQ(a.r, 1.0f);
    EXPECT_FLOAT_EQ(a.g, 1.2f);
    EXPECT_FLOAT_EQ(a.b, 1.4f);
    EXPECT_FLOAT_EQ(a.a, 1.6f);
}

TEST(Color, ComponentWiseMultiplyEquals) {
    Color a(0.5f, 0.4f, 0.3f, 1.0f);
    a *= Color(2.0f, 2.0f, 2.0f, 0.5f);
    EXPECT_FLOAT_EQ(a.r, 1.0f);
    EXPECT_FLOAT_EQ(a.g, 0.8f);
    EXPECT_FLOAT_EQ(a.b, 0.6f);
    EXPECT_FLOAT_EQ(a.a, 0.5f);
}

TEST(Color, Equality) {
    Color a(0.1f, 0.2f, 0.3f, 0.4f);
    Color b(0.1f, 0.2f, 0.3f, 0.4f);
    Color c(0.1f, 0.2f, 0.3f, 0.5f);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}
