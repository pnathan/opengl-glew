/* ═══════════════════════════════════════════════════════════════════════════
 * A Literate Journey Through Modern OpenGL: Cel-Shaded Spinning Cubes
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This program demonstrates the fundamentals of modern OpenGL (version 3.3+)
 * by rendering a scene with multiple rotating cubes inside a colored room,
 * illuminated by a single dynamic point light source.
 *
 * ARCHITECTURAL OVERVIEW:
 * ----------------------
 * Modern OpenGL has shifted from the old "immediate mode" (glBegin/glEnd) to
 * a programmable pipeline based on shaders. Here's the rendering flow:
 *
 * 1. VERTEX DATA → uploaded to GPU via Vertex Buffer Objects (VBO)
 * 2. VERTEX SHADER → transforms each vertex position, calculates per-vertex data
 * 3. RASTERIZATION → GPU interpolates vertex data across triangle faces
 * 4. FRAGMENT SHADER → calculates final color for each pixel
 * 5. FRAMEBUFFER → final image displayed on screen
 *
 * KEY CONCEPTS YOU'LL SEE:
 * - VAO (Vertex Array Object): Stores configuration of vertex attributes
 * - VBO (Vertex Buffer Object): Stores actual vertex data in GPU memory
 * - EBO (Element Buffer Object): Stores indices for indexed drawing
 * - Uniforms: Global shader variables we can update per-frame
 * - Matrix Transformations: Model, View, Projection (MVP)
 *
 * ══════════════════════════════════════════════════════════════════════════ */

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <GL/glew.h>    // OpenGL Extension Wrangler - loads modern OpenGL functions
#include <GLFW/glfw3.h> // Cross-platform window and input handling

#include "math.hpp"

/* ═══════════════════════════════════════════════════════════════════════════
 * SHADER LOADING
 * ══════════════════════════════════════════════════════════════════════════
 *
 * In modern OpenGL, shaders are written in GLSL (OpenGL Shading Language) and
 * loaded as strings. We keep them in separate .glsl files for better tooling
 * support (syntax highlighting, linting, etc.) and load them at runtime.
 *
 * ══════════════════════════════════════════════════════════════════════════ */

