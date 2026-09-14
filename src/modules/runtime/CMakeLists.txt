add_library(module_runtime OBJECT
    syscalls.c
    heap.c
)
target_include_directories(module_runtime PUBLIC ${MODULES_INCLUDE_DIR})
target_link_libraries(module_runtime PRIVATE threadx module_system)
