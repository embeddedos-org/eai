#!/bin/bash
# CI: Run the full containerized HAL test matrix
#
# Usage:
#   ci/test-docker-matrix.sh                  # run all
#   ci/test-docker-matrix.sh gcc clang        # run subset
#   ci/test-docker-matrix.sh --parallel       # run all in parallel
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

TARGETS=("gcc" "clang" "alpine" "asan" "arm64")
PARALLEL=false

# Parse args
if [ $# -gt 0 ]; then
    if [ "$1" = "--parallel" ]; then
        PARALLEL=true
        shift
    fi
    if [ $# -gt 0 ]; then
        TARGETS=("$@")
    fi
fi

echo "═══════════════════════════════════════════"
echo "  eAI Docker HAL Test Matrix"
echo "═══════════════════════════════════════════"
echo "  Targets: ${TARGETS[*]}"
echo "  Parallel: $PARALLEL"
echo ""

PASS=0
FAIL=0
RESULTS=()

run_target() {
    local target=$1
    echo "▶ Building $target..."

    if docker compose build "$target" 2>&1 | tail -3; then
        echo "▶ Running $target..."
        if docker compose run --rm "$target" 2>&1; then
            echo "✅ $target: PASSED"
            return 0
        else
            echo "❌ $target: FAILED"
            return 1
        fi
    else
        echo "❌ $target: BUILD FAILED"
        return 1
    fi
}

if $PARALLEL; then
    # Build all images first
    echo "Building all images..."
    docker compose build "${TARGETS[@]}"
    echo ""

    # Run all targets in parallel
    PIDS=()
    for target in "${TARGETS[@]}"; do
        (
            docker compose run --rm "$target" > "/tmp/eai-docker-$target.log" 2>&1
            echo $? > "/tmp/eai-docker-$target.exit"
        ) &
        PIDS+=($!)
        echo "  Launched $target (PID $!)"
    done

    echo "Waiting for all targets..."
    for i in "${!PIDS[@]}"; do
        wait "${PIDS[$i]}" || true
        target="${TARGETS[$i]}"
        EXIT_CODE=$(cat "/tmp/eai-docker-$target.exit" 2>/dev/null || echo 1)
        if [ "$EXIT_CODE" = "0" ]; then
            RESULTS+=("✅ $target")
            PASS=$((PASS + 1))
        else
            RESULTS+=("❌ $target")
            FAIL=$((FAIL + 1))
            echo ""
            echo "--- $target output (last 20 lines) ---"
            tail -20 "/tmp/eai-docker-$target.log" 2>/dev/null || true
        fi
    done
else
    # Run sequentially
    for target in "${TARGETS[@]}"; do
        echo ""
        echo "━━━ $target ━━━"
        if run_target "$target"; then
            RESULTS+=("✅ $target")
            PASS=$((PASS + 1))
        else
            RESULTS+=("❌ $target")
            FAIL=$((FAIL + 1))
        fi
    done
fi

# Summary
echo ""
echo "═══════════════════════════════════════════"
echo "  Docker HAL Test Results"
echo "═══════════════════════════════════════════"
for r in "${RESULTS[@]}"; do
    echo "  $r"
done
echo "═══════════════════════════════════════════"
echo "  Passed: $PASS / $((PASS + FAIL))"
echo "═══════════════════════════════════════════"

# Cleanup
docker compose down 2>/dev/null || true
rm -f /tmp/eai-docker-*.log /tmp/eai-docker-*.exit

if [ $FAIL -gt 0 ]; then
    exit 1
fi