std::string loadShaderFile(const char *filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        fprintf(stderr, "Failed to open shader file: %s\n", filepath);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCENE OBJECTS AND STATE
 * ══════════════════════════════════════════════════════════════════════════
 *
 * We maintain global state for our scene elements: camera, lights, and objects.
 * In a larger application, you'd encapsulate these in classes, but for a small
 * demo, globals keep the code simple and clear.
 *
 * ══════════════════════════════════════════════════════════════════════════ */

/* ───────────────────────────────────────────────────────────────────────────
 * RandomCube: Represents a spinning cube at a random position
 *
 * Each cube has:
 * - A position in 3D space (posX, posY, posZ)
 * - Rotation speeds for each axis (how fast it spins on X, Y, Z)
 * - Current rotation angles (accumulated over time)
 *
 * This allows each cube to rotate independently at different rates.
 * ─────────────────────────────────────────────────────────────────────────── */
struct RandomCube
{
    float posX, posY, posZ;
    float rotSpeedX, rotSpeedY, rotSpeedZ;
    float currentRotX, currentRotY, currentRotZ;
};

/* ───────────────────────────────────────────────────────────────────────────
 * Camera: Implements an orbital camera using spherical coordinates
 *
 * SPHERICAL COORDINATES:
 * Instead of storing camera position as (x,y,z), we use:
 * - yaw: Horizontal angle (rotation around Y-axis) - controls left/right
 * - pitch: Vertical angle (rotation from XZ plane) - controls up/down
 * - distance: How far from the origin we're looking
 *
 * This creates an "orbit camera" that always looks at the origin (0,0,0).
 * It's perfect for examining a 3D scene from all angles.
 *
 * CONVERSION TO CARTESIAN:
 * To get actual camera position, we convert spherical → Cartesian:
 *   x = distance × cos(pitch) × cos(yaw)
 *   y = distance × sin(pitch)
 *   z = distance × cos(pitch) × sin(yaw)
 *
 * Think of it like latitude/longitude on a globe, plus a radius.
 * ─────────────────────────────────────────────────────────────────────────── */
struct Camera
{
    float yaw = -90.0f;   // Start looking down -Z axis (OpenGL convention)
    float pitch = 0.0f;   // Start level (no up/down tilt)
    float distance = 5.0f; // Start 5 units away from origin
    float lastX = 400.0f;
    float lastY = 300.0f;
    bool firstMouse = true; // Track if this is first mouse input (avoid jump)

    /* Mouse movement updates yaw and pitch. We constrain pitch to ±89° to
     * avoid "gimbal lock" (where the camera flips upside down). */
    void processMouseMovement(float xpos, float ypos, float sensitivity = 0.1f)
    {
        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // Y is inverted (screen coords go top→bottom)
        lastX = xpos;
        lastY = ypos;

        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;

        // Clamp pitch to prevent camera flip
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    /* ───────────────────────────────────────────────────────────────────────
     * View Matrix Construction (lookAt implementation)
     *
     * The view matrix transforms world coordinates to camera coordinates.
     * It's the inverse of the camera's transformation matrix.
     *
     * We build it using the "lookAt" algorithm:
     * 1. Calculate camera position from spherical coordinates
     * 2. Define what we're looking at (the origin)
     * 3. Define "up" direction (positive Y)
     * 4. Compute camera basis vectors:
     *    - forward: Direction from camera to target (normalized)
     *    - right: Cross product of forward and up (normalized)
     *    - up: Cross product of right and forward (normalized)
     * 5. Build view matrix from these basis vectors
     *
     * The view matrix has two parts:
     * - Rotation: Aligns world axes with camera axes (3x3 upper-left)
     * - Translation: Moves world origin to camera position (last column)
     *
     * ─────────────────────────────────────────────────────────────────────── */
    Mat4 getViewMatrix()
    {
        // Convert spherical coordinates to Cartesian
        float yawRad = yaw * 3.14159f / 180.0f;
        float pitchRad = pitch * 3.14159f / 180.0f;

        float camX = distance * cosf(pitchRad) * cosf(yawRad);
        float camY = distance * sinf(pitchRad);
        float camZ = distance * cosf(pitchRad) * sinf(yawRad);

        // Camera position (eye)
        float eyeX = camX, eyeY = camY, eyeZ = camZ;
        // Look at origin (center)
        float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        // World up vector
        float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

        // Forward vector: from eye to center, normalized
        float fX = centerX - eyeX;
        float fY = centerY - eyeY;
        float fZ = centerZ - eyeZ;
        float fLen = sqrtf(fX * fX + fY * fY + fZ * fZ);
        fX /= fLen; fY /= fLen; fZ /= fLen;

        // Right vector: cross product of forward and up, normalized
        // Cross product formula: (a × b) = (a.y*b.z - a.z*b.y, ...)
        float rX = fY * upZ - fZ * upY;
        float rY = fZ * upX - fX * upZ;
        float rZ = fX * upY - fY * upX;
        float rLen = sqrtf(rX * rX + rY * rY + rZ * rZ);
        rX /= rLen; rY /= rLen; rZ /= rLen;

        // Recalculate up vector: cross product of right and forward
        // This ensures orthogonality (all three axes perpendicular)
        float uX = rY * fZ - rZ * fY;
        float uY = rZ * fX - rX * fZ;
        float uZ = rX * fY - rY * fX;

        // Build view matrix (row-major)
        // The translation part uses dot products to transform the eye position
        Mat4 view = {};
        view.m[0] = rX;  view.m[1] = rY;  view.m[2] = rZ;
        view.m[3] = -(rX * eyeX + rY * eyeY + rZ * eyeZ);

        view.m[4] = uX;  view.m[5] = uY;  view.m[6] = uZ;
        view.m[7] = -(uX * eyeX + uY * eyeY + uZ * eyeZ);

        view.m[8] = -fX; view.m[9] = -fY; view.m[10] = -fZ;
        view.m[11] = fX * eyeX + fY * eyeY + fZ * eyeZ;

        view.m[12] = 0.0f; view.m[13] = 0.0f; view.m[14] = 0.0f; view.m[15] = 1.0f;

        return view;
    }
};

/* ───────────────────────────────────────────────────────────────────────────
 * Global State Variables
 * ─────────────────────────────────────────────────────────────────────────── */

Camera g_camera; // Single camera for the scene

// Lighting state
float g_lightBrightness = 1.0f; // Multiplier for light intensity
float g_lightPosX = 3.0f, g_lightPosY = 3.0f, g_lightPosZ = 3.0f;

// Random cubes scattered in the scene
const int NUM_RANDOM_CUBES = 8;
RandomCube g_randomCubes[NUM_RANDOM_CUBES];

/* ───────────────────────────────────────────────────────────────────────────
 * Initialize Random Cubes with Pseudo-Random Values
 *
 * We use a simple linear congruential generator (LCG) for randomness.
 * This is a deterministic pseudo-random number generator (same seed = same
 * sequence). It's not cryptographically secure, but perfect for game/demo use.
 *
 * LCG formula: next = (current × 1103515245 + 12345) mod 2^32
 * This is the same algorithm used by many C standard libraries.
 * ─────────────────────────────────────────────────────────────────────────── */
void initRandomCubes()
{
    unsigned int seed = 12345;
    auto rnd = [&seed]() -> float {
        seed = seed * 1103515245 + 12345;
        return ((seed / 65536) % 32768) / 32768.0f; // Returns 0.0 to 1.0
    };

    for (int i = 0; i < NUM_RANDOM_CUBES; i++)
    {
        // Position: random locations within -6 to +6 range
        g_randomCubes[i].posX = (rnd() - 0.5f) * 12.0f;
        g_randomCubes[i].posY = (rnd() - 0.5f) * 12.0f;
        g_randomCubes[i].posZ = (rnd() - 0.5f) * 12.0f;

        // Rotation speeds: random angular velocities (radians per second)
        g_randomCubes[i].rotSpeedX = (rnd() - 0.5f) * 2.0f;
        g_randomCubes[i].rotSpeedY = (rnd() - 0.5f) * 2.0f;
        g_randomCubes[i].rotSpeedZ = (rnd() - 0.5f) * 2.0f;

        // Start with no rotation
        g_randomCubes[i].currentRotX = 0.0f;
        g_randomCubes[i].currentRotY = 0.0f;
        g_randomCubes[i].currentRotZ = 0.0f;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * INPUT CALLBACKS
 * ══════════════════════════════════════════════════════════════════════════
 *
 * GLFW uses callback functions for input events. We register these with GLFW,
 * and it calls them whenever the user moves the mouse or scrolls.
 *
 * ══════════════════════════════════════════════════════════════════════════ */

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    g_camera.processMouseMovement((float)xpos, (float)ypos);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    // Mouse wheel controls camera distance (zoom in/out)
    g_camera.distance -= (float)yoffset * 0.5f;
    if (g_camera.distance < 1.0f) g_camera.distance = 1.0f;
    if (g_camera.distance > 20.0f) g_camera.distance = 20.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN PROGRAM
 * ══════════════════════════════════════════════════════════════════════════
 *
 * The main function follows this structure:
 * 1. Initialize GLFW and create a window
 * 2. Initialize GLEW to access modern OpenGL functions
 * 3. Create vertex data and upload to GPU (VBO/VAO/EBO)
 * 4. Load and compile shaders
 * 5. Main render loop:
 *    - Poll input
 *    - Update scene state
 *    - Render frame
 *    - Swap buffers
 * 6. Cleanup and exit
 *
 * ══════════════════════════════════════════════════════════════════════════ */

int main()
{
    /* ───────────────────────────────────────────────────────────────────────
     * STEP 1: Initialize GLFW and Create Window
     *
     * GLFW (Graphics Library Framework) handles:
     * - Cross-platform window creation (Windows, Mac, Linux)
     * - OpenGL context creation
     * - Input handling (keyboard, mouse)
     * - Timing functions
     *
     * We request OpenGL 3.3 Core Profile:
     * - Version 3.3: Modern OpenGL with programmable pipeline
     * - Core Profile: No deprecated/legacy functions (forces best practices)
     * ─────────────────────────────────────────────────────────────────────── */

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to init GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Spinning Cube - Mouse Look", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Register input callbacks
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Capture cursor for FPS-style look

    /* ───────────────────────────────────────────────────────────────────────
     * STEP 2: Initialize GLEW
     *
     * GLEW (OpenGL Extension Wrangler) loads modern OpenGL function pointers.
     *
     * WHY DO WE NEED THIS?
     * On Windows, the system only provides OpenGL 1.1 functions by default.
     * To access modern OpenGL (3.3+), we need to query and load function
     * pointers at runtime. GLEW does this automatically for us.
     *
     * After glewInit(), we can use functions like glCreateShader(),
     * glGenVertexArrays(), etc.
     * ─────────────────────────────────────────────────────────────────────── */

    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to init GLEW\n");
        return -1;
    }

    // Initialize our scene objects
    initRandomCubes();

    /* ═══════════════════════════════════════════════════════════════════════
     * STEP 3: Define Vertex Data
     * ══════════════════════════════════════════════════════════════════════
     *
     * VERTEX FORMAT:
     * Each vertex has 6 floats: [posX, posY, posZ, colorR, colorG, colorB]
     *
     * This is called "interleaved vertex data" - position and color data are
     * mixed together for each vertex. This is cache-friendly on GPUs.
     *
     * CUBE CONSTRUCTION:
     * A cube has 8 vertices (corners). We list all 8 with their positions
     * and colors. Each vertex gets a different color to create a rainbow cube.
     *
     * WHY NOT 24 VERTICES (4 per face)?
     * We use indexed drawing with an Element Buffer Object (EBO). This lets
     * us reuse vertices. For example, vertex 0 is used by 3 different faces.
     * This saves memory and bandwidth.
     *
     * ══════════════════════════════════════════════════════════════════════ */

    // Small spinning cube (unit size, centered at origin)
    float vertices[] = {
        // positions         // colors (R, G, B)
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, // 0: back-bottom-left (red)
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, // 1: back-bottom-right (green)
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, // 2: back-top-right (blue)
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, // 3: back-top-left (yellow)
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, // 4: front-bottom-left (magenta)
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f, // 5: front-bottom-right (cyan)
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, // 6: front-top-right (white)
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.0f  // 7: front-top-left (black)
    };

    /* Element indices define triangles using vertex numbers.
     * Each face is 2 triangles (6 indices). The cube has 6 faces = 36 indices.
     *
     * Triangle winding order matters! By default, OpenGL considers triangles
     * with counter-clockwise (CCW) vertices to be front-facing.
     * We can enable backface culling to skip rendering back faces (optimization). */
    unsigned int indices[] = {
        0, 1, 2,  2, 3, 0,  // Back face
        4, 5, 6,  6, 7, 4,  // Front face
        0, 1, 5,  5, 4, 0,  // Bottom face
        2, 3, 7,  7, 6, 2,  // Top face
        0, 3, 7,  7, 4, 0,  // Left face
        1, 2, 6,  6, 5, 1   // Right face
    };

    /* ───────────────────────────────────────────────────────────────────────
     * World Cube: The "Room" We're Inside
     *
     * This is a large cube (20×20×20) that acts as walls around our scene.
     * We want to see the INSIDE of this cube, not the outside.
     *
     * INVERTED WINDING ORDER:
     * Normally, we see the outside of objects. To see inside, we reverse the
     * triangle winding order. Instead of counter-clockwise, we use clockwise.
     * This makes the inward-facing sides front-facing.
     *
     * PER-FACE COLORS:
     * To give each wall a distinct color, we duplicate vertices. Each face
     * gets 4 vertices (instead of sharing the 8 corners). This lets each face
     * have uniform color despite vertex color interpolation.
     *
     * Color scheme: Blue, Red, Green, Yellow, Magenta, Cyan
     * ─────────────────────────────────────────────────────────────────────── */

    float worldVertices[] = {
        // Back face (Z = -10) - BLUE
        -10.0f, -10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
         10.0f, -10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
         10.0f,  10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
        -10.0f,  10.0f, -10.0f,  0.3f, 0.4f, 0.9f,

        // Front face (Z = +10) - RED
        -10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
         10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
         10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.3f,

        // Bottom face (Y = -10) - GREEN
        -10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.4f,
         10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.4f,
         10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.4f,
        -10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.4f,

        // Top face (Y = +10) - YELLOW
        -10.0f,  10.0f, -10.0f,  0.9f, 0.9f, 0.3f,
         10.0f,  10.0f, -10.0f,  0.9f, 0.9f, 0.3f,
         10.0f,  10.0f,  10.0f,  0.9f, 0.9f, 0.3f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.9f, 0.3f,

        // Left face (X = -10) - MAGENTA
        -10.0f, -10.0f, -10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f,  10.0f, -10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.9f,

        // Right face (X = +10) - CYAN
         10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.9f,
         10.0f,  10.0f, -10.0f,  0.3f, 0.9f, 0.9f,
         10.0f,  10.0f,  10.0f,  0.3f, 0.9f, 0.9f,
         10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.9f
    };

    // Inverted winding order (clockwise) to see inside faces
    unsigned int worldIndices[] = {
        0, 2, 1,   0, 3, 2,   // Back face
        4, 6, 5,   4, 7, 6,   // Front face
        8, 10, 9,  8, 11, 10, // Bottom face
        12, 14, 13, 12, 15, 14, // Top face
        16, 18, 17, 16, 19, 18, // Left face
        20, 22, 21, 20, 23, 22  // Right face
    };

    /* ───────────────────────────────────────────────────────────────────────
     * Light Cube: Visual Representation of Light Source
     *
     * This small white cube sits at the light's position. It's purely visual -
     * it shows where the light is coming from. We render it as "emissive"
     * (self-illuminated) so it appears bright white regardless of lighting.
     * ─────────────────────────────────────────────────────────────────────── */

    float lightVertices[] = {
        // Half-size cube (0.5 instead of 1.0), all white
        -0.25f, -0.25f, -0.25f,  1.0f, 1.0f, 1.0f,
         0.25f, -0.25f, -0.25f,  1.0f, 1.0f, 1.0f,
         0.25f,  0.25f, -0.25f,  1.0f, 1.0f, 1.0f,
        -0.25f,  0.25f, -0.25f,  1.0f, 1.0f, 1.0f,
        -0.25f, -0.25f,  0.25f,  1.0f, 1.0f, 1.0f,
         0.25f, -0.25f,  0.25f,  1.0f, 1.0f, 1.0f,
         0.25f,  0.25f,  0.25f,  1.0f, 1.0f, 1.0f,
        -0.25f,  0.25f,  0.25f,  1.0f, 1.0f, 1.0f
    };
    unsigned int lightIndices[] = {
        0, 1, 2,  2, 3, 0,
        4, 5, 6,  6, 7, 4,
        0, 1, 5,  5, 4, 0,
        2, 3, 7,  7, 6, 2,
        0, 3, 7,  7, 4, 0,
        1, 2, 6,  6, 5, 1
    };

    /* ═══════════════════════════════════════════════════════════════════════
     * STEP 4: Upload Vertex Data to GPU
     * ══════════════════════════════════════════════════════════════════════
     *
     * Modern OpenGL uses three types of buffer objects:
     *
     * 1. VAO (Vertex Array Object):
     *    Stores the configuration of vertex attributes. Think of it as a
     *    "state container" that remembers how to interpret vertex data.
     *    When we bind a VAO, OpenGL remembers which VBO/EBO to use and
     *    how the data is laid out.
     *
     * 2. VBO (Vertex Buffer Object):
     *    Stores the actual vertex data (positions, colors, etc.) in GPU memory.
     *    This is GL_ARRAY_BUFFER - a general purpose data buffer.
     *
     * 3. EBO (Element Buffer Object):
     *    Stores indices for indexed drawing. This is GL_ELEMENT_ARRAY_BUFFER.
     *    Instead of duplicating vertices, we reference them by index.
     *
     * WHY USE VAOs?
     * Before VAOs, you had to set up vertex attributes before every draw call.
     * VAOs let you set up once, then just bind the VAO before drawing.
     * This is much more efficient.
     *
     * We create three separate VAO/VBO/EBO sets for our three types of cubes:
     * - Spinning cube (small, colorful)
     * - World cube (large room)
     * - Light cube (small white)
     *
     * ══════════════════════════════════════════════════════════════════════ */

    /* ───────────────────────────────────────────────────────────────────────
     * Setup Spinning Cube VAO
     * ─────────────────────────────────────────────────────────────────────── */
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);  // Generate VAO ID
    glGenBuffers(1, &vbo);       // Generate VBO ID
    glGenBuffers(1, &ebo);       // Generate EBO ID

    glBindVertexArray(vao);      // Bind VAO (start recording state)

    // Upload vertex data to VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // GL_STATIC_DRAW hints that we won't modify this data after upload

    // Upload index data to EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Configure vertex attributes:
     * We need to tell OpenGL how to interpret the vertex data.
     * Our format: [X, Y, Z, R, G, B] repeated for each vertex
     *
     * Attribute 0 (position): 3 floats, starts at offset 0
     * Attribute 1 (color): 3 floats, starts at offset 3*sizeof(float)
     * Stride: 6*sizeof(float) - distance between consecutive vertices
     */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

    // VAO has now recorded all this state. When we bind this VAO later,
    // OpenGL will automatically configure these attributes.

    /* ───────────────────────────────────────────────────────────────────────
     * Setup World Cube VAO (same process as above)
     * ─────────────────────────────────────────────────────────────────────── */
    GLuint worldVao, worldVbo, worldEbo;
    glGenVertexArrays(1, &worldVao);
    glGenBuffers(1, &worldVbo);
    glGenBuffers(1, &worldEbo);
    glBindVertexArray(worldVao);
    glBindBuffer(GL_ARRAY_BUFFER, worldVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(worldVertices), worldVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(worldIndices), worldIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

    /* ───────────────────────────────────────────────────────────────────────
     * Setup Light Cube VAO
     * ─────────────────────────────────────────────────────────────────────── */
    GLuint lightVao, lightVbo, lightEbo;
    glGenVertexArrays(1, &lightVao);
    glGenBuffers(1, &lightVbo);
    glGenBuffers(1, &lightEbo);
    glBindVertexArray(lightVao);
    glBindBuffer(GL_ARRAY_BUFFER, lightVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lightVertices), lightVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lightEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lightIndices), lightIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

    /* ═══════════════════════════════════════════════════════════════════════
     * STEP 5: Load and Compile Shaders
     * ══════════════════════════════════════════════════════════════════════
     *
     * WHAT ARE SHADERS?
     * Shaders are small programs that run on the GPU. Modern graphics requires
     * at least two types:
     *
     * 1. VERTEX SHADER:
     *    Runs once per vertex. Its job is to transform vertex positions from
     *    model space → world space → view space → clip space.
     *    It also prepares data (colors, normals, etc.) to pass to fragment shader.
     *
     * 2. FRAGMENT SHADER:
     *    Runs once per pixel (fragment). Its job is to calculate the final
     *    color of each pixel. This is where lighting calculations happen.
     *
     * THE SHADER PIPELINE:
     * Vertex Shader → Rasterization → Fragment Shader → Framebuffer
     *
     * Between vertex and fragment shader, the GPU interpolates values across
     * the triangle. This is how we get smooth color gradients and lighting.
     *
     * SHADER LANGUAGE:
     * Shaders are written in GLSL (OpenGL Shading Language), a C-like language.
     * We compile them at runtime, link them into a program, and use that program
     * for rendering.
     *
     * ══════════════════════════════════════════════════════════════════════ */

    std::string vertexShaderSource = loadShaderFile("src/shaders/vertex.glsl");
    std::string fragmentShaderSource = loadShaderFile("src/shaders/fragment.glsl");

    if (vertexShaderSource.empty() || fragmentShaderSource.empty())
    {
        fprintf(stderr, "Failed to load shader files\n");
        return -1;
    }

    /* Lambda to compile a shader and check for errors */
    auto compile = [](GLenum type, const char *src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);

        // Check compilation status
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char buf[512];
            glGetShaderInfoLog(s, 512, NULL, buf);
            fprintf(stderr, "Shader error: %s\n", buf);
        }
        return s;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vertexShaderSource.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());

    // Create shader program and link shaders together
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    // Check linking status
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char buf[512];
        glGetProgramInfoLog(prog, 512, NULL, buf);
        fprintf(stderr, "Program link error: %s\n", buf);
        return -1;
    }

    /* ═══════════════════════════════════════════════════════════════════════
     * STEP 6: Enable Depth Testing
     * ══════════════════════════════════════════════════════════════════════
     *
     * DEPTH TESTING (Z-Buffer):
     * Without depth testing, triangles are drawn in the order we submit them.
     * This causes far objects to appear in front of near objects - wrong!
     *
     * The depth buffer stores the depth (distance from camera) of each pixel.
     * When drawing a new pixel, OpenGL compares its depth to the stored depth:
     * - If closer: draw pixel and update depth buffer
     * - If farther: skip pixel (it's behind something)
     *
     * This correctly handles overlapping objects regardless of draw order.
     * We must clear the depth buffer each frame (GL_DEPTH_BUFFER_BIT).
     *
     * ══════════════════════════════════════════════════════════════════════ */

    glEnable(GL_DEPTH_TEST);

    /* ═══════════════════════════════════════════════════════════════════════
     * STEP 7: Main Render Loop
     * ══════════════════════════════════════════════════════════════════════
     *
     * The render loop continues until the user closes the window.
     * Each iteration:
     * 1. Calculate delta time (for smooth animation independent of frame rate)
     * 2. Process input
     * 3. Update scene state (rotate cubes, etc.)
     * 4. Render everything
     * 5. Swap buffers (display the rendered frame)
     *
     * DOUBLE BUFFERING:
     * We render to a back buffer while the front buffer is displayed.
     * When rendering is complete, we swap them. This prevents flickering
     * (you never see a half-drawn frame).
     *
     * ══════════════════════════════════════════════════════════════════════ */

    float t0 = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        /* Calculate delta time: time since last frame.
         * This makes animations frame-rate independent. If we rotate by
         * "angle += speed * dt", the rotation happens at a constant speed
         * regardless of whether we're running at 30fps or 144fps. */
        float t = (float)glfwGetTime();
        float dt = t - t0;
        t0 = t;

        /* ───────────────────────────────────────────────────────────────────
         * Input Handling
         *
         * ESC key toggles mouse capture (lets you use your mouse normally)
         * +/- keys control light brightness
         * ─────────────────────────────────────────────────────────────────── */

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            static bool cursorDisabled = true;
            cursorDisabled = !cursorDisabled;
            glfwSetInputMode(window, GLFW_CURSOR,
                           cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            g_camera.firstMouse = true; // Reset to avoid camera jump
            glfwWaitEventsTimeout(0.2); // Small delay to prevent rapid toggling
        }

        // Brightness controls: + and - keys (keyboard and numpad)
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            g_lightBrightness += dt * 2.0f;
            if (g_lightBrightness > 5.0f) g_lightBrightness = 5.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            g_lightBrightness -= dt * 2.0f;
            if (g_lightBrightness < 0.1f) g_lightBrightness = 0.1f;
        }

        /* ───────────────────────────────────────────────────────────────────
         * Clear Buffers
         *
         * We must clear both color and depth buffers each frame:
         * - Color buffer: Set all pixels to background color
         * - Depth buffer: Reset all depths to maximum (far away)
         * ─────────────────────────────────────────────────────────────────── */

        glViewport(0, 0, 800, 600);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f); // Dark blue-grey background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* ───────────────────────────────────────────────────────────────────
         * Activate Shader Program
         *
         * This tells OpenGL to use our compiled shader program for subsequent
         * draw calls. All uniforms we set and all draws will use this program.
         * ─────────────────────────────────────────────────────────────────── */

        glUseProgram(prog);

        /* ───────────────────────────────────────────────────────────────────
         * Setup Transformation Matrices
         *
         * THE MVP TRANSFORMATION:
         * To render a 3D object, we transform vertex positions through three
         * coordinate spaces:
         *
         * 1. MODEL space → WORLD space (Model matrix)
         *    Positions object in the world (translate, rotate, scale)
         *
         * 2. WORLD space → VIEW space (View matrix)
         *    Transforms world relative to camera
         *
         * 3. VIEW space → CLIP space (Projection matrix)
         *    Applies perspective and maps to normalized device coordinates
         *
         * We combine these: MVP = Projection × View × Model
         * (Remember: right-to-left application due to matrix multiplication)
         *
         * Each object gets its own Model matrix, but Projection and View are
         * shared across the whole scene.
         * ─────────────────────────────────────────────────────────────────── */

        Mat4 proj = perspective(3.1415f * 0.4f, // ~72 degree FOV
                               800.0f / 600.0f,  // Aspect ratio
                               0.1f,             // Near plane
                               100.0f);          // Far plane

        Mat4 view = g_camera.getViewMatrix();

        static float angle = 0;
        angle += dt; // Accumulate rotation angle over time

        // Calculate camera position for lighting calculations
        float yawRad = g_camera.yaw * 3.14159f / 180.0f;
        float pitchRad = g_camera.pitch * 3.14159f / 180.0f;
        float camX = g_camera.distance * cosf(pitchRad) * cosf(yawRad);
        float camY = g_camera.distance * sinf(pitchRad);
        float camZ = g_camera.distance * cosf(pitchRad) * sinf(yawRad);

        /* ───────────────────────────────────────────────────────────────────
         * Upload Uniforms to Shaders
         *
         * UNIFORMS are global variables in shaders that stay constant for all
         * vertices/fragments in a draw call. We can change them between draws.
         *
         * We pass to shaders:
         * - uLightPos: Light position (for lighting calculations)
         * - uViewPos: Camera position (for specular highlights and rim lighting)
         * - uBrightness: Light intensity multiplier
         * - uMVP: Model-View-Projection matrix (per-object)
         * - uModel: Model matrix alone (needed for normal transformation)
         * - uIsEmissive: Flag for emissive objects (light cube)
         * ─────────────────────────────────────────────────────────────────── */

        GLint lightPosLoc = glGetUniformLocation(prog, "uLightPos");
        GLint viewPosLoc = glGetUniformLocation(prog, "uViewPos");
        GLint brightLoc = glGetUniformLocation(prog, "uBrightness");
        glUniform3f(lightPosLoc, g_lightPosX, g_lightPosY, g_lightPosZ);
        glUniform3f(viewPosLoc, camX, camY, camZ);
        glUniform1f(brightLoc, g_lightBrightness);

        GLint mvpLoc = glGetUniformLocation(prog, "uMVP");
        GLint modelLoc = glGetUniformLocation(prog, "uModel");
        GLint emissiveLoc = glGetUniformLocation(prog, "uIsEmissive");

        /* ═══════════════════════════════════════════════════════════════════
         * Render Objects
         * ══════════════════════════════════════════════════════════════════
         *
         * We render in this order:
         * 1. World cube (room walls)
         * 2. Light cube (visual indicator of light source)
         * 3. Random spinning cubes
         *
         * For each object:
         * - Set uniforms (MVP matrices, emissive flag)
         * - Bind VAO (configures vertex attributes)
         * - Call glDrawElements (renders the triangles)
         *
         * ══════════════════════════════════════════════════════════════════ */

        /* ───────────────────────────────────────────────────────────────────
         * Draw World Cube (Room)
         *
         * The world cube uses an identity model matrix (no transformation).
         * It's already positioned at the origin with size 20×20×20.
         * We set uIsEmissive=0 so it receives lighting.
         * ─────────────────────────────────────────────────────────────────── */

        glUniform1i(emissiveLoc, 0); // Not emissive (receives lighting)

        Mat4 worldModel = identity();
        Mat4 worldMvp = mul(proj, mul(view, worldModel));

        // Convert to column-major for OpenGL
        float worldMvpColMajor[16], worldModelColMajor[16];
        worldMvp.toColumnMajor(worldMvpColMajor);
        worldModel.toColumnMajor(worldModelColMajor);

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, worldMvpColMajor);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, worldModelColMajor);

        glBindVertexArray(worldVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        // 36 indices = 12 triangles = 6 faces × 2 triangles per face

        /* ───────────────────────────────────────────────────────────────────
         * Draw Light Cube
         *
         * The light cube is translated to the light's position.
         * We set uIsEmissive=1 so it renders as pure white without lighting.
         * This makes it appear to glow.
         * ─────────────────────────────────────────────────────────────────── */

        glUniform1i(emissiveLoc, 1); // Emissive (no lighting, pure white)

        Mat4 lightTranslate = translate(g_lightPosX, g_lightPosY, g_lightPosZ);
        Mat4 lightMvp = mul(proj, mul(view, lightTranslate));

        float lightMvpColMajor[16], lightModelColMajor[16];
        lightMvp.toColumnMajor(lightMvpColMajor);
        lightTranslate.toColumnMajor(lightModelColMajor);

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, lightMvpColMajor);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, lightModelColMajor);

        glBindVertexArray(lightVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        /* ───────────────────────────────────────────────────────────────────
         * Draw Random Cubes
         *
         * Each cube has its own position and rotation. We:
         * 1. Update rotation angles based on time
         * 2. Build transformation: Translate × RotateZ × RotateY × RotateX
         * 3. Combine with view and projection
         * 4. Upload matrices and draw
         *
         * These cubes receive lighting (uIsEmissive=0).
         * ─────────────────────────────────────────────────────────────────── */

        glUniform1i(emissiveLoc, 0); // Not emissive

        for (int i = 0; i < NUM_RANDOM_CUBES; i++)
        {
            // Update rotations: angle += rotationSpeed × deltaTime
            g_randomCubes[i].currentRotX += g_randomCubes[i].rotSpeedX * dt;
            g_randomCubes[i].currentRotY += g_randomCubes[i].rotSpeedY * dt;
            g_randomCubes[i].currentRotZ += g_randomCubes[i].rotSpeedZ * dt;

            // Build model matrix: first rotate, then translate
            Mat4 rX = rotateX(g_randomCubes[i].currentRotX);
            Mat4 rY = rotateY(g_randomCubes[i].currentRotY);
            Mat4 rZ = rotateZ(g_randomCubes[i].currentRotZ);
            Mat4 pos = translate(g_randomCubes[i].posX,
                               g_randomCubes[i].posY,
                               g_randomCubes[i].posZ);

            // Combine: Translation × RotZ × RotY × RotX
            // (Rotations apply first, then translation moves to final position)
            Mat4 model = mul(pos, mul(rZ, mul(rY, rX)));
            Mat4 mvp = mul(proj, mul(view, model));

            float mvpColMajor[16], modelColMajor[16];
            mvp.toColumnMajor(mvpColMajor);
            model.toColumnMajor(modelColMajor);

            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvpColMajor);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelColMajor);

            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        /* ───────────────────────────────────────────────────────────────────
         * Swap Buffers and Poll Events
         *
         * glfwSwapBuffers: Display the frame we just rendered (swap back→front)
         * glfwPollEvents: Process window events (input, resize, etc.)
         * ─────────────────────────────────────────────────────────────────── */

        glfwSwapBuffers(window);
        glfwPollEvents();

        /* ───────────────────────────────────────────────────────────────────
         * HUD: Console Output
         *
         * Print camera and light info to console every 30 frames.
         * We use \r (carriage return) to overwrite the same line repeatedly.
         * ─────────────────────────────────────────────────────────────────── */

        static int frameCount = 0;
        if (++frameCount >= 30) {
            frameCount = 0;
            printf("\r[CAM] Yaw:%.1f Pitch:%.1f Dist:%.1f | [LIGHT] Brightness:%.2f Pos:(%.1f,%.1f,%.1f)   ",
                   g_camera.yaw, g_camera.pitch, g_camera.distance,
                   g_lightBrightness, g_lightPosX, g_lightPosY, g_lightPosZ);
            fflush(stdout);
        }
    }

    /* ═══════════════════════════════════════════════════════════════════════
     * Cleanup and Exit
     * ══════════════════════════════════════════════════════════════════════ */

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * END OF PROGRAM
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT'S HAPPENING IN THE SHADERS?
 *
 * Vertex Shader (vertex.glsl):
 * - Transforms vertex positions: model space → clip space
 * - Transforms normals by model matrix (for correct lighting after rotation)
 * - Passes color and position to fragment shader
 *
 * Fragment Shader (fragment.glsl):
 * - Implements cel shading (cartoon-style lighting)
 * - Calculates lighting direction from fragment to light
 * - Quantizes light intensity into discrete bands (1.0, 0.7, 0.4, 0.2)
 * - Applies distance attenuation (1 / (1 + linear*d + quadratic*d²))
 * - Adds rim lighting (highlights edges facing away from camera)
 * - For emissive objects (light cube), bypasses all lighting and returns pure color
 *
 * The result is a stylized, cartoon-like rendering with sharp lighting boundaries
 * rather than smooth gradients. This "cel shading" technique is used in games
 * like The Legend of Zelda: The Wind Waker and Borderlands.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */