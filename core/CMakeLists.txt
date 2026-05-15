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

# ============ 测试目标 ============

# 测试框架库
add_library(test_framework STATIC
    tests/test_framework.c
)

target_include_directories(test_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

# Scheduler 测试可执行文件
add_executable(test_scheduler
    tests/test_scheduler.c
)

target_link_libraries(test_scheduler
    PRIVATE
        test_framework
        aiwos_core
)

# 添加测试到 CTest
enable_testing()
add_test(NAME test_scheduler COMMAND test_scheduler)

