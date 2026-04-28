#!/bin/bash
# CI: Cross-compile eAI for ARM Cortex-M4
set -euo pipefail

echo "=== eAI ARM Cortex-M4 Build ==="

BUILD_DIR="${BUILD_DIR:-build-arm}"
TOOLCHAIN="cmake/toolchains/toolchain-arm-cortex-m4.cmake"

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
    -DEAI_BUILD_ACCEL=OFF \
    -DEAI_BUILD_FORMATS=OFF \
    -DEAI_HAL_THREAD=OFF \
    -DEAI_HAL_FS=OFF \
    -DEAI_HAL_NET=OFF \
    -DEAI_BCI_SIMULATOR=ON

cmake --build . --parallel "$(nproc)"

echo "=== ARM build complete ==="
echo "Memory usage:"
arm-none-eabi-size common/libeai_common.a || true
arm-none-eabi-size platform/libeai_platform.a || true
arm-none-eabi-size min/libeai_min.a || true
arm-none-eabi-size bci/libeai_bci.a || true
