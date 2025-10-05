#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../src/math.hpp"
#include <cmath>

// Helper to compare two floats with a tolerance
bool is_close(float a, float b, float epsilon = 1e-6f) {
    return std::abs(a - b) < epsilon;
}

// Helper to compare two Mat4 matrices
void check_mat4_equal(const Mat4& a, const Mat4& b) {
    for (int i = 0; i < 16; ++i) {
        CHECK(is_close(a.m[i], b.m[i]));
    }
}

TEST_CASE("Matrix Identity") {
    Mat4 id = identity();
    Mat4 expected = {};
    expected.m[0] = 1.0f;
    expected.m[5] = 1.0f;
    expected.m[10] = 1.0f;
    expected.m[15] = 1.0f;
    check_mat4_equal(id, expected);
}

TEST_CASE("Matrix Translation") {
    Mat4 t = translate(1.0f, 2.0f, 3.0f);
    Mat4 expected = identity();
    expected.m[3] = 1.0f;
    expected.m[7] = 2.0f;
    expected.m[11] = 3.0f;
    check_mat4_equal(t, expected);
}

TEST_CASE("Matrix Multiplication") {
    Mat4 a = identity();
    a.m[0] = 2.0f; // Scale X by 2

    Mat4 b = identity();
    b.m[3] = 5.0f; // Translate X by 5

    // Result should be a matrix that first scales, then translates
    // [2 0 0 5]
    // [0 1 0 0]
    // [0 0 1 0]
    // [0 0 0 1]
    // But since we do Translation * Scale, it's different.
    // The point is transformed by Scale first, then by Translation.
    // So a point (1,0,0) becomes (2,0,0) then (2+5,0,0) = (7,0,0)
    // The combined matrix should do this in one go.
    // T * S =
    // [1 0 0 5] [2 0 0 0]   [2 0 0 5]
    // [0 1 0 0] [0 1 0 0] = [0 1 0 0]
    // [0 0 1 0] [0 0 1 0]   [0 0 1 0]
    // [0 0 0 1] [0 0 0 1]   [0 0 0 1]

    Mat4 c = mul(b, a); // T * S
    Mat4 expected_c = identity();
    expected_c.m[0] = 2.0f;
    expected_c.m[3] = 5.0f;

    check_mat4_equal(c, expected_c);

    // Now let's check S * T
    // [2 0 0 0] [1 0 0 5]   [2 0 0 10]
    // [0 1 0 0] [0 1 0 0] = [0 1 0  0]
    // [0 0 1 0] [0 0 1 0]   [0 0 1  0]
    // [0 0 0 1] [0 0 0 1]   [0 0 0  1]
    // A point (1,0,0) becomes (1+5,0,0)=(6,0,0) then (12,0,0)
    Mat4 d = mul(a, b); // S * T
    Mat4 expected_d = identity();
    expected_d.m[0] = 2.0f;
    expected_d.m[3] = 10.0f;
    check_mat4_equal(d, expected_d);
}

TEST_CASE("Matrix toColumnMajor") {
    Mat4 row_major = {};
    for(int i = 0; i < 16; ++i) {
        row_major.m[i] = (float)i;
    }

    float col_major[16];
    row_major.toColumnMajor(col_major);

    float expected_col_major[16] = {
        0, 4, 8,  12,
        1, 5, 9,  13,
        2, 6, 10, 14,
        3, 7, 11, 15
    };

    for (int i = 0; i < 16; ++i) {
        CHECK(is_close(col_major[i], expected_col_major[i]));
    }
}