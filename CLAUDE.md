# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a cel shading experimentation project using modern OpenGL 3.3+ with C++. The program renders multiple spinning cubes with cartoon-style (cel-shaded) lighting inside a colored room environment, with an orbital camera controlled by mouse look.

## Build System

**Build command:**
```powershell
.\build.ps1
```

**Clean build:**
```powershell
.\build.ps1 -Clean
```

**Run the program:**
```powershell
.\build\a.exe
```

The build script (PowerShell) compiles with clang++ and handles:
- Auto-detection of GLEW/GLFW in `%USERPROFILE%\lib` (or uses `GLEW_DIR` and `GLFW_DIR` environment variables)
- Compiling GLEW from source with `-DGLEW_STATIC` flag when source is available
- Linking against: glfw3, opengl32, gdi32, user32, kernel32, shell32
- Using `/NODEFAULTLIB:libcmt` to avoid CRT conflicts
- Copying necessary DLLs to the build directory

## Architecture

### Graphics Pipeline

The program uses modern OpenGL's programmable pipeline:

1. **Vertex Data** → VBO/VAO/EBO → GPU memory
2. **Vertex Shader** (`src/shaders/vertex.glsl`) → transforms vertices, calculates normals
3. **Rasterization** → GPU interpolates across triangles
4. **Fragment Shader** (`src/shaders/fragment.glsl`) → cel shading with quantized lighting levels
5. **Framebuffer** → final rendered image

### Matrix Mathematics (Critical Design Decision)

**Storage format:** Matrices are stored in **ROW-MAJOR** format internally (human-readable) but converted to **COLUMN-MAJOR** when uploading to OpenGL via `Mat4::toColumnMajor()`.

This design choice provides:
- Human readability during development/debugging
- GPU efficiency when uploaded
- Explicit conversion prevents subtle bugs

### Shader System

**Uniforms passed to shaders:**
- `uMVP`: Model-View-Projection matrix (transforms vertices to clip space)
- `uModel`: Model matrix alone (needed for normal transformation)
- `uLightPos`: Light position in world space
- `uViewPos`: Camera position (for rim lighting)
- `uBrightness`: Light intensity multiplier (adjustable with +/- keys)
- `uIsEmissive`: Flag (1 = emissive/self-illuminated, 0 = receives lighting)

**Cel Shading Implementation:**
- Quantizes lighting into 4 discrete bands: 1.0, 0.7, 0.4, 0.2
- Distance attenuation: `1 / (1 + 0.09*d + 0.032*d²)`
- Rim lighting for edge highlights (cartoon effect)
- Emissive objects bypass lighting calculations

### Scene Objects

The program renders three types of geometry with separate VAO/VBO/EBO:

1. **World Cube** (20×20×20 room):
   - Inverted winding order (clockwise) to see interior faces
   - Per-face colors (24 vertices total, 4 per face) for uniform wall colors
   - Each wall: Blue, Red, Green, Yellow, Magenta, Cyan

2. **Light Cube** (0.5×0.5×0.5):
   - Visual indicator of light source position
   - Rendered as emissive (pure white, no lighting)

3. **Random Spinning Cubes** (8 cubes, unit size):
   - Randomly positioned within -6 to +6 range
   - Independent rotation speeds on all 3 axes
   - Receive cel-shaded lighting

### Camera System

**Orbital Camera using Spherical Coordinates:**
- `yaw`: Horizontal rotation (left/right)
- `pitch`: Vertical rotation (up/down), clamped to ±89° to prevent gimbal lock
- `distance`: Zoom (1.0 to 20.0 units from origin)

The camera always looks at the origin (0,0,0). Spherical coordinates are converted to Cartesian for the view matrix using a lookAt algorithm.

**Controls:**
- Mouse movement: Rotate camera
- Mouse wheel: Zoom in/out
- ESC: Toggle mouse capture
- +/- keys: Adjust light brightness

### Coordinate Spaces

Vertex transformation pipeline:
1. **Model Space** → local object coordinates
2. **World Space** → (Model matrix) → positioned in scene
3. **View Space** → (View matrix) → relative to camera
4. **Clip Space** → (Projection matrix) → perspective projection

Combined as: `MVP = Projection × View × Model` (applied right-to-left)

## Key Technical Details

**Normal Transformation:**
Normals are transformed by extracting the upper-left 3×3 of the model matrix (rotation/scale only, no translation):
```glsl
vNormal = mat3(uModel) * normalize(aPos);
```

**Vertex Attribute Layout:**
Interleaved format (6 floats per vertex):
- Attribute 0: Position (3 floats) at offset 0
- Attribute 1: Color (3 floats) at offset 12 bytes
- Stride: 24 bytes

**Indexed Drawing:**
Uses EBO (Element Buffer Object) for vertex reuse. Each cube has 8 vertices referenced by 36 indices (12 triangles = 6 faces × 2 triangles).

**Depth Testing:**
Enabled via `glEnable(GL_DEPTH_TEST)`. Depth buffer is cleared each frame alongside color buffer to correctly handle overlapping geometry.

## Common Modifications

**Adjusting cel shading bands:**
Edit `src/shaders/fragment.glsl` lines 31-35 to change lighting quantization thresholds.

**Changing number of random cubes:**
Modify `NUM_RANDOM_CUBES` constant in `src/main.cpp` (currently 8).

**Modifying camera behavior:**
Edit `Camera` struct and its methods `processMouseMovement()` and `getViewMatrix()` in `src/main.cpp`.

**Adding new shader uniforms:**
1. Declare in `.glsl` file
2. Get location with `glGetUniformLocation(prog, "uniformName")`
3. Set value with appropriate `glUniform*()` call before drawing

## Dependencies

- **GLEW** (OpenGL Extension Wrangler): Loads modern OpenGL function pointers
- **GLFW** (Graphics Library Framework): Window creation, OpenGL context, input handling
- **OpenGL 3.3 Core Profile**: Modern programmable pipeline (no legacy fixed-function)

Libraries should be in `%USERPROFILE%\lib\` or set via `GLEW_DIR`/`GLFW_DIR` environment variables.
