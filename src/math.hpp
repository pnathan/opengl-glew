#pragma once

#include <cmath>

/* ═══════════════════════════════════════════════════════════════════════════
 * MATRIX MATHEMATICS: The Heart of 3D Graphics
 * ══════════════════════════════════════════════════════════════════════════
 *
 * 3D graphics relies heavily on 4x4 matrices for transformations. We need them
 * to move, rotate, scale, and project 3D objects onto a 2D screen.
 *
 * STORAGE FORMAT CONSIDERATION:
 * There are two ways to store a 4x4 matrix in a 1D array of 16 floats:
 *
 * - ROW-MAJOR: Matrix[row][col] is stored at index (row * 4 + col)
 *   This is intuitive for humans and matches mathematical notation
 *
 * - COLUMN-MAJOR: Matrix[row][col] is stored at index (col * 4 + row)
 *   OpenGL expects matrices in this format for efficiency reasons
 *
 * OUR APPROACH: We store matrices in ROW-MAJOR format internally (easier to
 * read and debug), then convert to COLUMN-MAJOR when uploading to OpenGL.
 * This gives us the best of both worlds: human readability and GPU efficiency.
 *
 * ══════════════════════════════════════════════════════════════════════════ */

struct Mat4
{
    float m[16]; // 4x4 matrix stored in row-major format

    /* Convert from our row-major storage to OpenGL's column-major format.
     * This is a transpose operation: we swap rows and columns.
     * OpenGL expects column-major because that's how vector-matrix
     * multiplication works more efficiently on GPUs. */
    void toColumnMajor(float out[16]) const {
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                out[col * 4 + row] = m[row * 4 + col];
    }
};

/* ───────────────────────────────────────────────────────────────────────────
 * Identity Matrix: The "do nothing" transformation
 *
 * The identity matrix is like multiplying by 1 in regular arithmetic.
 * It transforms a vector/point to itself (no change).
 *
 * Identity matrix has 1s on the diagonal, 0s elsewhere:
 * [ 1  0  0  0 ]
 * [ 0  1  0  0 ]
 * [ 0  0  1  0 ]
 * [ 0  0  0  1 ]
 * ─────────────────────────────────────────────────────────────────────────── */
