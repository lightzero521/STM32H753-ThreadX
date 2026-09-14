#ifndef NX_STM32_ETH_CONFIG_H
#define NX_STM32_ETH_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "lan8742.h"
#include "board.h"

#define NX_DRIVER_ETH_HW_IP_INIT

extern ETH_HandleTypeDef heth;
#define eth_handle heth
#define nx_eth_init MX_ETH_Init

#ifdef __cplusplus
}
#endif

#endif
