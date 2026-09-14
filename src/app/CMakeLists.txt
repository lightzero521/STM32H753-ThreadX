add_executable(${TARGET_NAME}.elf)

set(APP_SOURCE bringup.c)
message(STATUS "APP_SOURCE: ${APP_SOURCE}")

target_sources(${TARGET_NAME}.elf PRIVATE main.c ${APP_SOURCE})

target_link_libraries(${TARGET_NAME}.elf PRIVATE
    module_console
    module_system
    module_runtime
    module_led
    module_power
    module_web
    threadx
    netxduo
    platform_bsp
    platform_netx
    platform_threadx
    platform_irq
    platform_cmsis
    platform_hal
    platform_link
)

target_include_directories(${TARGET_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${TARGET_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FIRMWARE_OUTPUT_DIR}"
)

target_link_options(${TARGET_NAME}.elf PRIVATE
    "-Wl,--wrap=_malloc_r"
    "-Wl,--wrap=_free_r"
    "-Wl,--wrap=_calloc_r"
    "-Wl,--wrap=_realloc_r"
)

add_custom_command(TARGET ${TARGET_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${TARGET_NAME}.elf>
            ${FIRMWARE_OUTPUT_DIR}/${TARGET_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${TARGET_NAME}.elf>
            ${FIRMWARE_OUTPUT_DIR}/${TARGET_NAME}.hex
    COMMAND ${CMAKE_SIZE} --format=berkeley
            $<TARGET_FILE:${TARGET_NAME}.elf>
    COMMENT "Firmware outputs -> ${FIRMWARE_OUTPUT_DIR}"
    VERBATIM
)
