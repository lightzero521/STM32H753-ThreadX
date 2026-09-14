add_library(module_led STATIC
    led.c
)
target_include_directories(module_led PUBLIC ${MODULES_INCLUDE_DIR})
target_link_libraries(module_led PRIVATE threadx platform_bsp module_console)
