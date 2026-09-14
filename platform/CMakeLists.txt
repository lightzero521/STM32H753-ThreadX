set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(PLATFORM_STARTUP ${PLATFORM_DIR}/drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h753xx.s)
set(PLATFORM_SYSTEM ${PLATFORM_DIR}/drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/system_stm32h7xx.c)
set(PLATFORM_LD ${PLATFORM_DIR}/linker/stm32h753zit6_flash.ld)
set(PLATFORM_CMSIS_CORE_DIR ${PLATFORM_DIR}/drivers/CMSIS)
set(PLATFORM_CMSIS_DEVICE_DIR ${PLATFORM_DIR}/drivers/CMSIS/Device/ST/STM32H7xx)
set(PLATFORM_HAL_DIR ${PLATFORM_DIR}/drivers/STM32H7xx_HAL_Driver)

if(NOT EXISTS ${PLATFORM_STARTUP})
    message(FATAL_ERROR "Startup file not found: ${PLATFORM_STARTUP}")
endif()
if(NOT EXISTS ${PLATFORM_LD})
    message(FATAL_ERROR "Linker script not found: ${PLATFORM_LD}")
endif()

add_library(platform_device_iface INTERFACE)
target_include_directories(platform_device_iface INTERFACE
    ${PLATFORM_CMSIS_CORE_DIR}
    ${PLATFORM_CMSIS_DEVICE_DIR}/Include
    ${PLATFORM_HAL_DIR}/Inc
    ${PLATFORM_DIR}/board
    ${PLATFORM_DIR}/irq
    ${PLATFORM_DIR}/netxduo
    ${PLATFORM_DIR}/netxduo/ethernet
    ${PLATFORM_DIR}/drivers/BSP/lan8742
)
target_compile_definitions(platform_device_iface INTERFACE
    STM32H753xx
    USE_HAL_DRIVER
)

add_library(platform_cmsis OBJECT
    ${PLATFORM_SYSTEM}
    ${PLATFORM_STARTUP}
)
target_link_libraries(platform_cmsis PRIVATE platform_device_iface)
set_source_files_properties(${PLATFORM_STARTUP} PROPERTIES LANGUAGE ASM)

add_library(platform_hal OBJECT
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_cortex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_rcc.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_pwr.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_gpio.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_dma.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_dma_ex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_exti.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_flash.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_uart.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_uart_ex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_i2c.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_i2c_ex.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_eth.c
    ${PLATFORM_HAL_DIR}/Src/stm32h7xx_hal_eth_ex.c
)
target_link_libraries(platform_hal PRIVATE platform_device_iface)

add_library(platform_irq OBJECT irq/stm32h7xx_it.c)
target_link_libraries(platform_irq PRIVATE platform_device_iface)

add_library(platform_bsp STATIC
    board/board.c
    board/board_eth.c
)
target_include_directories(platform_bsp PUBLIC
    ${PLATFORM_DIR}/board
)
target_link_libraries(platform_bsp PUBLIC platform_device_iface)
target_link_libraries(platform_bsp PRIVATE platform_hal threadx)

add_library(platform_link INTERFACE)
target_link_options(platform_link INTERFACE
    -T${PLATFORM_LD}
    -nostartfiles
    -Wl,-Map=${FIRMWARE_OUTPUT_DIR}/${TARGET_NAME}.map
)

add_subdirectory(threadx)
