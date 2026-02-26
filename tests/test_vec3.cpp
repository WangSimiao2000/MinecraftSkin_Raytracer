#include <gtest/gtest.h>
#include <cmath>
#include "math/vec3.h"

// Helper for float comparison
static constexpr float EPS = 1e-5f;

TEST(Vec3, DefaultConstructor) {
    Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3, ParameterizedConstructor) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vec3, Addition) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3, Subtraction) {
    Vec3 a(5.0f, 7.0f, 9.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c = a - b;
    EXPECT_FLOAT_EQ(c.x, 4.0f);
    EXPECT_FLOAT_EQ(c.y, 5.0f);
    EXPECT_FLOAT_EQ(c.z, 6.0f);
}

TEST(Vec3, Negation) {
    Vec3 v(1.0f, -2.0f, 3.0f);
    Vec3 neg = -v;
    EXPECT_FLOAT_EQ(neg.x, -1.0f);
    EXPECT_FLOAT_EQ(neg.y, 2.0f);
    EXPECT_FLOAT_EQ(neg.z, -3.0f);
}

TEST(Vec3, ScalarMultiply) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 c = v * 2.0f;
    EXPECT_FLOAT_EQ(c.x, 2.0f);
    EXPECT_FLOAT_EQ(c.y, 4.0f);
    EXPECT_FLOAT_EQ(c.z, 6.0f);
}

TEST(Vec3, ScalarMultiplyLeft) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 c = 3.0f * v;
    EXPECT_FLOAT_EQ(c.x, 3.0f);
    EXPECT_FLOAT_EQ(c.y, 6.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3, ScalarDivide) {
    Vec3 v(4.0f, 6.0f, 8.0f);
    Vec3 c = v / 2.0f;
    EXPECT_FLOAT_EQ(c.x, 2.0f);
    EXPECT_FLOAT_EQ(c.y, 3.0f);
    EXPECT_FLOAT_EQ(c.z, 4.0f);
}

TEST(Vec3, DotProduct) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    // 1*4 + 2*5 + 3*6 = 32
    EXPECT_FLOAT_EQ(a.dot(b), 32.0f);
}

TEST(Vec3, DotProductPerpendicular) {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 1.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 0.0f);
}

TEST(Vec3, CrossProduct) {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 1.0f, 0.0f);
    Vec3 c = a.cross(b);
    EXPECT_FLOAT_EQ(c.x, 0.0f);
    EXPECT_FLOAT_EQ(c.y, 0.0f);
    EXPECT_FLOAT_EQ(c.z, 1.0f);
}

TEST(Vec3, CrossProductAnticommutative) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 ab = a.cross(b);
    Vec3 ba = b.cross(a);
    EXPECT_FLOAT_EQ(ab.x, -ba.x);
    EXPECT_FLOAT_EQ(ab.y, -ba.y);
    EXPECT_FLOAT_EQ(ab.z, -ba.z);
}

TEST(Vec3, Length) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
}

TEST(Vec3, LengthSquared) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.lengthSquared(), 25.0f);
}

TEST(Vec3, Normalize) {
    Vec3 v(3.0f, 0.0f, 4.0f);
    Vec3 n = v.normalize();
    EXPECT_NEAR(n.length(), 1.0f, EPS);
    EXPECT_NEAR(n.x, 3.0f / 5.0f, EPS);
    EXPECT_NEAR(n.y, 0.0f, EPS);
    EXPECT_NEAR(n.z, 4.0f / 5.0f, EPS);
}

TEST(Vec3, NormalizeZeroVector) {
    Vec3 v(0.0f, 0.0f, 0.0f);
    Vec3 n = v.normalize();
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(Vec3, PlusEquals) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    a += Vec3(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(a.x, 11.0f);
    EXPECT_FLOAT_EQ(a.y, 22.0f);
    EXPECT_FLOAT_EQ(a.z, 33.0f);
}

TEST(Vec3, MinusEquals) {
    Vec3 a(10.0f, 20.0f, 30.0f);
    a -= Vec3(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(a.x, 9.0f);
    EXPECT_FLOAT_EQ(a.y, 18.0f);
    EXPECT_FLOAT_EQ(a.z, 27.0f);
}

TEST(Vec3, MultiplyEquals) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    a *= 5.0f;
    EXPECT_FLOAT_EQ(a.x, 5.0f);
    EXPECT_FLOAT_EQ(a.y, 10.0f);
    EXPECT_FLOAT_EQ(a.z, 15.0f);
}

TEST(Vec3, DivideEquals) {
    Vec3 a(10.0f, 20.0f, 30.0f);
    a /= 10.0f;
    EXPECT_FLOAT_EQ(a.x, 1.0f);
    EXPECT_FLOAT_EQ(a.y, 2.0f);
    EXPECT_FLOAT_EQ(a.z, 3.0f);
}

TEST(Vec3, Equality) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c(1.0f, 2.0f, 4.0f);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}
