add_library(module_system STATIC
    system.c
    system_clock.c
)
target_include_directories(module_system PUBLIC ${MODULES_INCLUDE_DIR})
target_link_libraries(module_system PRIVATE threadx platform_device_iface)
