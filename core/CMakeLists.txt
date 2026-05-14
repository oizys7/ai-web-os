add_library(aiwos_core STATIC
    src/core_logic.c
    src/scheduler.c
    src/aiwos_module.c
)

target_include_directories(aiwos_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/abi
)

