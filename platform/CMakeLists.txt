add_library(eai_platform STATIC
    src/platform.c
    src/platform_linux.c
    src/platform_windows.c
    src/platform_container.c
    src/eai_api_platform.c
)

target_include_directories(eai_platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(eai_platform PUBLIC eai_common)

if(EAI_PLATFORM_EOS)
    target_sources(eai_platform PRIVATE src/platform_eos.c)
    target_compile_definitions(eai_platform PUBLIC EAI_PLATFORM_EOS_ENABLED)
endif()

# ========== New HAL sub-modules ==========

option(EAI_HAL_THREAD "Build HAL threading support" ON)
option(EAI_HAL_FS     "Build HAL filesystem support" ON)
option(EAI_HAL_NET    "Build HAL networking support"  ON)

if(WIN32)
    target_sources(eai_platform PRIVATE
        src/hal/windows/core_windows.c
        src/hal/windows/memory_windows.c
        src/hal/windows/thread_windows.c
        src/hal/windows/fs_windows.c
        src/hal/windows/net_windows.c
        src/hal/windows/timer_windows.c
    )
    target_link_libraries(eai_platform PRIVATE ws2_32 ntdll)
elseif(UNIX)
    target_sources(eai_platform PRIVATE
        src/hal/linux/core_linux.c
        src/hal/linux/memory_linux.c
        src/hal/linux/gpio_linux.c
        src/hal/posix/timer_posix.c
    )
    if(EAI_HAL_THREAD)
        target_sources(eai_platform PRIVATE src/hal/posix/thread_posix.c)
        target_link_libraries(eai_platform PRIVATE pthread)
    endif()
    if(EAI_HAL_FS)
        target_sources(eai_platform PRIVATE src/hal/posix/fs_posix.c)
    endif()
    if(EAI_HAL_NET)
        target_sources(eai_platform PRIVATE src/hal/posix/net_posix.c)
    endif()
endif()
