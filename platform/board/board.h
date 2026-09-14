#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

extern ETH_HandleTypeDef heth;

int board_clock_init(void);
void board_tick_use_threadx(void);

int board_console_open(bool dma);
int board_console_write(const uint8_t *data, uint32_t size);

int board_led_open(void);
void board_led_on(unsigned id);
void board_led_off(unsigned id);
void board_led1_toggle(void);

int board_i2c_open(void);
int board_i2c_write(uint8_t addr7, const uint8_t *data, uint32_t size, uint32_t timeout_ms);
int board_i2c_write_read(uint8_t addr7, const uint8_t *wdata, uint32_t wsize, uint8_t *rdata, uint32_t rsize,
                         uint32_t timeout_ms);

void MX_ETH_Init(void);

#endif
