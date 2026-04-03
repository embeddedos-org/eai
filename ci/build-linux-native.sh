#!/bin/bash
# CI: Build eAI with BCI on native Linux x86_64
set -euo pipefail

echo "=== eAI BCI Build (Linux x86_64) ==="

BUILD_DIR="${BUILD_DIR:-build-linux}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DEAI_BUILD_MIN=ON \
    -DEAI_BUILD_FRAMEWORK=ON \
    -DEAI_BUILD_BCI=ON \
    -DEAI_BUILD_CLI=ON \
    -DEAI_BUILD_TESTS=ON \
    -DEAI_BCI_SIMULATOR=ON

cmake --build . --parallel "$(nproc)"

echo "=== Build complete ==="
