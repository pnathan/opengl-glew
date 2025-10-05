cmake_minimum_required(VERSION 3.10)
project(SpinningCube)

# Find OpenGL, GLEW, and GLFW
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLFW REQUIRED)

# Add source files
add_executable(spinning_cube src/main.cpp)

# Link libraries
target_link_libraries(spinning_cube PRIVATE OpenGL::GL GLEW::glew GLFW::glfw)

# Add tests subdirectory
add_subdirectory(tests)