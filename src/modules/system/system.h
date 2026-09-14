#ifndef MODULES_SYSTEM_H
#define MODULES_SYSTEM_H

#include <stdint.h>

uint32_t system_uptime_ms(void);
uint32_t system_core_clock_hz(void);
const char *system_build_time(void);

#endif
