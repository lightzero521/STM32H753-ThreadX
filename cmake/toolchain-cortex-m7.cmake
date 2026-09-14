# Cortex-M7 with FPv5-SP (STM32H753 / mcu-env cortex-m7)

set(CMAKE_SYSTEM_PROCESSOR cortex-m7)
set(MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")

include(${CMAKE_CURRENT_LIST_DIR}/toolchain-base-arm.cmake)
