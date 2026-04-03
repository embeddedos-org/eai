add_library(eai_bci STATIC
    src/signal.c
    src/device_simulator.c
    src/decoder_threshold.c
    src/output_log.c
    src/output_gpio.c
    src/pipeline.c
    src/device_manager.c
    src/plugin_registry.c
    src/bci_config.c
    src/device_openbci.c
    src/device_emotiv.c
    src/device_muse.c
    src/bci_eipc.c
    src/connector_bci.c
    src/api.c
    src/decoder_snn.c
)

target_include_directories(eai_bci PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eai_bci PUBLIC eai_common eai_platform)

if(UNIX)
    target_link_libraries(eai_bci PUBLIC m)
endif()

option(EAI_BCI_SIMULATOR "Enable BCI simulator device" ON)
if(EAI_BCI_SIMULATOR)
    target_compile_definitions(eai_bci PUBLIC EAI_BCI_SIMULATOR_ENABLED)
endif()

option(EAI_BCI_OPENBCI "Enable OpenBCI hardware adapter" OFF)
if(EAI_BCI_OPENBCI)
    target_compile_definitions(eai_bci PUBLIC EAI_BCI_OPENBCI_ENABLED)
endif()

option(EAI_BCI_EMOTIV "Enable Emotiv hardware adapter" OFF)
if(EAI_BCI_EMOTIV)
    target_compile_definitions(eai_bci PUBLIC EAI_BCI_EMOTIV_ENABLED)
endif()

option(EAI_BCI_MUSE "Enable Muse EEG hardware adapter" OFF)
if(EAI_BCI_MUSE)
    target_compile_definitions(eai_bci PUBLIC EAI_BCI_MUSE_ENABLED)
endif()

option(EAI_BCI_TINYML "Enable TinyML inference backend" OFF)
if(EAI_BCI_TINYML)
    target_compile_definitions(eai_bci PUBLIC EAI_BCI_TINYML_ENABLED)
endif()
