#!/bin/bash
# CI: Build all platforms available on this host
set -euo pipefail

echo "=== eAI Build All ==="

OS=$(uname -s)
ARCH=$(uname -m)

case "$OS" in
    Linux)
        echo "--- Linux $ARCH ---"
        bash ci/build-linux-native.sh

        # Cross-compile if toolchains available
        if command -v aarch64-linux-gnu-gcc &>/dev/null; then
            echo ""
            echo "--- Linux ARM64 (cross) ---"
            BUILD_DIR=build-linux-arm64 bash -c '
                mkdir -p $BUILD_DIR && cd $BUILD_DIR
                cmake .. -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
                    -DCMAKE_SYSTEM_NAME=Linux -DEAI_BUILD_TESTS=ON \
                    -DEAI_BUILD_ACCEL=ON -DEAI_BUILD_FORMATS=ON
                cmake --build . --parallel $(nproc)
            '
        fi

        if command -v arm-none-eabi-gcc &>/dev/null; then
            echo ""
            echo "--- ARM Cortex-M4 (cross) ---"
            bash ci/build-arm-cross.sh
        fi
        ;;
    Darwin)
        echo "--- macOS $ARCH ---"
        bash ci/build-macos.sh
        ;;
    MINGW*|MSYS*|CYGWIN*)
        echo "--- Windows ---"
        cmd //c ci\\build-windows.bat
        ;;
    *)
        echo "Unknown OS: $OS"
        exit 1
        ;;
esac

echo ""
echo "=== Build All Complete ==="
