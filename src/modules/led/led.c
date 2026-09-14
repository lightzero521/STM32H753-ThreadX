#include "modules/led/led.h"

#include "board.h"
#include "board_config.h"
#include "modules/console/console.h"
#include "tx_api.h"

#define LED_THREAD_STACK_SIZE 512U
#define LED_THREAD_PRIORITY 12U
#define LED_HALF_PERIOD_MS (1000U / (BOARD_LED_BLINK_HZ * 2U))

static TX_THREAD led_thread;
static ULONG led_thread_stack[LED_THREAD_STACK_SIZE / sizeof(ULONG)];

static void led_thread_entry(ULONG arg)
{
    (void)arg;
    for (;;) {
        board_led1_toggle();
        tx_thread_sleep((ULONG)LED_HALF_PERIOD_MS);
    }
}

int led_module_start(void)
{
    if (board_led_open() != 0) {
        (void)console_puts("led: gpio open failed");
        return -1;
    }
    if (tx_thread_create(&led_thread, "led", led_thread_entry, 0, led_thread_stack, sizeof(led_thread_stack),
                         LED_THREAD_PRIORITY, LED_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
        return -1;
    return 0;
}
