#!/bin/bash
# CI: Build eAI on macOS
set -euo pipefail

echo "=== eAI macOS Build ==="

ARCH=$(uname -m)
BUILD_DIR="build-macos-${ARCH}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DEAI_BUILD_TESTS=ON \
    -DEAI_BUILD_ACCEL=ON \
    -DEAI_BUILD_FORMATS=ON \
    -DEAI_BUILD_MIN=ON \
    -DEAI_BUILD_FRAMEWORK=ON \
    -DEAI_BUILD_BCI=ON \
    -DEAI_BUILD_CLI=ON \
    -DEAI_PLATFORM_MACOS=ON

cmake --build . --parallel $(sysctl -n hw.ncpu)

echo ""
echo "Running tests..."
ctest --output-on-failure --parallel $(sysctl -n hw.ncpu)

echo "=== macOS $ARCH build complete ==="
