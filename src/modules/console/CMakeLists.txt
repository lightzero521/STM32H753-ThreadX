add_library(module_console STATIC
    console.c
)
target_include_directories(module_console PUBLIC ${MODULES_INCLUDE_DIR})
target_link_libraries(module_console PRIVATE threadx)
