cmake_minimum_required(VERSION 3.20)
project(ai_web_os_hello C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(AI_WEB_OS_HAL_NATIVE "Build native HAL implementation" ON)

add_subdirectory(core)
add_subdirectory(hal/native)
add_subdirectory(apps/native-hello)

