#!/bin/bash
set -e

# build.sh - Build the project using CMake
# Usage:
#   ./build.sh        # Build the project
#   ./build.sh -clean # Remove the build directory

# Check for -clean flag
if [ "$1" == "-clean" ]; then
    echo "Removing build directory..."
    rm -rf build
    exit 0
fi

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Run CMake and Make
echo "Configuring with CMake..."
cmake ..

echo "Building with Make..."
make

echo "Build successful! Executable is in the build/ directory."
echo "Shaders are in build/shaders/"