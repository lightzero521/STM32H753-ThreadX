#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 * NUCLEO-H753ZI (MB1364)
 *
 * Clock: HSE bypass 8 MHz from ST-LINK MCO, PLL SYSCLK 480 MHz, AXI/AHB 240 MHz.
 * Cache: I-Cache on, D-Cache off, no MPU (DMA stays coherent without D-Cache).
 * Console: USART3 PD8/PD9 AF7, ST-LINK VCP, 460800 baud.
 * LEDs: LD1 PB0 green, LD2 PE1 yellow, LD3 PB14 red, push-pull active high.
 * Charger: I2C1 PB8/PB9 AF4 (Arduino D15/D14), BQ25756 7-bit 0x6B, fly-wire + pull-ups.
 * ETH: LAN8742A RMII.
 */

#define BOARD_SYSCLK_HZ 480000000U
#define BOARD_CONSOLE_BAUD 460800U

#define BOARD_LED_BLINK_HZ 2U

#define BOARD_CHARGER_I2C_HZ 100000U
#define BOARD_CHARGER_SENSE_MOHM 5U
#define BOARD_CHARGER_SAMPLE_HZ 5U

#define BOARD_NET_STATIC_IP 0xC0A80150UL /* 192.168.1.80 */
#define BOARD_NET_STATIC_MASK 0xFFFFFF00UL
#define BOARD_NET_STATIC_GW 0xC0A80101UL
#define BOARD_NET_DHCP_WAIT_MS 8000U

#endif
