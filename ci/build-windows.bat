@echo off
REM CI: Build and test eAI on Windows
REM Requires: Visual Studio 2022+ with C/C++ workload, CMake 3.16+

echo === eAI Windows Build ===

cmake -B build ^
    -DEAI_BUILD_TESTS=ON ^
    -DEAI_BUILD_ACCEL=ON ^
    -DEAI_BUILD_FORMATS=ON ^
    -DEAI_BUILD_MIN=ON ^
    -DEAI_BUILD_FRAMEWORK=ON ^
    -DEAI_BUILD_BCI=ON ^
    -DEAI_BUILD_CLI=ON

if %ERRORLEVEL% NEQ 0 (
    echo Configure failed
    exit /b %ERRORLEVEL%
)

cmake --build build --config Release --parallel 4
if %ERRORLEVEL% NEQ 0 (
    echo Build failed
    exit /b %ERRORLEVEL%
)

echo.
echo Running tests...
ctest --test-dir build --output-on-failure -C Release --parallel 4
exit /b %ERRORLEVEL%
