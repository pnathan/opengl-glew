cmake_minimum_required(VERSION 3.10)

# Add the test runner executable
add_executable(test_runner test_math.cpp)

# Make sure it can find math.hpp
target_include_directories(test_runner PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Doctest is header-only, so no library to link, just need to add it to includes.
# The main test file includes doctest.h and provides a main()
target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})