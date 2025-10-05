#!/bin/bash

# Exit on error
set -e

# Get script directory
PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
BUILD_DIR="$PROJ_ROOT/build"

# Clean build
if [[ "$1" == "-clean" || "$1" == "-Clean" ]]; then
    if [ -d "$BUILD_DIR" ]; then
        echo "Removing directory $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    exit 0
fi

# Configure and build
echo "Configuring with CMake..."
mkdir -p "$BUILD_DIR"
cmake -S "$PROJ_ROOT" -B "$BUILD_DIR"

echo "Building with make..."
make -C "$BUILD_DIR"

# Run tests
echo "Running tests..."
"$BUILD_DIR/tests/test_runner"

echo "Build and tests completed successfully."
exit 0