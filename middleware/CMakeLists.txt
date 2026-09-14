add_subdirectory(threadx)
add_subdirectory(netxduo)

add_library(platform_netx STATIC
    ${CMAKE_SOURCE_DIR}/platform/netxduo/ethernet/nx_stm32_eth_driver.c
    ${CMAKE_SOURCE_DIR}/platform/netxduo/ethernet/nx_stm32_phy_driver.c
    ${CMAKE_SOURCE_DIR}/platform/drivers/BSP/lan8742/lan8742.c
)
target_include_directories(platform_netx PUBLIC
    ${CMAKE_SOURCE_DIR}/platform/netxduo
    ${CMAKE_SOURCE_DIR}/platform/netxduo/ethernet
    ${CMAKE_SOURCE_DIR}/platform/drivers/BSP/lan8742
)
target_link_libraries(platform_netx PUBLIC netxduo platform_bsp)
target_link_libraries(platform_netx PRIVATE platform_device_iface)
