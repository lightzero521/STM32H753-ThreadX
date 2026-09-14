#include "modules/system/system.h"

#include "stm32h7xx.h"
#include "tx_api.h"

uint32_t system_uptime_ms(void)
{
    return (uint32_t)(((uint64_t)tx_time_get() * 1000ULL) / TX_TIMER_TICKS_PER_SECOND);
}

uint32_t system_core_clock_hz(void)
{
    SystemCoreClockUpdate();
    return SystemCoreClock;
}

const char *system_build_time(void)
{
    return __DATE__ " " __TIME__;
}
