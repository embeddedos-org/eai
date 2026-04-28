#!/bin/bash
# CI: Build eAI for Android using NDK
# Usage: ci/build-android.sh [ABI] [API_LEVEL]
set -euo pipefail

ABI="${1:-arm64-v8a}"
API="${2:-26}"
BUILD_DIR="build-android-${ABI}"

echo "=== eAI Android Build ==="
echo "  ABI:       $ABI"
echo "  API Level: $API"
echo "  NDK:       ${ANDROID_NDK_HOME:-not set}"

if [ -z "${ANDROID_NDK_HOME:-}" ]; then
    echo "Error: ANDROID_NDK_HOME not set"
    echo "Install NDK: sdkmanager 'ndk;26.1.10909125'"
    exit 1
fi

TOOLCHAIN="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "Error: NDK toolchain not found at $TOOLCHAIN"
    exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_NATIVE_API_LEVEL="$API" \
    -DEAI_BUILD_TESTS=OFF \
    -DEAI_BUILD_CLI=OFF \
    -DEAI_BUILD_ACCEL=ON \
    -DEAI_BUILD_FORMATS=ON \
    -DEAI_BUILD_MIN=ON \
    -DEAI_BUILD_FRAMEWORK=OFF \
    -DEAI_BUILD_BCI=OFF

cmake --build . --parallel "$(nproc)"

echo ""
echo "=== Android $ABI build complete ==="
echo "Libraries:"
find . -name '*.a' -exec ls -lh {} \;
