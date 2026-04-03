add_executable(eai_test_common test_common.c)
target_link_libraries(eai_test_common PRIVATE eai_common)
add_test(NAME eai_common_tests COMMAND eai_test_common)

add_executable(eai_test_min test_min.c)
target_link_libraries(eai_test_min PRIVATE eai_common eai_min)
add_test(NAME eai_min_tests COMMAND eai_test_min)

add_executable(eai_test_framework test_framework.c)
target_link_libraries(eai_test_framework PRIVATE eai_common eai_framework)
add_test(NAME eai_framework_tests COMMAND eai_test_framework)

# --- test_tool: Tool registry ---
add_executable(eai_test_tool test_tool.c)
target_link_libraries(eai_test_tool PRIVATE eai_common)
add_test(NAME eai_tool_tests COMMAND eai_test_tool)

# --- test_config: Configuration ---
add_executable(eai_test_config test_config.c)
target_link_libraries(eai_test_config PRIVATE eai_common)
add_test(NAME eai_config_tests COMMAND eai_test_config)

# --- test_agent: Min agent ---
add_executable(eai_test_agent test_agent.c)
target_link_libraries(eai_test_agent PRIVATE eai_common eai_min)
add_test(NAME eai_agent_tests COMMAND eai_test_agent)

# --- test_memory: Memory lite ---
add_executable(eai_test_memory test_memory.c)
target_link_libraries(eai_test_memory PRIVATE eai_common eai_min)
add_test(NAME eai_memory_tests COMMAND eai_test_memory)

# --- test_models: Model catalog ---
add_executable(eai_test_models test_models.c)
target_link_libraries(eai_test_models PRIVATE eai_common)
add_test(NAME eai_model_tests COMMAND eai_test_models)

# --- test_adaptive: Adaptive/Personalized AI ---
add_executable(eai_test_adaptive test_adaptive.c)
target_link_libraries(eai_test_adaptive PRIVATE eai_common)
add_test(NAME eai_adaptive_tests COMMAND eai_test_adaptive)

# --- BCI tests ---
if(EAI_BUILD_BCI)
    add_executable(eai_test_bci_signal test_bci_signal.c)
    target_link_libraries(eai_test_bci_signal PRIVATE eai_common eai_platform eai_bci)
    add_test(NAME eai_bci_signal_tests COMMAND eai_test_bci_signal)

    add_executable(eai_test_bci_device test_bci_device.c)
    target_link_libraries(eai_test_bci_device PRIVATE eai_common eai_platform eai_bci)
    add_test(NAME eai_bci_device_tests COMMAND eai_test_bci_device)

    add_executable(eai_test_bci_pipeline test_bci_pipeline.c)
    target_link_libraries(eai_test_bci_pipeline PRIVATE eai_common eai_platform eai_bci)
    add_test(NAME eai_bci_pipeline_tests COMMAND eai_test_bci_pipeline)

    if(UNIX)
        target_link_libraries(eai_test_bci_signal PRIVATE m)
        target_link_libraries(eai_test_bci_device PRIVATE m)
        target_link_libraries(eai_test_bci_pipeline PRIVATE m)
    endif()
endif()
