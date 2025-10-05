#!/bin/bash

# Exit on error
set -e

# Get script directory
PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
OUT_DIR="$PROJ_ROOT/build"

# Clean build
if [[ "$1" == "-clean" || "$1" == "-Clean" ]]; then
    if [ -d "$OUT_DIR" ]; then
        echo "Removing directory $OUT_DIR"
        rm -rf "$OUT_DIR"
    fi
    exit 0
fi

# Test build
if [[ "$1" == "-test" || "$1" == "--test" ]]; then
    TEST_SRC_FILE="test_math.cpp"
    TEST_OUT_EXE="$OUT_DIR/test_runner"

    echo "Building and running tests..."
    mkdir -p "$OUT_DIR"

    # Change to the tests directory to compile
    cd "$PROJ_ROOT/tests"

    g++ -std=c++17 -O2 \
        "$TEST_SRC_FILE" \
        -o "$TEST_OUT_EXE"

    echo "Running tests..."
    "$TEST_OUT_EXE"
    exit 0
fi

# Normal build
SRC_FILE="$PROJ_ROOT/src/main.cpp"
OUT_EXE="$OUT_DIR/a.out"

# Create output directory
mkdir -p "$OUT_DIR"

# Check for g++
if ! command -v g++ &> /dev/null; then
    echo "g++ not found. Please install build-essential and try again."
    exit 1
fi

echo "Using compiler: $(command -v g++)"

# Compile
echo "Compiling..."
g++ -std=c++17 -O2 \
    -I/usr/include \
    -L/usr/lib/x86_64-linux-gnu \
    "$SRC_FILE" \
    -o "$OUT_EXE" \
    -lGLEW -lglfw -lGL -lm -ldl -lpthread -lX11

echo "Build successful: $OUT_EXE"
exit 0