Mat4 identity()
{
    Mat4 r = {}; // Zero-initialize all elements
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f; // Set diagonal to 1
    return r;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Perspective Projection Matrix
 *
 * This is the most important matrix in 3D graphics. It creates the illusion
 * of depth by making distant objects appear smaller - just like human vision.
 *
 * Parameters:
 * - fovy: Field of view angle (in radians) in the Y direction
 *         Larger values = wider angle = more "fisheye" effect
 * - aspect: Width/Height ratio of the viewport (e.g., 800/600 = 1.333)
 * - znear: Distance to near clipping plane (objects closer are clipped)
 * - zfar: Distance to far clipping plane (objects farther are clipped)
 *
 * The matrix maps the view frustum (truncated pyramid) to a cube that OpenGL
 * can easily clip and rasterize. The math involves projecting 3D points onto
 * a 2D plane while preserving depth information for z-buffering.
 *
 * Row-major perspective matrix form:
 * [ f/aspect    0       0           0        ]
 * [    0        f       0           0        ]
 * [    0        0       A           B        ]
 * [    0        0      -1           0        ]
 *
 * where A = (zfar+znear)/(znear-zfar)  and  B = (2*zfar*znear)/(znear-zfar)
 * ─────────────────────────────────────────────────────────────────────────── */
Mat4 perspective(float fovy, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fovy * 0.5f); // Cotangent of half the FOV angle
    Mat4 r = {};
    r.m[0] = f / aspect;                              // Scale X by aspect ratio
    r.m[5] = f;                                       // Scale Y
    r.m[10] = (zfar + znear) / (znear - zfar);        // Z mapping (non-linear)
    r.m[11] = (2.0f * zfar * znear) / (znear - zfar); // Depth scaling
    r.m[14] = -1.0f;                                  // W = -Z (for perspective divide)
    r.m[15] = 0.0f;                                   // No constant W
    return r;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Translation Matrix
 *
 * Moves an object in 3D space by adding offsets to X, Y, Z coordinates.
 * In homogeneous coordinates (4D), translation is encoded in the last column:
 *
 * [ 1  0  0  x ]     [ posX ]     [ posX + x ]
 * [ 0  1  0  y ]  ×  [ posY ]  =  [ posY + y ]
 * [ 0  0  1  z ]     [ posZ ]     [ posZ + z ]
 * [ 0  0  0  1 ]     [  1   ]     [    1     ]
 *
 * In row-major storage, the last column occupies indices 3, 7, 11 (and 15=1).
 * ─────────────────────────────────────────────────────────────────────────── */
Mat4 translate(float x, float y, float z)
{
    Mat4 r = identity();
    r.m[3] = x;   // Row 0, column 3 (row*4 + col = 0*4 + 3 = 3)
    r.m[7] = y;   // Row 1, column 3 (row*4 + col = 1*4 + 3 = 7)
    r.m[11] = z;  // Row 2, column 3 (row*4 + col = 2*4 + 3 = 11)
    return r;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Rotation Matrices
 *
 * These rotate objects around the X, Y, or Z axis by a given angle (radians).
 * Rotation matrices are orthogonal (their inverse equals their transpose).
 *
 * The formulas come from trigonometry. For rotation around the X-axis:
 * - Y and Z coordinates rotate in a circle (hence cos/sin)
 * - X coordinate stays the same
 *
 * Similar logic applies to Y-axis and Z-axis rotations.
 * Note: In 3D, rotation order matters! Rotating X→Y→Z gives different results
 * than rotating Z→Y→X. This is because matrix multiplication is non-commutative.
 * ─────────────────────────────────────────────────────────────────────────── */

Mat4 rotateX(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = 1;      // X stays the same
    r.m[5] = c;      // Y component of Y
    r.m[6] = -s;     // Z component of Y
    r.m[9] = s;      // Y component of Z
    r.m[10] = c;     // Z component of Z
    r.m[15] = 1;
    return r;
}

Mat4 rotateY(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = c;      // X component of X
    r.m[2] = s;      // Z component of X
    r.m[5] = 1;      // Y stays the same
    r.m[8] = -s;     // X component of Z
    r.m[10] = c;     // Z component of Z
    r.m[15] = 1;
    return r;
}

Mat4 rotateZ(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = c;      // X component of X
    r.m[1] = -s;     // Y component of X
    r.m[4] = s;      // X component of Y
    r.m[5] = c;      // Y component of Y
    r.m[10] = 1;     // Z stays the same
    r.m[15] = 1;
    return r;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Matrix Multiplication
 *
 * Combining transformations requires matrix multiplication. To apply multiple
 * transformations to a point, we multiply their matrices together first, then
 * multiply the point by the combined matrix. This is more efficient when
 * transforming many points with the same transformation sequence.
 *
 * IMPORTANT: Order matters! To first rotate then translate, we compute:
 *   FinalMatrix = Translation × Rotation
 *
 * This seems backwards, but it's because we multiply point on the right:
 *   TransformedPoint = FinalMatrix × OriginalPoint
 *
 * The transformation nearest to the point happens first (right-to-left).
 *
 * Matrix multiplication formula for row-major storage:
 *   C[i][j] = Σ(k=0 to 3) A[i][k] × B[k][j]
 * ─────────────────────────────────────────────────────────────────────────── */
Mat4 mul(const Mat4 &a, const Mat4 &b)
{
    Mat4 r = {};
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += a.m[row * 4 + k] * b.m[k * 4 + col];
            r.m[row * 4 + col] = sum;
        }
    return r;
}