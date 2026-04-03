#!/bin/bash
# CI: Cross-compile eAI BCI for ARM Cortex-M4
set -euo pipefail

echo "=== eAI BCI Build (ARM Cortex-M4) ==="

BUILD_DIR="${BUILD_DIR:-build-arm}"
TOOLCHAIN="cmake/toolchain-arm-cortex-m4.cmake"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DEAI_BUILD_MIN=ON \
    -DEAI_BUILD_FRAMEWORK=OFF \
    -DEAI_BUILD_BCI=ON \
    -DEAI_BUILD_CLI=OFF \
    -DEAI_BUILD_TESTS=OFF \
    -DEAI_BCI_SIMULATOR=ON

cmake --build . --parallel "$(nproc)"

echo "=== ARM build complete ==="
echo "Memory usage:"
arm-none-eabi-size bci/libeai_bci.a || true
