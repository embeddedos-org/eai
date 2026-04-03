# ARM Cortex-M4 Cross-Compilation Toolchain
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-cortex-m4.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -Wall -Wextra -Os")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -Wl,--print-memory-usage")

# MCU-optimized BCI settings
add_compile_definitions(
    EAI_BCI_MAX_CHANNELS=4
    EAI_BCI_RING_SIZE=128
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
