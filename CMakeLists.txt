cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug|Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

set(CMAKE_C_FLAGS_DEBUG "-Og -g -ggdb3" CACHE STRING "C compiler debug flags" FORCE)
set(CMAKE_ASM_FLAGS_DEBUG "-g -ggdb3" CACHE STRING "ASM compiler debug flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Os" CACHE STRING "C compiler release flags" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "ASM compiler release flags" FORCE)

project(bringup_STM32H753 C ASM)

set(TARGET_NAME ${PROJECT_NAME})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(FIRMWARE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/output" CACHE PATH "Firmware output directory")
file(MAKE_DIRECTORY "${FIRMWARE_OUTPUT_DIR}")

set(THREADX_ARCH "cortex_m7")
set(THREADX_TOOLCHAIN "gnu")
set(TX_USER_FILE ${CMAKE_CURRENT_LIST_DIR}/platform/threadx/tx_user.h)
set(NX_USER_FILE ${CMAKE_CURRENT_LIST_DIR}/platform/netxduo/nx_user.h)
set(NXD_ENABLE_FILE_SERVERS OFF CACHE BOOL "" FORCE)

add_subdirectory(platform)
add_subdirectory(middleware)
add_subdirectory(src/modules)
add_subdirectory(src/app)

target_compile_options(${TARGET_NAME}.elf PRIVATE
    -Wall
    -Wextra
)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    get_filename_component(_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(_compiler_name "${CMAKE_C_COMPILER}" NAME_WE)
    string(REGEX REPLACE "-[^-]+$" "" _compiler_prefix "${_compiler_name}")
    string(REPLACE "\\" "/" CLANGD_QUERY_DRIVER_GLOB "${_compiler_dir}/${_compiler_prefix}-*")

    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.vscode")
    configure_file(
        "${CMAKE_SOURCE_DIR}/.vscode/settings.json.in"
        "${CMAKE_SOURCE_DIR}/.vscode/settings.json"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/.vscode/launch.json.in"
        "${CMAKE_SOURCE_DIR}/.vscode/launch.json"
        @ONLY
    )

    message(STATUS "Generated .vscode/settings.json (QueryDriver): ${CLANGD_QUERY_DRIVER_GLOB}")
endif()
