add_library(module_power STATIC
    power.c
    bq25756/bq25756.c
    shp8808/shp8808.c
)
target_include_directories(module_power PUBLIC ${MODULES_INCLUDE_DIR})
target_link_libraries(module_power PRIVATE threadx platform_bsp module_console module_system)
