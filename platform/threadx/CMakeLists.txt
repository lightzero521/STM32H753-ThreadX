# --- ThreadX initialization (PRIVATE) ---
add_library(platform_threadx OBJECT tx_initialize_low_level.S)
target_link_libraries(platform_threadx PRIVATE threadx platform_device_iface)
set_source_files_properties(tx_initialize_low_level.S PROPERTIES LANGUAGE ASM)
