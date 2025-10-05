cmake_minimum_required(VERSION 3.10)
project(CelShadingExperiment)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLEW
find_package(GLEW REQUIRED)

# Find GLFW
find_package(glfw3 REQUIRED)

# Add the executable
add_executable(cel_shader src/main.cpp)

# Link libraries
target_link_libraries(cel_shader
    PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
)

# Copy shader files to the build directory
file(COPY src/shaders DESTINATION ${CMAKE_BINARY_DIR})