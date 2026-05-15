set(CORE_RUST_DIR "${CMAKE_SOURCE_DIR}/core")
set(RUST_STATICLIB "${CORE_RUST_DIR}/target/release/aiwos_core.lib")

add_custom_target(aiwos_core_rust ALL
    COMMAND cargo build --release
    WORKING_DIRECTORY "${CORE_RUST_DIR}"
    COMMENT "Building aiwos-core Rust staticlib"
    BYPRODUCTS "${RUST_STATICLIB}"
)

add_executable(aiwos_native_hello main.c)

add_dependencies(aiwos_native_hello aiwos_core_rust)

target_include_directories(aiwos_native_hello
    PRIVATE
        ${CMAKE_SOURCE_DIR}/abi
        ${CMAKE_SOURCE_DIR}/hal/native/include
)

# Rust stdlib on Windows depends on these system libraries
set(RUST_WIN_DEPS
    advapi32
    bcrypt
    cfgmgr32
    kernel32
    ntdll
    ole32
    secur32
    userenv
    ws2_32
)

target_link_libraries(aiwos_native_hello
    PRIVATE
        "${RUST_STATICLIB}"
        aiwos_hal_native
        ${RUST_WIN_DEPS}
)
