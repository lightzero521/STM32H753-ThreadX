#include "board.h"
#include "board_config.h"
#include "modules/console/console.h"
#include "modules/led/led.h"
#include "modules/power/power.h"
#include "modules/runtime/heap.h"
#include "modules/system/system.h"
#include "modules/web/web.h"
#include "tx_api.h"

#define APP_HEAP_SIZE 16384U

static UCHAR app_heap[APP_HEAP_SIZE];

void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    board_tick_use_threadx();
    (void)heap_port_init(app_heap, sizeof(app_heap));
    (void)board_console_open(false);
    (void)console_init(board_console_write);

    (void)console_puts("STM32H753 ThreadX bring-up");
    (void)console_print("board NUCLEO-H753ZI, sysclk %u Hz, USART3 PD8/PD9 %u baud, LD1 2 Hz, I-cache on/D-cache off, no MPU\r\n",
                        system_core_clock_hz(), BOARD_CONSOLE_BAUD);
    (void)console_print("build %s\r\n", system_build_time());
    (void)console_puts("pwr I2C1 PB8/PB9 SHP8808 0x6C, eth RMII LAN8742 + NetX Duo HTTP :80");

    if (led_module_start() != 0)
        (void)console_puts("led module start failed");
    if (power_module_start() != 0)
        (void)console_puts("power module start failed");
    if (web_module_start() != 0)
        (void)console_puts("web module start failed");
}
