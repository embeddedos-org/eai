add_library(eai_framework STATIC
    src/runtime_manager.c
    src/orchestrator.c
    src/connector_mqtt.c
    src/connector_opcua.c
    src/connector_modbus.c
    src/connector_can.c
    src/memory.c
    src/policy.c
    src/observability.c
    src/adaptive.c
    src/federated.c
    src/update_manager.c
    src/secure_boot.c
    src/supply_chain.c
    src/sensor_fusion.c
    src/network_security.c
    src/guardrails.c
)

target_include_directories(eai_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eai_framework PUBLIC eai_common)

if(UNIX)
    target_link_libraries(eai_framework PRIVATE m)
endif()
