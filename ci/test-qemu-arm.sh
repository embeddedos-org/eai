#!/bin/bash
# CI: Run eAI tests on ARM via QEMU user-mode emulation
# Usage: ci/test-qemu-arm.sh [ARCH]
# Supported: aarch64, arm, riscv64
set -euo pipefail

ARCH="${1:-aarch64}"

echo "=== eAI QEMU ARM Test ==="
echo "  Architecture: $ARCH"

case "$ARCH" in
    aarch64)
        QEMU=qemu-aarch64
        CROSS=aarch64-linux-gnu
        BUILD_DIR="build-linux-arm64"
        CC=aarch64-linux-gnu-gcc
        ;;
    arm)
        QEMU=qemu-arm
        CROSS=arm-linux-gnueabihf
        BUILD_DIR="build-linux-armhf"
        CC=arm-linux-gnueabihf-gcc
        ;;
    riscv64)
        QEMU=qemu-riscv64
        CROSS=riscv64-linux-gnu
        BUILD_DIR="build-linux-riscv64"
        CC=riscv64-linux-gnu-gcc
        ;;
    *)
        echo "Error: unsupported architecture '$ARCH'"
        echo "Supported: aarch64, arm, riscv64"
        exit 1
        ;;
esac

# Check prerequisites
command -v $QEMU >/dev/null 2>&1 || { echo "Error: $QEMU not installed. Run: sudo apt-get install qemu-user"; exit 1; }
command -v $CC >/dev/null 2>&1 || { echo "Error: $CC not installed. Run: sudo apt-get install gcc-$CROSS"; exit 1; }

# Build if needed
if [ ! -d "$BUILD_DIR" ]; then
    echo "Building for $ARCH..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. \
        -DCMAKE_C_COMPILER=$CC \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DEAI_BUILD_TESTS=ON \
        -DEAI_BUILD_ACCEL=ON \
        -DEAI_BUILD_FORMATS=ON \
        -DEAI_BUILD_MIN=ON \
        -DEAI_BUILD_FRAMEWORK=ON \
        -DEAI_BUILD_BCI=ON
    cmake --build . --parallel "$(nproc)"
    cd ..
fi

# Run tests via QEMU
echo ""
echo "=== Running tests via $QEMU ==="
export QEMU_LD_PREFIX="/usr/$CROSS"

PASS=0
FAIL=0
SKIP=0

for test_bin in $(find "$BUILD_DIR/tests" -maxdepth 1 -type f -executable 2>/dev/null); do
    NAME=$(basename "$test_bin")
    echo -n "  $NAME ... "

    if $QEMU -L "$QEMU_LD_PREFIX" "$test_bin" > /tmp/qemu_test_out.txt 2>&1; then
        RESULT=$(tail -1 /tmp/qemu_test_out.txt)
        echo "PASS ($RESULT)"
        PASS=$((PASS + 1))
    else
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 127 ]; then
            echo "SKIP (missing deps)"
            SKIP=$((SKIP + 1))
        else
            echo "FAIL (exit $EXIT_CODE)"
            cat /tmp/qemu_test_out.txt | grep -E '\[FAIL\]' || true
            FAIL=$((FAIL + 1))
        fi
    fi
done

echo ""
echo "═══════════════════════════════════════"
echo "  QEMU $ARCH Results"
echo "═══════════════════════════════════════"
echo "  Passed:  $PASS"
echo "  Failed:  $FAIL"
echo "  Skipped: $SKIP"
echo "═══════════════════════════════════════"

if [ $FAIL -gt 0 ]; then
    echo "❌ Some tests failed"
    exit 1
fi
echo "✅ All tests passed"
