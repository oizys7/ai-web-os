add_executable(aiwos_native_hello
    main.c
)

target_include_directories(aiwos_native_hello
    PRIVATE
        ${CMAKE_SOURCE_DIR}/abi
        ${CMAKE_SOURCE_DIR}/hal/native/include
)

target_link_libraries(aiwos_native_hello
    PRIVATE
        aiwos_core
        aiwos_hal_native
)

