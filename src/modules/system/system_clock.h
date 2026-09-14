#ifndef MODULES_SYSTEM_CLOCK_H
#define MODULES_SYSTEM_CLOCK_H

#include <stdint.h>

int system_clock_init(void);
int64_t system_clock_unix(void);

#endif
