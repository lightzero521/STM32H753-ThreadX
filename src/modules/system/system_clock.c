#include "modules/system/system_clock.h"

#include <sys/time.h>
#include "tx_api.h"

int system_clock_init(void)
{
    return 0;
}

int64_t system_clock_unix(void)
{
    return (int64_t)(((uint64_t)tx_time_get()) / TX_TIMER_TICKS_PER_SECOND);
}

int syscalls_gettimeofday_port(struct timeval *tv, struct timezone *tz)
{
    (void)tz;
    if (tv == 0)
        return -1;
    tv->tv_sec = (time_t)system_clock_unix();
    tv->tv_usec = 0;
    return 0;
}
