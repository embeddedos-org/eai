#!/bin/bash
# CI: Run all eAI tests
set -euo pipefail

echo "=== eAI Test Suite ==="

BUILD_DIR="${BUILD_DIR:-build-linux}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Run build-linux-native.sh first."
    exit 1
fi

cd "$BUILD_DIR"
ctest --output-on-failure --parallel "$(nproc)"

echo ""
echo "=== All tests completed ==="
