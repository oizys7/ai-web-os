if (AI_WEB_OS_HAL_NATIVE)
    add_library(aiwos_hal_native STATIC
        src/hal_native.c
    )

    target_include_directories(aiwos_hal_native
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/abi
    )
endif()

