// Minimal spinning cube demo using GLFW + GLEW and modern OpenGL
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Helper function to load shader source from file
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

// Simple matrix helpers - stored in ROW-MAJOR format (intuitive)
struct Mat4
{
    float m[16];

    // Convert to column-major for OpenGL upload
    void toColumnMajor(float out[16]) const {
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                out[col * 4 + row] = m[row * 4 + col];
    }
};

Mat4 identity()
{
    Mat4 r = {};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 perspective(float fovy, float aspect, float znear, float zfar)
{
    // Row-major perspective matrix
    // Layout:  f/a  0    0    0
    //          0    f    0    0
    //          0    0    A    B
    //          0    0   -1    0
    float f = 1.0f / tanf(fovy * 0.5f);
    Mat4 r = {};
    r.m[0] = f / aspect;                              // [0][0]
    r.m[5] = f;                                       // [1][1]
    r.m[10] = (zfar + znear) / (znear - zfar);        // [2][2] = A
    r.m[11] = (2.0f * zfar * znear) / (znear - zfar); // [2][3] = B
    r.m[14] = -1.0f;                                  // [3][2] = -1
    r.m[15] = 0.0f;                                   // [3][3] = 0
    return r;
}

Mat4 translate(float x, float y, float z)
{
    // Create a translation matrix in row-major format
    // In row-major, index = row*4 + col
    // Translation in last column means col=3: indices 3, 7, 11
    Mat4 r = identity();
    r.m[3] = x;   // index 3  = row 0*4 + 3
    r.m[7] = y;   // index 7  = row 1*4 + 3
    r.m[11] = z;  // index 11 = row 2*4 + 3
    return r;
}
Mat4 rotateX(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = 1;
    r.m[5] = c;
    r.m[6] = -s;
    r.m[9] = s;
    r.m[10] = c;
    r.m[15] = 1;
    return r;
}

Mat4 rotateY(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = c;
    r.m[2] = s;
    r.m[5] = 1;
    r.m[8] = -s;
    r.m[10] = c;
    r.m[15] = 1;
    return r;
}

Mat4 rotateZ(float angle)
{
    Mat4 r = {};
    float c = cosf(angle), s = sinf(angle);
    r.m[0] = c;
    r.m[1] = -s;
    r.m[4] = s;
    r.m[5] = c;
    r.m[10] = 1;
    r.m[15] = 1;
    return r;
}
Mat4 mul(const Mat4 &a, const Mat4 &b)
{
    // Row-major multiplication: C[row][col] = sum over k of A[row][k] * B[k][col]
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

// Random cube struct
struct RandomCube
{
    float posX, posY, posZ;
    float rotSpeedX, rotSpeedY, rotSpeedZ;
    float currentRotX, currentRotY, currentRotZ;
};

// Camera struct to track orientation
struct Camera
{
    float yaw = -90.0f;   // Horizontal rotation (left/right)
    float pitch = 0.0f;   // Vertical rotation (up/down)
    float distance = 5.0f; // Distance from origin
    float lastX = 400.0f;
    float lastY = 300.0f;
    bool firstMouse = true;

    void processMouseMovement(float xpos, float ypos, float sensitivity = 0.1f)
    {
        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top
        lastX = xpos;
        lastY = ypos;

        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;

        // Constrain pitch to avoid gimbal lock
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;
    }

    Mat4 getViewMatrix()
    {
        // Convert spherical coordinates (yaw, pitch, distance) to Cartesian
        float yawRad = yaw * 3.14159f / 180.0f;
        float pitchRad = pitch * 3.14159f / 180.0f;

        float camX = distance * cosf(pitchRad) * cosf(yawRad);
        float camY = distance * sinf(pitchRad);
        float camZ = distance * cosf(pitchRad) * sinf(yawRad);

        // Camera position
        float eyeX = camX, eyeY = camY, eyeZ = camZ;
        // Look at origin
        float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        // Up vector
        float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

        // Calculate forward vector (from eye to center)
        float fX = centerX - eyeX;
        float fY = centerY - eyeY;
        float fZ = centerZ - eyeZ;
        float fLen = sqrtf(fX * fX + fY * fY + fZ * fZ);
        fX /= fLen;
        fY /= fLen;
        fZ /= fLen;

        // Calculate right vector (cross product of forward and up)
        float rX = fY * upZ - fZ * upY;
        float rY = fZ * upX - fX * upZ;
        float rZ = fX * upY - fY * upX;
        float rLen = sqrtf(rX * rX + rY * rY + rZ * rZ);
        rX /= rLen;
        rY /= rLen;
        rZ /= rLen;

        // Recalculate up vector (cross product of right and forward)
        float uX = rY * fZ - rZ * fY;
        float uY = rZ * fX - rX * fZ;
        float uZ = rX * fY - rY * fX;

        // Build view matrix (row-major)
        Mat4 view = {};
        view.m[0] = rX;
        view.m[1] = rY;
        view.m[2] = rZ;
        view.m[3] = -(rX * eyeX + rY * eyeY + rZ * eyeZ);
        view.m[4] = uX;
        view.m[5] = uY;
        view.m[6] = uZ;
        view.m[7] = -(uX * eyeX + uY * eyeY + uZ * eyeZ);
        view.m[8] = -fX;
        view.m[9] = -fY;
        view.m[10] = -fZ;
        view.m[11] = fX * eyeX + fY * eyeY + fZ * eyeZ;
        view.m[12] = 0.0f;
        view.m[13] = 0.0f;
        view.m[14] = 0.0f;
        view.m[15] = 1.0f;

        return view;
    }
};

// Global camera instance for mouse callback
Camera g_camera;

// Global lighting
float g_lightBrightness = 1.0f;
float g_lightPosX = 3.0f, g_lightPosY = 3.0f, g_lightPosZ = 3.0f;

// Random cubes
const int NUM_RANDOM_CUBES = 8;
RandomCube g_randomCubes[NUM_RANDOM_CUBES];

void initRandomCubes()
{
    // Simple pseudo-random using time
    unsigned int seed = 12345;
    auto rnd = [&seed]() -> float {
        seed = seed * 1103515245 + 12345;
        return ((seed / 65536) % 32768) / 32768.0f;
    };

    for (int i = 0; i < NUM_RANDOM_CUBES; i++)
    {
        g_randomCubes[i].posX = (rnd() - 0.5f) * 12.0f; // -6 to 6
        g_randomCubes[i].posY = (rnd() - 0.5f) * 12.0f;
        g_randomCubes[i].posZ = (rnd() - 0.5f) * 12.0f;
        g_randomCubes[i].rotSpeedX = (rnd() - 0.5f) * 2.0f;
        g_randomCubes[i].rotSpeedY = (rnd() - 0.5f) * 2.0f;
        g_randomCubes[i].rotSpeedZ = (rnd() - 0.5f) * 2.0f;
        g_randomCubes[i].currentRotX = 0.0f;
        g_randomCubes[i].currentRotY = 0.0f;
        g_randomCubes[i].currentRotZ = 0.0f;
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    g_camera.processMouseMovement((float)xpos, (float)ypos);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    g_camera.distance -= (float)yoffset * 0.5f;
    if (g_camera.distance < 1.0f)
        g_camera.distance = 1.0f;
    if (g_camera.distance > 20.0f)
        g_camera.distance = 20.0f;
}

int main()
{
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

    // Setup mouse input
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Capture cursor

    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to init GLEW\n");
        return -1;
    }

    // Initialize random cubes
    initRandomCubes();

    // Simple cube data (positions + colors) - the spinning inner cube
    float vertices[] = {
        // positions         // colors
        -0.5f, -0.5f, -0.5f, 1, 0, 0,
        0.5f, -0.5f, -0.5f, 0, 1, 0,
        0.5f, 0.5f, -0.5f, 0, 0, 1,
        -0.5f, 0.5f, -0.5f, 1, 1, 0,
        -0.5f, -0.5f, 0.5f, 1, 0, 1,
        0.5f, -0.5f, 0.5f, 0, 1, 1,
        0.5f, 0.5f, 0.5f, 1, 1, 1,
        -0.5f, 0.5f, 0.5f, 0, 0, 0};
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0,
        2, 3, 7, 7, 6, 2,
        0, 3, 7, 7, 4, 0,
        1, 2, 6, 6, 5, 1};

    // World cube (large static room) - inverted winding order to see inside
    // Each face is a distinct bright color (expanded vertices for per-face colors)
    float worldVertices[] = {
        // Back face (looking in) - BLUE
        -10.0f, -10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
         10.0f, -10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
         10.0f,  10.0f, -10.0f,  0.3f, 0.4f, 0.9f,
        -10.0f,  10.0f, -10.0f,  0.3f, 0.4f, 0.9f,

        // Front face - RED
        -10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
         10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
         10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.3f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.3f,

        // Bottom face - GREEN
        -10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.4f,
         10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.4f,
         10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.4f,
        -10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.4f,

        // Top face - YELLOW
        -10.0f,  10.0f, -10.0f,  0.9f, 0.9f, 0.3f,
         10.0f,  10.0f, -10.0f,  0.9f, 0.9f, 0.3f,
         10.0f,  10.0f,  10.0f,  0.9f, 0.9f, 0.3f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.9f, 0.3f,

        // Left face - MAGENTA
        -10.0f, -10.0f, -10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f,  10.0f, -10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f,  10.0f,  10.0f,  0.9f, 0.3f, 0.9f,
        -10.0f, -10.0f,  10.0f,  0.9f, 0.3f, 0.9f,

        // Right face - CYAN
         10.0f, -10.0f, -10.0f,  0.3f, 0.9f, 0.9f,
         10.0f,  10.0f, -10.0f,  0.3f, 0.9f, 0.9f,
         10.0f,  10.0f,  10.0f,  0.3f, 0.9f, 0.9f,
         10.0f, -10.0f,  10.0f,  0.3f, 0.9f, 0.9f
    };
    // Inverted winding order (clockwise instead of counter-clockwise)
    unsigned int worldIndices[] = {
        // Back face
        0, 2, 1, 0, 3, 2,
        // Front face
        4, 6, 5, 4, 7, 6,
        // Bottom face
        8, 10, 9, 8, 11, 10,
        // Top face
        12, 14, 13, 12, 15, 14,
        // Left face
        16, 18, 17, 16, 19, 18,
        // Right face
        20, 22, 21, 20, 23, 22
    };

    // Light cube (small white cube at light position)
    float lightVertices[] = {
        // positions (scaled to 0.5)   // white color
        -0.25f, -0.25f, -0.25f, 1, 1, 1,
         0.25f, -0.25f, -0.25f, 1, 1, 1,
         0.25f,  0.25f, -0.25f, 1, 1, 1,
        -0.25f,  0.25f, -0.25f, 1, 1, 1,
        -0.25f, -0.25f,  0.25f, 1, 1, 1,
         0.25f, -0.25f,  0.25f, 1, 1, 1,
         0.25f,  0.25f,  0.25f, 1, 1, 1,
        -0.25f,  0.25f,  0.25f, 1, 1, 1
    };
    // Same indices as regular cube
    unsigned int lightIndices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0,
        2, 3, 7, 7, 6, 2,
        0, 3, 7, 7, 4, 0,
        1, 2, 6, 6, 5, 1
    };

    // Setup spinning cube VAO
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

    // Setup world cube VAO
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

    // Setup light cube VAO
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

    // Load and compile shaders from files
    std::string vertexShaderSource = loadShaderFile("src/shaders/vertex.glsl");
    std::string fragmentShaderSource = loadShaderFile("src/shaders/fragment.glsl");

    if (vertexShaderSource.empty() || fragmentShaderSource.empty())
    {
        fprintf(stderr, "Failed to load shader files\n");
        return -1;
    }

    auto compile = [](GLenum type, const char *src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
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
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    // Check program linking
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char buf[512];
        glGetProgramInfoLog(prog, 512, NULL, buf);
        fprintf(stderr, "Program link error: %s\n", buf);
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    float t0 = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        float t = (float)glfwGetTime();
        float dt = t - t0;
        t0 = t;

        // Handle ESC key to toggle mouse capture
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            static bool cursorDisabled = true;
            cursorDisabled = !cursorDisabled;
            glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            g_camera.firstMouse = true; // Reset to avoid jump on re-capture

            // Small delay to avoid rapid toggling
            glfwWaitEventsTimeout(0.2);
        }

        // Handle +/- keys for light brightness
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            g_lightBrightness += dt * 2.0f; // Increase brightness
            if (g_lightBrightness > 5.0f) g_lightBrightness = 5.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            g_lightBrightness -= dt * 2.0f; // Decrease brightness
            if (g_lightBrightness < 0.1f) g_lightBrightness = 0.1f;
        }

        glViewport(0, 0, 800, 600);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);

        // Setup common matrices
        Mat4 proj = perspective(3.1415f * 0.4f, 800.0f / 600.0f, 0.1f, 100.0f);
        Mat4 view = g_camera.getViewMatrix();
        static float angle = 0;
        angle += dt;

        // Calculate camera position from yaw, pitch, distance
        float yawRad = g_camera.yaw * 3.14159f / 180.0f;
        float pitchRad = g_camera.pitch * 3.14159f / 180.0f;
        float camX = g_camera.distance * cosf(pitchRad) * cosf(yawRad);
        float camY = g_camera.distance * sinf(pitchRad);
        float camZ = g_camera.distance * cosf(pitchRad) * sinf(yawRad);

        // Upload light and camera positions
        GLint lightPosLoc = glGetUniformLocation(prog, "uLightPos");
        GLint viewPosLoc = glGetUniformLocation(prog, "uViewPos");
        GLint brightLoc = glGetUniformLocation(prog, "uBrightness");
        glUniform3f(lightPosLoc, g_lightPosX, g_lightPosY, g_lightPosZ);
        glUniform3f(viewPosLoc, camX, camY, camZ);
        glUniform1f(brightLoc, g_lightBrightness);

        GLint mvpLoc = glGetUniformLocation(prog, "uMVP");
        GLint modelLoc = glGetUniformLocation(prog, "uModel");
        GLint emissiveLoc = glGetUniformLocation(prog, "uIsEmissive");

        // Draw world cube (stationary) - not emissive
        glUniform1i(emissiveLoc, 0);
        Mat4 worldModel = identity();
        Mat4 worldMvp = mul(proj, mul(view, worldModel));
        float worldMvpColMajor[16], worldModelColMajor[16];
        worldMvp.toColumnMajor(worldMvpColMajor);
        worldModel.toColumnMajor(worldModelColMajor);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, worldMvpColMajor);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, worldModelColMajor);
        glBindVertexArray(worldVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Draw light cube at light position - IS emissive (pure white)
        glUniform1i(emissiveLoc, 1);
        Mat4 lightTranslate = translate(g_lightPosX, g_lightPosY, g_lightPosZ);
        Mat4 lightMvp = mul(proj, mul(view, lightTranslate));
        float lightMvpColMajor[16], lightModelColMajor[16];
        lightMvp.toColumnMajor(lightMvpColMajor);
        lightTranslate.toColumnMajor(lightModelColMajor);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, lightMvpColMajor);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, lightModelColMajor);
        glBindVertexArray(lightVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Draw random cubes - not emissive
        glUniform1i(emissiveLoc, 0);
        for (int i = 0; i < NUM_RANDOM_CUBES; i++)
        {
            // Update rotations
            g_randomCubes[i].currentRotX += g_randomCubes[i].rotSpeedX * dt;
            g_randomCubes[i].currentRotY += g_randomCubes[i].rotSpeedY * dt;
            g_randomCubes[i].currentRotZ += g_randomCubes[i].rotSpeedZ * dt;

            // Build transformation
            Mat4 rX = rotateX(g_randomCubes[i].currentRotX);
            Mat4 rY = rotateY(g_randomCubes[i].currentRotY);
            Mat4 rZ = rotateZ(g_randomCubes[i].currentRotZ);
            Mat4 pos = translate(g_randomCubes[i].posX, g_randomCubes[i].posY, g_randomCubes[i].posZ);
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

        glfwSwapBuffers(window);
        glfwPollEvents();

        // HUD: Print camera and light info to console (every 30 frames)
        static int frameCount = 0;
        if (++frameCount >= 30) {
            frameCount = 0;
            printf("\r[CAM] Yaw:%.1f Pitch:%.1f Dist:%.1f | [LIGHT] Brightness:%.2f Pos:(%.1f,%.1f,%.1f)   ",
                   g_camera.yaw, g_camera.pitch, g_camera.distance,
                   g_lightBrightness, g_lightPosX, g_lightPosY, g_lightPosZ);
            fflush(stdout);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}