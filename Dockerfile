# ═══════════════════════════════════════════════════════════
# eAI Containerized HAL Testing — Multi-stage Dockerfile
# ═══════════════════════════════════════════════════════════
#
# Usage:
#   docker build --target test-runner -t eai-test .
#   docker run --rm eai-test
#
# Build args:
#   BUILD_TYPE     = Debug | Release | MinSizeRel (default: Release)
#   EAI_ACCEL      = ON | OFF (default: ON)
#   EAI_FORMATS    = ON | OFF (default: ON)
#   EAI_BCI        = ON | OFF (default: ON)
#   CC_COMPILER    = gcc | clang (default: gcc)
#
# Targets:
#   builder        — build stage with all object files
#   test-runner    — slim image that runs the test suite
#   dev            — full dev image with source + tools
# ═══════════════════════════════════════════════════════════

# ── Stage 1: Builder ──────────────────────────────────────
FROM gcc:13-bookworm AS builder-gcc
FROM llvm/clang:17 AS builder-clang

# Default to GCC
FROM builder-gcc AS builder-base

ARG BUILD_TYPE=Release
ARG EAI_ACCEL=ON
ARG EAI_FORMATS=ON
ARG EAI_BCI=ON
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /eai
COPY . .

RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DEAI_BUILD_TESTS=ON \
    -DEAI_BUILD_ACCEL=${EAI_ACCEL} \
    -DEAI_BUILD_FORMATS=${EAI_FORMATS} \
    -DEAI_BUILD_MIN=ON \
    -DEAI_BUILD_FRAMEWORK=ON \
    -DEAI_BUILD_BCI=${EAI_BCI} \
    -DEAI_BUILD_CLI=ON \
    -DEAI_PLATFORM_CONTAINER=ON \
    && cmake --build build --parallel $(nproc)

# ── Stage 2: Test Runner (minimal) ───────────────────────
FROM debian:bookworm-slim AS test-runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libm-dev \
    && rm -rf /var/lib/apt/lists/* \
    || true

WORKDIR /eai

# Copy only test binaries and CTest config
COPY --from=builder-base /eai/build/tests/ /eai/tests/
COPY --from=builder-base /eai/build/CTestTestfile.cmake /eai/
COPY --from=builder-base /eai/build/tests/CTestTestfile.cmake /eai/tests/

# Container detection markers — the platform detect test should find this
# (/.dockerenv is created by Docker automatically, but add a fallback)
RUN touch /.eai-container-test

# Health check: verify at least one test binary exists and runs
RUN ls /eai/tests/eai_test_common && /eai/tests/eai_test_common

# Default: run all tests
CMD ["sh", "-c", "\
echo '═══════════════════════════════════════' && \
echo '  eAI Containerized HAL Test Suite' && \
echo '═══════════════════════════════════════' && \
echo '' && \
PASS=0 && FAIL=0 && \
for t in /eai/tests/eai_test_*; do \
    NAME=$(basename $t); \
    if [ -x \"$t\" ]; then \
        echo \"--- $NAME ---\"; \
        if $t; then \
            PASS=$((PASS + 1)); \
        else \
            FAIL=$((FAIL + 1)); \
        fi; \
        echo ''; \
    fi; \
done && \
echo '═══════════════════════════════════════' && \
echo \"  Passed: $PASS  Failed: $FAIL\" && \
echo '═══════════════════════════════════════' && \
[ $FAIL -eq 0 ]"]

# ── Stage 3: Dev Image (full) ────────────────────────────
FROM builder-base AS dev

RUN apt-get update && apt-get install -y --no-install-recommends \
    gdb \
    valgrind \
    cppcheck \
    clang-format \
    git \
    && rm -rf /var/lib/apt/lists/*

# Run tests on build to validate
RUN cd build && ctest --output-on-failure --parallel $(nproc)

CMD ["/bin/bash"]
