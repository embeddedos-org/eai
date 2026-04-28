add_library(eai_accel STATIC
    src/tensor.c
    src/accel.c
    src/backend_cpu.c
)

target_include_directories(eai_accel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(eai_accel PUBLIC eai_common)

if(UNIX)
    target_link_libraries(eai_accel PRIVATE m)
endif()

if(EAI_ACCEL_VULKAN)
    target_sources(eai_accel PRIVATE src/backend_vulkan.c)
    target_compile_definitions(eai_accel PUBLIC EAI_ACCEL_VULKAN)
endif()

if(EAI_ACCEL_COREML AND APPLE)
    target_sources(eai_accel PRIVATE src/backend_coreml.m)
    target_compile_definitions(eai_accel PUBLIC EAI_ACCEL_COREML)
    target_link_libraries(eai_accel PRIVATE "-framework Foundation" "-framework CoreML")
endif()

if(EAI_ACCEL_QNN)
    target_sources(eai_accel PRIVATE src/backend_qnn.c)
    target_compile_definitions(eai_accel PUBLIC EAI_ACCEL_QNN)
endif()
